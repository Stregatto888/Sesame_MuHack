#pragma once

// ============================================================================
// ROS BRIDGE — micro-ROS sulla USB nativa (fase 5)
// ============================================================================
//
// USB nativa con 2 porte CDC (esp_tinyusb):
//   ttyACM0 — trasporto micro-ROS verso l'agente sul PC
//   ttyACM1 — log ESP_LOGx (idf.py monitor --no-reset)
//
// Nodo "sesame_esp32" (ROS_DOMAIN_ID 0), QoS best-effort:
//   /sesame/joint_commands  (sottoscrizione) std_msgs/Float32MultiArray
//   /sesame/joint_echo      (pubblicazione)  std_msgs/Float32MultiArray, 50 Hz
// 8 valori in ordine firmware R1, R2, L1, L2, R4, R3, L3, L4, in gradi del
// servo (conversione da gradi URDF con direzione e subtrim fatta dal plugin).
//
// Sicurezze nel firmware: limiti per giunto, passo massimo 12° ogni 20 ms,
// watchdog 250 ms senza comandi → fermo nella posa corrente.
// ============================================================================

namespace RosBridge
{
  /**
   * @brief Avvia il log su USB CDC, imposta il trasporto micro-ROS e crea TaskRos.
   *
   * Da chiamare una sola volta alla fine di setup(), dopo Tasks::startAll().
   * Non blocca: se l'agente non c'è, TaskRos continua a cercarlo e il resto
   * del firmware (web, display, pose) funziona come prima.
   */
  void start();
} // namespace RosBridge
