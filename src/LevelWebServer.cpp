// LevelWebServer.cpp - С БИБЛИОТЕКОЙ WebSockets (Links2004)
#include "LevelWebServer.h"

// Статический указатель для callback
LevelWebServer* LevelWebServer::instance = nullptr;

LevelWebServer::LevelWebServer(SensorManager& sensorMgr)
    : httpServer(80),
      wsServer(81),  // WebSocket на порту 81
      sensorManager(sensorMgr),
      wsDebugEnabled(true),
      lastBroadcastTime(0),
      broadcastCount(0),
      wsClientCount(0) {
  instance = this;
}

void LevelWebServer::begin() {
  Serial.println("=== Initializing Web Server ===");

  // Запускаем HTTP сервер
  setupRoutes();
  httpServer.begin();
  Serial.println("HTTP Server started on port 80");

  // Запускаем WebSocket сервер
  wsServer.begin();
  wsServer.onEvent(webSocketEvent);
  Serial.println("WebSocket Server started on port 81");

  Serial.printf("Free heap: %u bytes\n", ESP.getFreeHeap());
}

void LevelWebServer::sendCORSHeaders() {
  httpServer.sendHeader("Access-Control-Allow-Origin", "*");
  httpServer.sendHeader("Access-Control-Allow-Methods",
                        "GET, POST, PUT, DELETE, OPTIONS");
  httpServer.sendHeader("Access-Control-Allow-Headers",
                        "Content-Type, Authorization, X-Requested-With");
  httpServer.sendHeader("Access-Control-Max-Age", "3600");
}

void LevelWebServer::webSocketEvent(uint8_t num, WStype_t type,
                                    uint8_t* payload, size_t length) {
  if (!instance) return;

  switch (type) {
    case WStype_DISCONNECTED: {
      Serial.printf("[WS] ✗ Client #%u DISCONNECTED\n", num);
      instance->wsClientCount--;
      Serial.printf("[WS]   Total clients: %d\n", instance->wsClientCount);
      break;
    }

    case WStype_CONNECTED: {
      IPAddress ip = instance->wsServer.remoteIP(num);
      Serial.printf("[WS] ✓ Client #%u CONNECTED from %s\n", num,
                    ip.toString().c_str());
      instance->wsClientCount++;
      Serial.printf("[WS]   Total clients: %d\n", instance->wsClientCount);

      // Отправляем начальные данные
      String json = instance->getSensorDataJson();
      instance->wsServer.sendTXT(num, json);
      Serial.printf("[WS]   Sent initial data to #%u (%d bytes)\n", num,
                    json.length());
      break;
    }

    case WStype_TEXT: {
      Serial.printf("[WS] Message from #%u: %s\n", num, (char*)payload);
      break;
    }

    case WStype_BIN: {
      Serial.printf("[WS] Binary data from #%u (%u bytes)\n", num, length);
      break;
    }

    case WStype_ERROR: {
      Serial.printf("[WS] ⚠ ERROR on client #%u\n", num);
      break;
    }

    case WStype_PING: {
      if (instance->wsDebugEnabled) {
        Serial.printf("[WS] PING from client #%u\n", num);
      }
      break;
    }

    case WStype_PONG: {
      if (instance->wsDebugEnabled) {
        Serial.printf("[WS] PONG from client #%u\n", num);
      }
      break;
    }
  }
}

void LevelWebServer::broadcastSensorData() {
  unsigned long now = millis();

  // Проверка минимального интервала
  if (now - lastBroadcastTime < BROADCAST_INTERVAL_MS) {
    return;
  }

  // Проверка количества клиентов
  if (wsClientCount == 0) {
    if (wsDebugEnabled && (now - lastBroadcastTime > 5000)) {
      Serial.println("[WS] No clients connected, skipping broadcast");
      lastBroadcastTime = now;
    }
    return;
  }

  // Проверка памяти
  uint32_t freeHeap = ESP.getFreeHeap();
  if (freeHeap < 10000) {
    Serial.printf("[WS] ⚠ Low memory (%u bytes), skipping broadcast\n",
                  freeHeap);
    return;
  }

  // Генерируем JSON
  String json = getSensorDataJson();
  size_t jsonSize = json.length();

  if (jsonSize > 1024) {
    Serial.printf("[WS] ⚠ Message too large (%d bytes)\n", jsonSize);
  }

  // Отправляем всем клиентам
  // broadcastTXT автоматически пропускает отключённых клиентов
  wsServer.broadcastTXT(json);
  broadcastCount++;
  lastBroadcastTime = now;

  // Подробное логирование каждые 10 секунд
  if (wsDebugEnabled && (now - lastBroadcastTime > 10000)) {
    Serial.printf("[WS] Broadcast #%lu: clients=%u, bytes=%d, heap=%u\n",
                  broadcastCount, wsClientCount, jsonSize, freeHeap);
  }
}

