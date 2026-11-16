#pragma once

// ============================================================================
// ROS BRIDGE — micro-ROS sulla USB nativa (fase 5)
// ============================================================================
//
// USB nativa con 2 porte CDC (esp_tinyusb):
//   ttyACM0 — trasporto micro-ROS verso l'agente sul PC
//   ttyACM1 — log ESP_LOGx (idf.py monitor --no-reset)
//
// Passo 3b: nodo "sesame_esp32" con il solo publisher di prova
// /sesame/heartbeat (std_msgs/Int32, 1 Hz), riconnessione automatica
// all'agente e misura della memoria. I topic definitivi arrivano con la
// decisione 2 del passo 3.
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
