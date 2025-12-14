// LevelWebServer.h - С БИБЛИОТЕКОЙ WebSockets (Links2004)
#ifndef LEVEL_WEB_SERVER_H
#define LEVEL_WEB_SERVER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <WebServer.h>
#include <WebSocketsServer.h>

#include "ConfigManager.h"
#include "FileManager.h"
#include "SensorManager.h"

class LevelWebServer {
 public:
  LevelWebServer(SensorManager& sensorMgr);

  /**
   * @brief Запуск серверов (HTTP + WebSocket)
   */
  void begin();

  /**
   * @brief Отправка данных всем WebSocket клиентам
   */
  void broadcastSensorData();

  /**
   * @brief Обработка WebSocket событий (вызывать из loop())
   */
  void handleClients();

  /**
   * @brief Получить количество подключённых WebSocket клиентов
   */
  uint8_t getClientCount() const { return wsClientCount; }

  /**
   * @brief Включить/выключить подробное логирование
   */
  void setDebugMode(bool enabled) { wsDebugEnabled = enabled; }

 private:
  WebServer httpServer;
  WebSocketsServer wsServer;

  SensorManager& sensorManager;
  FileManager fileManager;

  // WebSocket статистика
  bool wsDebugEnabled;
  unsigned long lastBroadcastTime;
  unsigned long broadcastCount;
  uint8_t wsClientCount;

  // Минимальный интервал между broadcast (мс)
  static const uint32_t BROADCAST_INTERVAL_MS = 200;  // 5 Hz

  // WebSocket обработчик событий
  static void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload,
                             size_t length);
  static LevelWebServer* instance;  // Для доступа из статического callback

  // HTTP маршруты
  void setupRoutes();

  // CORS helper
  void sendCORSHeaders();

  // Вспомогательные функции
  String getSensorDataJson();
};

#endif  // LEVEL_WEB_SERVER_H