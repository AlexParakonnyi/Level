// LevelWebServer.cpp - С ИСПРАВЛЕННЫМ CORS
#include "LevelWebServer.h"

LevelWebServer::LevelWebServer(SensorManager& sensorMgr)
    : server(80),
      ws("/ws"),
      sensorManager(sensorMgr),
      wsDebugEnabled(true),
      lastBroadcastTime(0),
      broadcastCount(0) {}

void LevelWebServer::begin() {
  Serial.println("=== Initializing Web Server ===");

  setupCORS();
  setupWebSocket();
  setupRoutes();

  server.begin();
  Serial.println("Web Server started");
  Serial.printf("Free heap: %u bytes\n", ESP.getFreeHeap());
}

void LevelWebServer::setupCORS() {
  // CORS через DefaultHeaders (работает для статических файлов)
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods",
                                       "GET, POST, PUT, DELETE, OPTIONS");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers",
                                       "Content-Type, Authorization");
  DefaultHeaders::Instance().addHeader("Access-Control-Max-Age", "3600");

  Serial.println("CORS configured");
}

void LevelWebServer::setupWebSocket() {
  ws.onEvent([this](AsyncWebSocket* server, AsyncWebSocketClient* client,
                    AwsEventType type, void* arg, uint8_t* data, size_t len) {
    switch (type) {
      case WS_EVT_CONNECT: {
        Serial.printf("[WS] ✓ Client #%u CONNECTED from %s\n", client->id(),
                      client->remoteIP().toString().c_str());
        Serial.printf("[WS]   Total clients: %d\n", ws.count());
        Serial.printf("[WS]   Free heap: %u bytes\n", ESP.getFreeHeap());

        // Отправляем начальные данные новому клиенту
        String json = getSensorDataJson();
        client->text(json);
        Serial.printf("[WS]   Sent initial data to #%u (%d bytes)\n",
                      client->id(), json.length());
        break;
      }

      case WS_EVT_DISCONNECT: {
        Serial.printf("[WS] ✗ Client #%u DISCONNECTED\n", client->id());
        Serial.printf("[WS]   Total clients: %d\n", ws.count());
        Serial.printf("[WS]   Free heap: %u bytes\n", ESP.getFreeHeap());
        break;
      }

      case WS_EVT_ERROR: {
        Serial.printf("[WS] ⚠ ERROR on client #%u: %s\n", client->id(),
                      (char*)data);
        Serial.printf(
            "[WS]   Client status: %s\n",
            client->status() == WS_CONNECTED ? "CONNECTED" : "DISCONNECTED");
        break;
      }

      case WS_EVT_PONG: {
        if (wsDebugEnabled) {
          Serial.printf("[WS] PONG received from client #%u\n", client->id());
        }
        break;
      }

      case WS_EVT_DATA: {
        handleWebSocketMessage(client, data, len);
        break;
      }
    }
  });

  server.addHandler(&ws);
  Serial.println("WebSocket configured at /ws");
}

void LevelWebServer::handleWebSocketMessage(AsyncWebSocketClient* client,
                                            uint8_t* data, size_t len) {
  if (len > 0) {
    String message = String((char*)data).substring(0, len);
    Serial.printf("[WS] Message from #%u: %s\n", client->id(), message.c_str());
  }
}

void LevelWebServer::broadcastSensorData() {
  unsigned long now = millis();

  // Проверка количества клиентов
  uint8_t clientCount = ws.count();
  if (clientCount == 0) {
    if (wsDebugEnabled && (now - lastBroadcastTime > 5000)) {
      Serial.println("[WS] No clients connected, skipping broadcast");
      lastBroadcastTime = now;
    }
    return;
  }

  if (clientCount > 5) {
    Serial.printf("[WS] ⚠ WARNING: Too many clients (%d), skipping broadcast\n",
                  clientCount);
    return;
  }

  // Генерируем JSON
  String json = getSensorDataJson();
  size_t jsonSize = json.length();

  // Проверка размера сообщения
  if (jsonSize > 1024) {
    Serial.printf("[WS] ⚠ WARNING: Message too large (%d bytes)\n", jsonSize);
  }

  // Проверка памяти перед отправкой
  uint32_t freeHeap = ESP.getFreeHeap();
  if (freeHeap < 10000) {
    Serial.printf("[WS] ⚠ WARNING: Low memory (%u bytes), skipping broadcast\n",
                  freeHeap);
    return;
  }

  // Попытка отправки
  try {
    ws.textAll(json);
    broadcastCount++;

    // Подробное логирование каждые 10 секунд
    if (wsDebugEnabled && (now - lastBroadcastTime > 10000)) {
      Serial.printf("[WS] Broadcast #%lu: %d clients, %d bytes, %u heap free\n",
                    broadcastCount, clientCount, jsonSize, freeHeap);

      // Проверяем статус каждого клиента
      for (auto client : ws.getClients()) {
        Serial.printf(
            "[WS]   Client #%u: %s\n", client->id(),
            client->status() == WS_CONNECTED ? "CONNECTED" : "DISCONNECTED");
      }

      lastBroadcastTime = now;
    }

  } catch (...) {
    Serial.println("[WS] ⚠ ERROR: Exception during broadcast!");
  }
}

