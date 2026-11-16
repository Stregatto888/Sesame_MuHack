// ============================================================================
// ros/ros_bridge.cpp — micro-ROS sulla USB nativa (fase 5, passo 4)
//
//  - ttyACM0: trasporto micro-ROS. TinyUSB è installato una sola volta (dal
//    log) e la porta CDC resta aperta tra una sessione e l'altra.
//  - ttyACM1: log ESP_LOGx
//  - TaskRos, ciclo a 50 Hz:
//      * comandi da /sesame/joint_commands → limiti per giunto (URDF ± margine
//        del subtrim, dentro 0-180) → passo
//        massimo 12° per ciclo → Motors::writeDegrees()
//      * watchdog 250 ms senza comandi validi → fermo nella posa corrente
//      * eco degli angoli scritti su /sesame/joint_echo
//      * agente perso (3 ping falliti) → entità distrutte, lo ricerca;
//        i servo restano fermi grazie al watchdog
//  - ogni 5 s scrive su ttyACM1 modalità, heap interno, PSRAM e stack libero
// ============================================================================

#include "ros/ros_bridge.h"
#include "core/config.h"
#include "core/mode.h"
#include "motors/servo_driver.h"

#include <algorithm>
#include <cmath>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_usbcdc_logging.h"
#include "esp_usbcdc_transport.h"

#include <rcl/error_handling.h>
#include <rcl/rcl.h>
#include <rclc/executor.h>
#include <rclc/rclc.h>
#include <rmw_microros/rmw_microros.h>
#include <rmw_microxrcedds_c/config.h>
#include <std_msgs/msg/float32_multi_array.h>

#if !defined(RMW_UXRCE_TRANSPORT_CUSTOM)
#error "micro-ROS: serve RMW_UXRCE_TRANSPORT=custom (vedi app-colcon.meta)"
#endif

