// LevelWebServer.h
// Отвечает за: HTTP сервер, WebSocket, CORS, маршруты, настройки

#ifndef LEVEL_WEB_SERVER_H
#define LEVEL_WEB_SERVER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <AsyncTCP.h>
#include <AsyncWebSocket.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>

#include "ConfigManager.h"
#include "FileManager.h"
#include "SensorManager.h"

class LevelWebServer {
 public:
  LevelWebServer(SensorManager& sensorMgr);

  /**
   * @brief Запуск сервера (работает и в AP, и в STA режиме)
   */
  void begin();

  /**
   * @brief Отправка данных всем WebSocket клиентам
   * Автоматически применяет offset и axis swap
   */
  void broadcastSensorData();

  /**
   * @brief Очистка мёртвых соединений (вызывать из loop())
   */
  void handleClients();

  /**
   * @brief Получить количество подключённых WebSocket клиентов
   */
  uint8_t getClientCount() const { return ws.count(); }

  /**
   * @brief Включить/выключить подробное логирование WebSocket
   */
  void setDebugMode(bool enabled) { wsDebugEnabled = enabled; }

 private:
  AsyncWebServer server;
  AsyncWebSocket ws;

  SensorManager& sensorManager;

  // WebSocket отладка и статистика (должны быть до fileManager)
  bool wsDebugEnabled;
  unsigned long lastBroadcastTime;
  unsigned long broadcastCount;

  FileManager fileManager;

  // Настройка маршрутов
  void setupRoutes();
  void setupWebSocket();
  void setupCORS();

  // Обработчики WebSocket событий
  void handleWebSocketMessage(AsyncWebSocketClient* client, uint8_t* data,
                              size_t len);

  // CORS helper
  void addCORSHeaders(AsyncWebServerResponse* response);

  // Вспомогательные функции
  String getSensorDataJson();
};

#endif  // LEVEL_WEB_SERVER_H