void LevelWebServer::handleClients() {
  unsigned long now = millis();

  // Cleanup disconnected clients
  static unsigned long lastCleanup = 0;
  if (now - lastCleanup > 5000) {
    ws.cleanupClients();
    lastCleanup = now;
  }

  // Периодический ping
  static unsigned long lastPing = 0;
  if (now - lastPing > 30000) {
    if (ws.count() > 0) {
      Serial.printf("[WS] Sending PING to %d clients...\n", ws.count());
      ws.pingAll();
    }
    lastPing = now;
  }

  // Проверка "мёртвых" клиентов по статусу
  static unsigned long lastStatusCheck = 0;
  if (now - lastStatusCheck > 15000) {
    for (auto client : ws.getClients()) {
      if (client->status() != WS_CONNECTED) {
        Serial.printf("[WS] ⚠ Removing dead client #%u (status: %d)\n",
                      client->id(), client->status());
        client->close();
      }
    }
    lastStatusCheck = now;
  }
}

String LevelWebServer::getSensorDataJson() {
  StaticJsonDocument<256> doc;

  SensorData data = sensorManager.getCachedData();

  if (!data.valid) {
    Serial.println("[WS] ⚠ WARNING: Sensor data not valid!");
  }

  float roll = data.roll;
  float pitch = data.pitch;

  // Применяем настройки из кеша ConfigManager
  roll += ConfigManager::getZeroOffset();

  if (ConfigManager::getAxisSwap()) {
    float temp = roll;
    roll = pitch;
    pitch = temp;
  }

  JsonObject accel = doc["accelerometer"].to<JsonObject>();
  accel["x"] = serialized(String(data.accel_x, 2));
  accel["y"] = serialized(String(data.accel_y, 2));
  accel["z"] = serialized(String(data.accel_z, 2));

  JsonObject mag = doc["magnetometer"].to<JsonObject>();
  mag["x"] = serialized(String(data.mag_x, 1));
  mag["y"] = serialized(String(data.mag_y, 1));
  mag["z"] = serialized(String(data.mag_z, 1));

  doc["roll"] = serialized(String(roll, 2));
  doc["pitch"] = serialized(String(pitch, 2));
  doc["timestamp"] = data.timestamp;

  String output;
  serializeJson(doc, output);
  return output;
}

// ========== ВСПОМОГАТЕЛЬНАЯ ФУНКЦИЯ ДЛЯ CORS ==========
void LevelWebServer::addCORSHeaders(AsyncWebServerResponse* response) {
  response->addHeader("Access-Control-Allow-Origin", "*");
  response->addHeader("Access-Control-Allow-Methods",
                      "GET, POST, PUT, DELETE, OPTIONS");
  response->addHeader("Access-Control-Allow-Headers",
                      "Content-Type, Authorization, X-Requested-With");
  response->addHeader("Access-Control-Max-Age", "3600");
}

