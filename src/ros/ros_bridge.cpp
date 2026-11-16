// ============================================================================
// ros/ros_bridge.cpp — micro-ROS sulla USB nativa (fase 5, passo 3b)
//
//  - ttyACM0: trasporto micro-ROS (componenti esp_usbcdc_* dell'esempio
//    ufficiale, nel sottomodulo micro_ros_espidf_component)
//  - ttyACM1: log ESP_LOGx
//  - TaskRos: cerca l'agente ogni 500 ms; collegato, fa girare l'executor e
//    controlla l'agente ogni secondo; se lo perde distrugge le entità e torna
//    ad aspettare, senza fermare il resto del firmware
//  - ogni 5 s scrive su ttyACM1 heap interno, PSRAM e stack libero di TaskRos
// ============================================================================

#include "ros/ros_bridge.h"

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
#include <std_msgs/msg/int32.h>

#if !defined(RMW_UXRCE_TRANSPORT_CUSTOM)
#error "micro-ROS: serve RMW_UXRCE_TRANSPORT=custom (vedi app-colcon.meta)"
#endif

namespace
{

// ---------------------------------------------------------------------------
// Configurazione
// ---------------------------------------------------------------------------
constexpr const char *TAG             = "ros_bridge";
constexpr size_t      ROS_DOMAIN_ID   = 0;                    ///< ROS_DOMAIN_ID predefinito
constexpr const char *NODE_NAME       = "sesame_esp32";
constexpr const char *HEARTBEAT_TOPIC = "/sesame/heartbeat";  ///< Solo per il passo 3b

constexpr uint32_t    TASK_STACK_BYTES  = 16000;  ///< Come l'esempio ufficiale; si misura
constexpr UBaseType_t TASK_PRIORITY     = 3;      ///< Sopra TaskMotor (2), sotto TinyUSB (5)
constexpr uint32_t    WAIT_PERIOD_MS    = 500;    ///< Ricerca dell'agente
constexpr uint32_t    PING_PERIOD_MS    = 1000;   ///< Controllo dell'agente da collegato
constexpr uint32_t    MEM_LOG_PERIOD_MS = 5000;   ///< Log della memoria

// ---------------------------------------------------------------------------
// Stato micro-ROS (usato solo da TaskRos)
// ---------------------------------------------------------------------------
tinyusb_cdcacm_itf_t cdcPort = TINYUSB_CDC_ACM_0;

rcl_allocator_t      allocator;
rclc_support_t       support;
rcl_node_t           node;
rcl_publisher_t      heartbeatPub;
rcl_timer_t          heartbeatTimer;
rclc_executor_t      executor;
std_msgs__msg__Int32 heartbeatMsg;
bool                 supportReady = false;

void heartbeatCallback(rcl_timer_t *timer, int64_t /*lastCallTime*/)
{
  if (timer == nullptr)
    return;
  if (rcl_publish(&heartbeatPub, &heartbeatMsg, nullptr) != RCL_RET_OK)
  {
    ESP_LOGW(TAG, "publish fallito");
    rcl_reset_error();
  }
  heartbeatMsg.data++;
}

/// Distrugge le entità; con l'agente perso non aspetta le sue risposte.
void destroyEntities()
{
  if (supportReady)
  {
    rmw_context_t *rmwContext = rcl_context_get_rmw_context(&support.context);
    (void)rmw_uros_set_context_entity_destroy_session_timeout(rmwContext, 0);
  }
  (void)rcl_publisher_fini(&heartbeatPub, &node);
  (void)rcl_timer_fini(&heartbeatTimer);
  (void)rclc_executor_fini(&executor);
  (void)rcl_node_fini(&node);
  if (supportReady)
    (void)rclc_support_fini(&support);
  supportReady = false;
  rcl_reset_error();
}

/// Crea sessione, nodo, publisher, timer ed executor. false se un passo fallisce.
bool createEntities()
{
  allocator      = rcl_get_default_allocator();
  node           = rcl_get_zero_initialized_node();
  heartbeatPub   = rcl_get_zero_initialized_publisher();
  heartbeatTimer = rcl_get_zero_initialized_timer();
  executor       = rclc_executor_get_zero_initialized_executor();

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

  return rclc_node_init_default(&node, NODE_NAME, "", &support) == RCL_RET_OK &&
         rclc_publisher_init_default(&heartbeatPub, &node,
                                     ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int32),
                                     HEARTBEAT_TOPIC) == RCL_RET_OK &&
         rclc_timer_init_default2(&heartbeatTimer, &support, RCL_MS_TO_NS(1000),
                                  heartbeatCallback, true) == RCL_RET_OK &&
         rclc_executor_init(&executor, &support.context, 1, &allocator) == RCL_RET_OK &&
         rclc_executor_add_timer(&executor, &heartbeatTimer) == RCL_RET_OK;
}