namespace
{

// ---------------------------------------------------------------------------
// Configurazione
// ---------------------------------------------------------------------------
constexpr const char *TAG           = "ros_bridge";
constexpr size_t      ROS_DOMAIN_ID = 0;
constexpr const char *NODE_NAME     = "sesame_esp32";
constexpr const char *COMMAND_TOPIC = "/sesame/joint_commands";
constexpr const char *ECHO_TOPIC    = "/sesame/joint_echo";

constexpr uint32_t    TASK_STACK_BYTES       = 10240;  ///< Misurati ~3 KB usati al passo 3b
constexpr UBaseType_t TASK_PRIORITY          = 3;      ///< Sopra TaskMotor (2), sotto TinyUSB (5)
constexpr uint32_t    WAIT_PERIOD_MS         = 500;    ///< Ricerca dell'agente
constexpr uint32_t    PING_PERIOD_MS         = 1000;   ///< Controllo dell'agente da collegato
constexpr int         PING_FAILURES_FOR_LOSS = 3;
constexpr uint32_t    MEM_LOG_PERIOD_MS      = 5000;
constexpr uint32_t    WARN_PERIOD_MS         = 2000;   ///< Avvisi ripetuti al massimo ogni 2 s

// ---------------------------------------------------------------------------
// Trasporto USB CDC (ttyACM0): apre la porta una volta sola
// ---------------------------------------------------------------------------
tinyusb_cdcacm_itf_t cdcPort  = TINYUSB_CDC_ACM_0;
bool                 cdcReady = false;

bool transportOpen(struct uxrCustomTransport *transport)
{
  if (cdcReady)
    return true;
  tinyusb_config_cdcacm_t cfg = {};
  cfg.usb_dev          = TINYUSB_USBDEV_0;
  cfg.cdc_port         = *static_cast<tinyusb_cdcacm_itf_t *>(transport->args);
  cfg.rx_unread_buf_sz = CONFIG_TINYUSB_CDC_RX_BUFSIZE;
  if (tusb_cdc_acm_init(&cfg) != ESP_OK)
    return false;
  cdcReady = true;
  return true;
}

bool transportClose(struct uxrCustomTransport * /*transport*/)
{
  return true; // la porta resta aperta: niente reinstallazioni a ogni tentativo
}

// ---------------------------------------------------------------------------
// Stato micro-ROS e controllo (usato solo da TaskRos)
// ---------------------------------------------------------------------------
rcl_allocator_t   allocator;
rclc_support_t    support;
rcl_node_t        node;
rcl_subscription_t commandSub;
rcl_publisher_t   echoPub;
rclc_executor_t   executor;
bool              supportReady = false;

float commandBuffer[SERVO_COUNT];
float echoBuffer[SERVO_COUNT];
std_msgs__msg__Float32MultiArray commandMsg;
std_msgs__msg__Float32MultiArray echoMsg;

float      target[SERVO_COUNT];
bool       haveTarget    = false;
bool       holding       = true;
TickType_t lastCommand   = 0;
TickType_t lastWarn      = 0;
uint32_t   commandsTotal = 0;
uint32_t   commandsBad   = 0;
uint32_t   clampedTotal  = 0;

bool warnAllowed(TickType_t now)
{
  if (now - lastWarn < pdMS_TO_TICKS(WARN_PERIOD_MS))
    return false;
  lastWarn = now;
  return true;
}

void initMessages()
{
  commandMsg.data.data          = commandBuffer;
  commandMsg.data.capacity      = SERVO_COUNT;
  commandMsg.data.size          = 0;
  commandMsg.layout.dim.data    = nullptr;
  commandMsg.layout.dim.capacity = 0;
  commandMsg.layout.dim.size    = 0;
  commandMsg.layout.data_offset = 0;

  echoMsg.data.data          = echoBuffer;
  echoMsg.data.capacity      = SERVO_COUNT;
  echoMsg.data.size          = SERVO_COUNT;
  echoMsg.layout.dim.data    = nullptr;
  echoMsg.layout.dim.capacity = 0;
  echoMsg.layout.dim.size    = 0;
  echoMsg.layout.data_offset = 0;
}

/// Comando ricevuto: 8 valori finiti, limitati ai limiti del giunto.
void commandCallback(const void *msgIn)
{
  const auto *msg      = static_cast<const std_msgs__msg__Float32MultiArray *>(msgIn);
  const TickType_t now = xTaskGetTickCount();
  commandsTotal++;

  if (msg->data.size != SERVO_COUNT)
  {
    commandsBad++;
    if (warnAllowed(now))
      ESP_LOGW(TAG, "comando con %u valori ignorato (ne servono %d)", (unsigned)msg->data.size, SERVO_COUNT);
    return;
  }
  for (int i = 0; i < SERVO_COUNT; i++)
  {
    if (!std::isfinite(msg->data.data[i]))
    {
      commandsBad++;
      if (warnAllowed(now))
        ESP_LOGW(TAG, "comando con valori non finiti ignorato");
      return;
    }
  }

  bool clamped = false;
  for (int i = 0; i < SERVO_COUNT; i++)
  {
    // Limiti URDF allargati del margine del subtrim, poi campo del servo.
    const float low  = std::max(JOINT_LIMIT_MIN_DEG[i] - ROS_LIMIT_MARGIN_DEG, SERVO_RANGE_MIN_DEG);
    const float high = std::min(JOINT_LIMIT_MAX_DEG[i] + ROS_LIMIT_MARGIN_DEG, SERVO_RANGE_MAX_DEG);
    float value      = msg->data.data[i];
    if (value < low) { value = low; clamped = true; }
    if (value > high) { value = high; clamped = true; }
    target[i] = value;
  }
  if (clamped)
  {
    clampedTotal++;
    if (warnAllowed(now))
      ESP_LOGW(TAG, "comando fuori dai limiti: valori limitati");
  }

  haveTarget  = true;
  lastCommand = now;
  Mode::noteRosCommand();
}

/// Un ciclo di controllo (50 Hz): watchdog, poi passo limitato verso il bersaglio.
void controlTick()
{
  if (!Mode::isRos() || !haveTarget)
    return;

  // Ora letta qui, dopo l'executor: lastCommand può essere più recente dell'inizio del ciclo.
  // Differenza con segno: un comando "nel futuro" di 1 ms non deve sembrare vecchio.
  const int32_t sinceCommand = (int32_t)(xTaskGetTickCount() - lastCommand);
  if (sinceCommand > (int32_t)pdMS_TO_TICKS(ROS_COMMAND_TIMEOUT_MS))
  {
    if (!holding)
    {
      holding = true;
      ESP_LOGW(TAG, "watchdog: nessun comando da %d ms, fermo nella posa corrente", ROS_COMMAND_TIMEOUT_MS);
    }
    return;
  }
  if (holding)
  {
    holding = false;
    ESP_LOGI(TAG, "comandi ROS in arrivo: modalità ROS, servo in movimento");
  }

  for (int i = 0; i < SERVO_COUNT; i++)
  {
    const float current = Motors::lastAngle(i);
    float step          = target[i] - current;
    if (step > ROS_MAX_STEP_DEG)
      step = ROS_MAX_STEP_DEG;
    if (step < -ROS_MAX_STEP_DEG)
      step = -ROS_MAX_STEP_DEG;
    if (step != 0.0f)
      Motors::writeDegrees(i, current + step);
  }
}

void publishEcho()
{
  for (int i = 0; i < SERVO_COUNT; i++)
    echoBuffer[i] = Motors::lastAngle(i);
  if (rcl_publish(&echoPub, &echoMsg, nullptr) != RCL_RET_OK)
    rcl_reset_error();
}

/// Distrugge le entità; con l'agente perso non aspetta le sue risposte.
void destroyEntities()
{
  if (supportReady)
  {
    rmw_context_t *rmwContext = rcl_context_get_rmw_context(&support.context);
    (void)rmw_uros_set_context_entity_destroy_session_timeout(rmwContext, 0);
  }
  (void)rcl_subscription_fini(&commandSub, &node);
  (void)rcl_publisher_fini(&echoPub, &node);
  (void)rclc_executor_fini(&executor);
  (void)rcl_node_fini(&node);
  if (supportReady)
    (void)rclc_support_fini(&support);
  supportReady = false;
  rcl_reset_error();
}

/// Crea sessione, nodo, sottoscrizione, publisher ed executor. false se un passo fallisce.
bool createEntities()
{
  allocator  = rcl_get_default_allocator();
  node       = rcl_get_zero_initialized_node();
  commandSub = rcl_get_zero_initialized_subscription();
  echoPub    = rcl_get_zero_initialized_publisher();
  executor   = rclc_executor_get_zero_initialized_executor();

  rcl_init_options_t initOptions = rcl_get_zero_initialized_init_options();
  if (rcl_init_options_init(&initOptions, allocator) != RCL_RET_OK)
    return false;

  // rcl_init copia le opzioni: si liberano subito (come fa rclc_support_init).
  bool ok = rcl_init_options_set_domain_id(&initOptions, ROS_DOMAIN_ID) == RCL_RET_OK &&
            rclc_support_init_with_options(&support, 0, nullptr, &initOptions, &allocator) == RCL_RET_OK;
  (void)rcl_init_options_fini(&initOptions);
  if (!ok)
    return false;
  supportReady = true;

  const rosidl_message_type_support_t *type =
      ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Float32MultiArray);