void LevelWebServer::setupRoutes() {
  // ========== GLOBAL OPTIONS HANDLER (CORS Preflight) ==========
  server.onNotFound([this](AsyncWebServerRequest* request) {
    // Обработка CORS preflight для всех запросов
    if (request->method() == HTTP_OPTIONS) {
      Serial.printf("[CORS] OPTIONS preflight: %s\n", request->url().c_str());
      AsyncWebServerResponse* response = request->beginResponse(200);
      addCORSHeaders(response);
      request->send(response);
      return;
    }

    // Обычный 404
    Serial.printf("[404] %s %s\n", request->methodToString(),
                  request->url().c_str());
    AsyncWebServerResponse* response = request->beginResponse(
        404, "application/json", "{\"error\":\"Not found\"}");
    addCORSHeaders(response);
    request->send(response);
  });

  // ========== ГЛАВНАЯ СТРАНИЦА ==========

  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    Serial.println(F("GET /"));
    request->send(LittleFS, "/index.html", "text/html");
  });

  server.on("/wifimanager.html", HTTP_GET, [](AsyncWebServerRequest* request) {
    Serial.println(F("GET /wifimanager.html"));
    request->send(LittleFS, "/wifimanager.html", "text/html");
  });

  // ========== PING ==========

  server.on("/ping", HTTP_GET, [this](AsyncWebServerRequest* request) {
    AsyncWebServerResponse* response =
        request->beginResponse(200, "text/plain", "pong");
    addCORSHeaders(response);
    request->send(response);
  });

  // ========== DATA ==========

  server.on("/data", HTTP_GET, [this](AsyncWebServerRequest* request) {
    String json = getSensorDataJson();
    AsyncWebServerResponse* response =
        request->beginResponse(200, "application/json", json);
    addCORSHeaders(response);
    request->send(response);
  });

  // ========== WEBSOCKET STATUS ==========

  server.on("/ws/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
    StaticJsonDocument<512> doc;
    doc["clients"] = ws.count();
    doc["connected"] = ws.count() > 0;
    doc["broadcasts"] = broadcastCount;
    doc["free_heap"] = ESP.getFreeHeap();

    JsonArray clientsArray = doc["client_list"].to<JsonArray>();
    for (auto client : ws.getClients()) {
      JsonObject clientObj = clientsArray.createNestedObject();
      clientObj["id"] = client->id();
      clientObj["ip"] = client->remoteIP().toString();
      clientObj["status"] =
          client->status() == WS_CONNECTED ? "connected" : "disconnected";
    }

    String output;
    serializeJson(doc, output);

    AsyncWebServerResponse* response =
        request->beginResponse(200, "application/json", output);
    addCORSHeaders(response);
    request->send(response);
  });

  // ========== WIFI SETTINGS ==========

  server.on("/set_wifi", HTTP_GET, [this](AsyncWebServerRequest* request) {
    Serial.println(F("GET /set_wifi"));

    if (request->hasParam("ssid") && request->hasParam("pass") &&
        request->hasParam("ip") && request->hasParam("gateway")) {
      String ssid = request->getParam("ssid")->value();
      String pass = request->getParam("pass")->value();
      String ip = request->getParam("ip")->value();
      String gateway = request->getParam("gateway")->value();

      fileManager.writeFile(LittleFS, "/ssid.txt", ssid.c_str());
      fileManager.writeFile(LittleFS, "/pass.txt", pass.c_str());
      fileManager.writeFile(LittleFS, "/ip.txt", ip.c_str());
      fileManager.writeFile(LittleFS, "/gateway.txt", gateway.c_str());

      Serial.println(F("WiFi credentials saved"));

      AsyncWebServerResponse* response = request->beginResponse(
          200, "application/json", "{\"message\":\"success\"}");
      addCORSHeaders(response);
      request->send(response);

      delay(1000);
      ESP.restart();
    } else {
      AsyncWebServerResponse* response = request->beginResponse(
          400, "application/json", "{\"error\":\"Missing parameters\"}");
      addCORSHeaders(response);
      request->send(response);
    }
  });

  server.on(
      "/clear_credentials", HTTP_GET, [this](AsyncWebServerRequest* request) {
        Serial.println(F("GET /clear_credentials"));

        fileManager.writeFile(LittleFS, "/ssid.txt", "");
        fileManager.writeFile(LittleFS, "/pass.txt", "");
        fileManager.writeFile(LittleFS, "/ip.txt", "");
        fileManager.writeFile(LittleFS, "/gateway.txt", "");

        AsyncWebServerResponse* response = request->beginResponse(
            200, "application/json", "{\"message\":\"Credentials cleared\"}");
        addCORSHeaders(response);
        request->send(response);

        delay(1000);
        ESP.restart();
      });

  // ========== LEVEL RANGE ==========

  server.on(
      "/set_level_range", HTTP_GET, [this](AsyncWebServerRequest* request) {
        Serial.println(F("GET /set_level_range"));

        if (request->hasParam("min") && request->hasParam("max")) {
          float minAngle = request->getParam("min")->value().toFloat();
          float maxAngle = request->getParam("max")->value().toFloat();

          if (!ConfigManager::setLevelRange(minAngle, maxAngle)) {
            AsyncWebServerResponse* response = request->beginResponse(
                400, "application/json", "{\"error\":\"Invalid range\"}");
            addCORSHeaders(response);
            request->send(response);
            return;
          }

          Serial.printf("Level range updated: %.1f° to %.1f°\n", minAngle,
                        maxAngle);

          StaticJsonDocument<128> doc;
          doc["message"] = "success";
          doc["min"] = minAngle;
          doc["max"] = maxAngle;

          String output;
          serializeJson(doc, output);

          AsyncWebServerResponse* response =
              request->beginResponse(200, "application/json", output);
          addCORSHeaders(response);
          request->send(response);

        } else {
          AsyncWebServerResponse* response = request->beginResponse(
              400, "application/json", "{\"error\":\"Missing parameters\"}");
          addCORSHeaders(response);
          request->send(response);
        }
      });

  server.on("/get_level_range", HTTP_GET,
            [this](AsyncWebServerRequest* request) {
              Serial.println(F("GET /get_level_range"));

              StaticJsonDocument<128> doc;
              doc["min"] = ConfigManager::getLevelMin();
              doc["max"] = ConfigManager::getLevelMax();

              String output;
              serializeJson(doc, output);

              AsyncWebServerResponse* response =
                  request->beginResponse(200, "application/json", output);
              addCORSHeaders(response);
              request->send(response);
            });

  // ========== ZERO CALIBRATION ==========

  server.on(
      "/set_zero_offset", HTTP_GET, [this](AsyncWebServerRequest* request) {
        Serial.println(F("GET /set_zero_offset"));

        if (request->hasParam("offset")) {
          float offset = request->getParam("offset")->value().toFloat();

          if (!ConfigManager::setZeroOffset(offset)) {
            AsyncWebServerResponse* response = request->beginResponse(
                400, "application/json", "{\"error\":\"Invalid offset\"}");
            addCORSHeaders(response);
            request->send(response);
            return;
          }

          Serial.printf("Zero offset updated: %.2f°\n", offset);

          StaticJsonDocument<128> doc;
          doc["message"] = "success";
          doc["offset"] = offset;

          String output;
          serializeJson(doc, output);

          AsyncWebServerResponse* response =
              request->beginResponse(200, "application/json", output);
          addCORSHeaders(response);
          request->send(response);

        } else {
          AsyncWebServerResponse* response = request->beginResponse(
              400, "application/json", "{\"error\":\"Missing parameter\"}");
          addCORSHeaders(response);
          request->send(response);
        }
      });

  server.on(
      "/calibrate_zero", HTTP_GET, [this](AsyncWebServerRequest* request) {
        Serial.println(F("GET /calibrate_zero"));

        float currentRoll = sensorManager.getRoll();
        float newOffset = -currentRoll;

        ConfigManager::setZeroOffset(newOffset);

        Serial.printf("Zero calibrated: offset = %.2f° (was roll %.2f°)\n",
                      newOffset, currentRoll);

        StaticJsonDocument<128> doc;
        doc["message"] = "success";
        doc["offset"] = newOffset;
        doc["previous_roll"] = currentRoll;

        String output;
        serializeJson(doc, output);

        AsyncWebServerResponse* response =
            request->beginResponse(200, "application/json", output);
        addCORSHeaders(response);
        request->send(response);
      });

  server.on("/get_zero_offset", HTTP_GET,
            [this](AsyncWebServerRequest* request) {
              Serial.println(F("GET /get_zero_offset"));

              StaticJsonDocument<128> doc;
              doc["offset"] = ConfigManager::getZeroOffset();

              String output;
              serializeJson(doc, output);

              AsyncWebServerResponse* response =
                  request->beginResponse(200, "application/json", output);
              addCORSHeaders(response);
              request->send(response);
            });

  // ========== AXIS SWAP ==========

  server.on("/set_axis_swap", HTTP_GET, [this](AsyncWebServerRequest* request) {
    Serial.println(F("GET /set_axis_swap"));

    if (request->hasParam("swap")) {
      String swapStr = request->getParam("swap")->value();
      swapStr.toLowerCase();
      bool swap = (swapStr == "true" || swapStr == "1" || swapStr == "yes");

      ConfigManager::setAxisSwap(swap);

      Serial.printf("Axis swap %s\n", swap ? "ENABLED" : "DISABLED");

      StaticJsonDocument<128> doc;
      doc["message"] = "success";
      doc["swap"] = swap;

      String output;
      serializeJson(doc, output);

      AsyncWebServerResponse* response =
          request->beginResponse(200, "application/json", output);
      addCORSHeaders(response);
      request->send(response);

    } else {
      AsyncWebServerResponse* response = request->beginResponse(
          400, "application/json", "{\"error\":\"Missing parameter\"}");
      addCORSHeaders(response);
      request->send(response);
    }
  });

  server.on("/toggle_axis_swap", HTTP_GET,
            [this](AsyncWebServerRequest* request) {
              Serial.println(F("GET /toggle_axis_swap"));

              bool currentSwap = ConfigManager::getAxisSwap();
              bool newSwap = !currentSwap;

              ConfigManager::setAxisSwap(newSwap);

              Serial.printf("Axis swap toggled: %s → %s\n",
                            currentSwap ? "ON" : "OFF", newSwap ? "ON" : "OFF");

              StaticJsonDocument<128> doc;
              doc["message"] = "success";
              doc["swap"] = newSwap;
              doc["previous"] = currentSwap;

              String output;
              serializeJson(doc, output);

              AsyncWebServerResponse* response =
                  request->beginResponse(200, "application/json", output);
              addCORSHeaders(response);
              request->send(response);
            });

  server.on("/get_axis_swap", HTTP_GET, [this](AsyncWebServerRequest* request) {
    Serial.println(F("GET /get_axis_swap"));

    StaticJsonDocument<128> doc;
    doc["swap"] = ConfigManager::getAxisSwap();

    String output;
    serializeJson(doc, output);

    AsyncWebServerResponse* response =
        request->beginResponse(200, "application/json", output);
    addCORSHeaders(response);
    request->send(response);
  });

  // ========== BATTERY ==========

  server.on("/battery", HTTP_GET, [this](AsyncWebServerRequest* request) {
    Serial.println(F("GET /battery"));

    const int BATTERY_PIN = 35;
    int adcValue = analogRead(BATTERY_PIN);
    float voltage = (adcValue / 4095.0f) * 3.3f * 2.0f;
    float percentage = ((voltage - 3.0f) / (4.2f - 3.0f)) * 100.0f;
    if (percentage < 0.0f) percentage = 0.0f;
    if (percentage > 100.0f) percentage = 100.0f;

    String status = "unknown";
    if (percentage >= 99.0f) {
      status = "full";
    } else if (voltage > 4.1f) {
      status = "charging";
    } else {
      status = "discharging";
    }

    StaticJsonDocument<256> doc;
    doc["voltage"] = serialized(String(voltage, 2));
    doc["percentage"] = (int)percentage;
    doc["status"] = status;
    doc["raw_adc"] = adcValue;

    if (percentage < 20.0f) {
      doc["warning"] = "Low battery";
    }
    if (percentage < 10.0f) {
      doc["critical"] = true;
    }

    String output;
    serializeJson(doc, output);

    AsyncWebServerResponse* response =
        request->beginResponse(200, "application/json", output);
    addCORSHeaders(response);
    request->send(response);

    if (percentage < 20.0f) {
      Serial.printf("WARNING: Low battery! %.1f%% (%.2fV)\n", percentage,
                    voltage);
    }
  });

  // ========== SETTINGS ==========

  server.on("/settings", HTTP_GET, [this](AsyncWebServerRequest* request) {
    Serial.println(F("GET /settings"));

    StaticJsonDocument<512> doc;

    JsonObject range = doc["level_range"].to<JsonObject>();
    range["min"] = ConfigManager::getLevelMin();
    range["max"] = ConfigManager::getLevelMax();

    doc["zero_offset"] = ConfigManager::getZeroOffset();
    doc["axis_swap"] = ConfigManager::getAxisSwap();

    const int BATTERY_PIN = 35;
    int adcValue = analogRead(BATTERY_PIN);
    float voltage = (adcValue / 4095.0f) * 3.3f * 2.0f;
    float percentage = ((voltage - 3.0f) / 1.2f) * 100.0f;
    if (percentage < 0.0f) percentage = 0.0f;
    if (percentage > 100.0f) percentage = 100.0f;

    JsonObject battery = doc["battery"].to<JsonObject>();
    battery["voltage"] = serialized(String(voltage, 2));
    battery["percentage"] = (int)percentage;

    String output;
    serializeJson(doc, output);

    AsyncWebServerResponse* response =
        request->beginResponse(200, "application/json", output);
    addCORSHeaders(response);
    request->send(response);
  });

  // ========== STATIC FILES ==========
  server.serveStatic("/", LittleFS, "/");
}