void LevelWebServer::handleClients() {
  // Обрабатываем HTTP запросы
  httpServer.handleClient();

  // Обрабатываем WebSocket события
  wsServer.loop();

  // // Периодический ping (каждые 30 секунд)
  // static unsigned long lastPing = 0;
  // unsigned long now = millis();

  // if (now - lastPing > 30000) {
  //   if (wsClientCount > 0) {
  //     Serial.printf("[WS] Sending PING to %d clients...\n", wsClientCount);
  //     // WebSockets библиотека автоматически отправляет ping/pong
  //     wsServer.sendPing();
  //   }
  //   lastPing = now;
  // }
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

void LevelWebServer::setupRoutes() {
  // ========== ГЛАВНАЯ СТРАНИЦА ==========

  httpServer.on("/", HTTP_GET, [this]() {
    Serial.println(F("GET /"));
    File file = LittleFS.open("/index.html", "r");
    if (file) {
      httpServer.streamFile(file, "text/html");
      file.close();
    } else {
      httpServer.send(404, "text/plain", "File not found");
    }
  });

  httpServer.on("/wifimanager.html", HTTP_GET, [this]() {
    Serial.println(F("GET /wifimanager.html"));
    File file = LittleFS.open("/wifimanager.html", "r");
    if (file) {
      httpServer.streamFile(file, "text/html");
      file.close();
    } else {
      httpServer.send(404, "text/plain", "File not found");
    }
  });

  // ========== PING ==========

  httpServer.on("/ping", HTTP_GET, [this]() {
    sendCORSHeaders();
    httpServer.send(200, "text/plain", "pong");
  });

  // ========== DATA ==========

  httpServer.on("/data", HTTP_GET, [this]() {
    String json = getSensorDataJson();
    sendCORSHeaders();
    httpServer.send(200, "application/json", json);
  });

  // ========== WEBSOCKET STATUS ==========

  httpServer.on("/ws/status", HTTP_GET, [this]() {
    StaticJsonDocument<256> doc;
    doc["clients"] = wsClientCount;
    doc["connected"] = wsClientCount > 0;
    doc["broadcasts"] = broadcastCount;
    doc["free_heap"] = ESP.getFreeHeap();
    doc["port"] = 81;

    String output;
    serializeJson(doc, output);

    sendCORSHeaders();
    httpServer.send(200, "application/json", output);
  });

  // ========== WIFI SETTINGS ==========

  httpServer.on("/set_wifi", HTTP_GET, [this]() {
    Serial.println(F("GET /set_wifi"));

    if (httpServer.hasArg("ssid") && httpServer.hasArg("pass") &&
        httpServer.hasArg("ip") && httpServer.hasArg("gateway")) {
      String ssid = httpServer.arg("ssid");
      String pass = httpServer.arg("pass");
      String ip = httpServer.arg("ip");
      String gateway = httpServer.arg("gateway");

      fileManager.writeFile(LittleFS, "/ssid.txt", ssid.c_str());
      fileManager.writeFile(LittleFS, "/pass.txt", pass.c_str());
      fileManager.writeFile(LittleFS, "/ip.txt", ip.c_str());
      fileManager.writeFile(LittleFS, "/gateway.txt", gateway.c_str());

      Serial.println(F("WiFi credentials saved"));

      sendCORSHeaders();
      httpServer.send(200, "application/json", "{\"message\":\"success\"}");

      delay(1000);
      ESP.restart();
    } else {
      sendCORSHeaders();
      httpServer.send(400, "application/json",
                      "{\"error\":\"Missing parameters\"}");
    }
  });

  httpServer.on("/clear_credentials", HTTP_GET, [this]() {
    Serial.println(F("GET /clear_credentials"));

    fileManager.writeFile(LittleFS, "/ssid.txt", "");
    fileManager.writeFile(LittleFS, "/pass.txt", "");
    fileManager.writeFile(LittleFS, "/ip.txt", "");
    fileManager.writeFile(LittleFS, "/gateway.txt", "");

    sendCORSHeaders();
    httpServer.send(200, "application/json",
                    "{\"message\":\"Credentials cleared\"}");

    delay(1000);
    ESP.restart();
  });

  // ========== LEVEL RANGE ==========

  httpServer.on("/set_level_range", HTTP_GET, [this]() {
    Serial.println(F("GET /set_level_range"));

    if (httpServer.hasArg("min") && httpServer.hasArg("max")) {
      float minAngle = httpServer.arg("min").toFloat();
      float maxAngle = httpServer.arg("max").toFloat();

      if (!ConfigManager::setLevelRange(minAngle, maxAngle)) {
        sendCORSHeaders();
        httpServer.send(400, "application/json",
                        "{\"error\":\"Invalid range\"}");
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

      sendCORSHeaders();
      httpServer.send(200, "application/json", output);
    } else {
      sendCORSHeaders();
      httpServer.send(400, "application/json",
                      "{\"error\":\"Missing parameters\"}");
    }
  });

  httpServer.on("/get_level_range", HTTP_GET, [this]() {
    Serial.println(F("GET /get_level_range"));

    StaticJsonDocument<128> doc;
    doc["min"] = ConfigManager::getLevelMin();
    doc["max"] = ConfigManager::getLevelMax();

    String output;
    serializeJson(doc, output);

    sendCORSHeaders();
    httpServer.send(200, "application/json", output);
  });

  // ========== ZERO CALIBRATION ==========

  httpServer.on("/set_zero_offset", HTTP_GET, [this]() {
    Serial.println(F("GET /set_zero_offset"));

    if (httpServer.hasArg("offset")) {
      float offset = httpServer.arg("offset").toFloat();

      if (!ConfigManager::setZeroOffset(offset)) {
        sendCORSHeaders();
        httpServer.send(400, "application/json",
                        "{\"error\":\"Invalid offset\"}");
        return;
      }

      Serial.printf("Zero offset updated: %.2f°\n", offset);

      StaticJsonDocument<128> doc;
      doc["message"] = "success";
      doc["offset"] = offset;

      String output;
      serializeJson(doc, output);

      sendCORSHeaders();
      httpServer.send(200, "application/json", output);
    } else {
      sendCORSHeaders();
      httpServer.send(400, "application/json",
                      "{\"error\":\"Missing parameter\"}");
    }
  });

  httpServer.on("/calibrate_zero", HTTP_GET, [this]() {
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

    sendCORSHeaders();
    httpServer.send(200, "application/json", output);
  });

  httpServer.on("/get_zero_offset", HTTP_GET, [this]() {
    Serial.println(F("GET /get_zero_offset"));

    StaticJsonDocument<128> doc;
    doc["offset"] = ConfigManager::getZeroOffset();

    String output;
    serializeJson(doc, output);

    sendCORSHeaders();
    httpServer.send(200, "application/json", output);
  });

  // ========== AXIS SWAP ==========

  httpServer.on("/set_axis_swap", HTTP_GET, [this]() {
    Serial.println(F("GET /set_axis_swap"));

    if (httpServer.hasArg("swap")) {
      String swapStr = httpServer.arg("swap");
      swapStr.toLowerCase();
      bool swap = (swapStr == "true" || swapStr == "1" || swapStr == "yes");

      ConfigManager::setAxisSwap(swap);

      Serial.printf("Axis swap %s\n", swap ? "ENABLED" : "DISABLED");

      StaticJsonDocument<128> doc;
      doc["message"] = "success";
      doc["swap"] = swap;

      String output;
      serializeJson(doc, output);

      sendCORSHeaders();
      httpServer.send(200, "application/json", output);
    } else {
      sendCORSHeaders();
      httpServer.send(400, "application/json",
                      "{\"error\":\"Missing parameter\"}");
    }
  });

  httpServer.on("/toggle_axis_swap", HTTP_GET, [this]() {
    Serial.println(F("GET /toggle_axis_swap"));

    bool currentSwap = ConfigManager::getAxisSwap();
    bool newSwap = !currentSwap;

    ConfigManager::setAxisSwap(newSwap);

    Serial.printf("Axis swap toggled: %s → %s\n", currentSwap ? "ON" : "OFF",
                  newSwap ? "ON" : "OFF");

    StaticJsonDocument<128> doc;
    doc["message"] = "success";
    doc["swap"] = newSwap;
    doc["previous"] = currentSwap;

    String output;
    serializeJson(doc, output);

    sendCORSHeaders();
    httpServer.send(200, "application/json", output);
  });

  httpServer.on("/get_axis_swap", HTTP_GET, [this]() {
    Serial.println(F("GET /get_axis_swap"));

    StaticJsonDocument<128> doc;
    doc["swap"] = ConfigManager::getAxisSwap();

    String output;
    serializeJson(doc, output);

    sendCORSHeaders();
    httpServer.send(200, "application/json", output);
  });

  // ========== BATTERY ==========

  httpServer.on("/battery", HTTP_GET, [this]() {
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

    sendCORSHeaders();
    httpServer.send(200, "application/json", output);

    if (percentage < 20.0f) {
      Serial.printf("WARNING: Low battery! %.1f%% (%.2fV)\n", percentage,
                    voltage);
    }
  });

  // ========== SETTINGS ==========

  httpServer.on("/settings", HTTP_GET, [this]() {
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

    sendCORSHeaders();
    httpServer.send(200, "application/json", output);
  });

  httpServer.serveStatic("/", LittleFS, "/");

  // ========== CORS PREFLIGHT ==========

  httpServer.onNotFound([this]() {
    if (httpServer.method() == HTTP_OPTIONS) {
      Serial.printf("[CORS] OPTIONS preflight: %s\n", httpServer.uri().c_str());
      sendCORSHeaders();
      httpServer.send(200);
    } else {
      Serial.printf("[404] %s\n", httpServer.uri().c_str());
      sendCORSHeaders();
      httpServer.send(404, "application/json", "{\"error\":\"Not found\"}");
    }
  });
}