  return rclc_node_init_default(&node, NODE_NAME, "", &support) == RCL_RET_OK &&
         rclc_subscription_init_best_effort(&commandSub, &node, type, COMMAND_TOPIC) == RCL_RET_OK &&
         rclc_publisher_init_best_effort(&echoPub, &node, type, ECHO_TOPIC) == RCL_RET_OK &&
         rclc_executor_init(&executor, &support.context, 1, &allocator) == RCL_RET_OK &&
         rclc_executor_add_subscription(&executor, &commandSub, &commandMsg,
                                        &commandCallback, ON_NEW_DATA) == RCL_RET_OK;
}

void logStatus(bool connected)
{
  ESP_LOGI(TAG, "%s | modo %s%s | comandi %u (scartati %u, limitati %u) | heap interno %u (minimo %u, blocco max %u) | PSRAM %u | stack libero %u",
           connected ? "collegato" : "in attesa dell'agente",
           Mode::isRos() ? "ROS" : "WEB",
           (Mode::isRos() && holding) ? " fermo" : "",
           (unsigned)commandsTotal, (unsigned)commandsBad, (unsigned)clampedTotal,
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
           (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
           (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
           (unsigned)uxTaskGetStackHighWaterMark(nullptr));
}

void rosTask(void * /*params*/)
{
  const TickType_t period = pdMS_TO_TICKS(ROS_CONTROL_PERIOD_MS);
  bool connected          = false;
  int pingFailures        = 0;
  TickType_t start        = xTaskGetTickCount();
  TickType_t lastPing     = start;
  TickType_t lastAttempt  = start - pdMS_TO_TICKS(WAIT_PERIOD_MS);
  TickType_t lastStatus   = start - pdMS_TO_TICKS(MEM_LOG_PERIOD_MS);
  TickType_t nextTick     = start + period;

  for (;;)
  {
    const TickType_t now = xTaskGetTickCount();

    if (now - lastStatus >= pdMS_TO_TICKS(MEM_LOG_PERIOD_MS))
    {
      lastStatus = now;
      logStatus(connected);
    }

    if (connected)
    {
      (void)rclc_executor_spin_some(&executor, RCL_MS_TO_NS(1));
      controlTick();
      publishEcho();

      if (now - lastPing >= pdMS_TO_TICKS(PING_PERIOD_MS))
      {
        lastPing = now;
        if (rmw_uros_ping_agent(10, 1) == RMW_RET_OK)
          pingFailures = 0;
        else if (++pingFailures >= PING_FAILURES_FOR_LOSS)
        {
          ESP_LOGW(TAG, "agente perso: entità distrutte, lo cerco di nuovo");
          destroyEntities();
          connected = false;
        }
      }
    }
    else
    {
      controlTick(); // senza comandi il watchdog tiene il robot fermo
      if (now - lastAttempt >= pdMS_TO_TICKS(WAIT_PERIOD_MS))
      {
        lastAttempt = now;
        if (rmw_uros_ping_agent(50, 1) == RMW_RET_OK)
        {
          if (createEntities())
          {
            connected    = true;
            pingFailures = 0;
            lastPing     = xTaskGetTickCount();
            ESP_LOGI(TAG, "agente collegato: nodo %s, comandi %s, eco %s", NODE_NAME, COMMAND_TOPIC, ECHO_TOPIC);
          }
          else
          {
            ESP_LOGW(TAG, "creazione delle entità fallita, riprovo");
            destroyEntities();
          }
        }
      }
    }

    // Prossimo ciclo a 50 Hz; dopo un ritardo (ping, collegamento) si riparte da adesso.
    const TickType_t after = xTaskGetTickCount();
    if ((int32_t)(nextTick - after) > 0)
      vTaskDelay(nextTick - after);
    nextTick += period;
    if ((int32_t)(nextTick - xTaskGetTickCount()) <= 0)
      nextTick = xTaskGetTickCount() + period;
  }
}

} // namespace

void RosBridge::start()
{
  initMessages();

  // Installa TinyUSB con 2 porte CDC e manda stdout (ESP_LOGx) su ttyACM1.
  if (esp_usbcdc_logging_init() == ESP_OK)
    ESP_LOGI(TAG, "log su USB CDC (ttyACM1) attivo");

  if (rmw_uros_set_custom_transport(true, &cdcPort,
                                    transportOpen, transportClose,
                                    esp_usbcdc_write, esp_usbcdc_read) != RMW_RET_OK)
  {
    ESP_LOGE(TAG, "trasporto micro-ROS non impostato");
    return;
  }

  if (xTaskCreate(rosTask, "TaskRos", TASK_STACK_BYTES, nullptr, TASK_PRIORITY, nullptr) != pdPASS)
    ESP_LOGE(TAG, "TaskRos non creato: memoria interna insufficiente per %u byte di stack",
             (unsigned)TASK_STACK_BYTES);
}