void logMemory(bool connected)
{
  ESP_LOGI(TAG, "%s | heartbeat %ld | heap interno libero %u (minimo %u, blocco max %u) | PSRAM libera %u | stack libero TaskRos %u",
           connected ? "collegato" : "in attesa dell'agente",
           (long)heartbeatMsg.data,
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
           (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
           (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
           (unsigned)uxTaskGetStackHighWaterMark(nullptr));
}

void rosTask(void * /*params*/)
{
  heartbeatMsg.data   = 0;
  bool connected      = false;
  TickType_t lastPing = xTaskGetTickCount();
  TickType_t lastMem  = lastPing - pdMS_TO_TICKS(MEM_LOG_PERIOD_MS);

  for (;;)
  {
    const TickType_t now = xTaskGetTickCount();
    if (now - lastMem >= pdMS_TO_TICKS(MEM_LOG_PERIOD_MS))
    {
      lastMem = now;
      logMemory(connected);
    }

    if (!connected)
    {
      if (rmw_uros_ping_agent(100, 1) == RMW_RET_OK)
      {
        if (createEntities())
        {
          connected = true;
          lastPing  = xTaskGetTickCount();
          ESP_LOGI(TAG, "agente collegato: nodo %s, topic %s", NODE_NAME, HEARTBEAT_TOPIC);
          continue;
        }
        ESP_LOGW(TAG, "creazione delle entità fallita, riprovo");
        destroyEntities();
      }
      vTaskDelay(pdMS_TO_TICKS(WAIT_PERIOD_MS));
      continue;
    }

    (void)rclc_executor_spin_some(&executor, RCL_MS_TO_NS(10));

    if (now - lastPing >= pdMS_TO_TICKS(PING_PERIOD_MS))
    {
      lastPing = now;
      if (rmw_uros_ping_agent(100, 3) != RMW_RET_OK)
      {
        ESP_LOGW(TAG, "agente perso: entità distrutte, lo cerco di nuovo");
        destroyEntities();
        connected = false;
        continue;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

} // namespace

void RosBridge::start()
{
  // Installa TinyUSB con 2 porte CDC e manda stdout (ESP_LOGx) su ttyACM1.
  if (esp_usbcdc_logging_init() == ESP_OK)
    ESP_LOGI(TAG, "log su USB CDC (ttyACM1) attivo");

  if (rmw_uros_set_custom_transport(true, &cdcPort,
                                    esp_usbcdc_open, esp_usbcdc_close,
                                    esp_usbcdc_write, esp_usbcdc_read) != RMW_RET_OK)
  {
    ESP_LOGE(TAG, "trasporto micro-ROS non impostato");
    return;
  }

  if (xTaskCreate(rosTask, "TaskRos", TASK_STACK_BYTES, nullptr, TASK_PRIORITY, nullptr) != pdPASS)
    ESP_LOGE(TAG, "TaskRos non creato: memoria interna insufficiente per %u byte di stack",
             (unsigned)TASK_STACK_BYTES);
}
