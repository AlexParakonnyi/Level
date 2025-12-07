// LevelWebServer.cpp - С РАСШИРЕННЫМИ ENDPOINTS
#include "LevelWebServer.h"

LevelWebServer::LevelWebServer(SensorManager& sensorMgr)
    : server(80), ws("/ws"), sensorManager(sensorMgr) {}

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
  // Настройка CORS через DefaultHeaders
  // Настройка CORS для всех запросов

  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods",
                                       "GET, POST, PUT, DELETE, OPTIONS");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers",
                                       "Content-Type, Authorization");
  DefaultHeaders::Instance().addHeader("Access-Control-Max-Age", "3600");

  Serial.println("CORS configured for all origins");
}

void LevelWebServer::setupWebSocket() {
  ws.onEvent([this](AsyncWebSocket* server, AsyncWebSocketClient* client,
                    AwsEventType type, void* arg, uint8_t* data, size_t len) {
    switch (type) {
      case WS_EVT_CONNECT:
        Serial.printf("[WS] Client #%u connected from %s\n", client->id(),
                      client->remoteIP().toString().c_str());
        // Отправляем начальные данные новому клиенту
        broadcastSensorData();
        break;

      case WS_EVT_DISCONNECT:
        Serial.printf("[WS] Client #%u disconnected\n", client->id());
        break;

      case WS_EVT_ERROR:
        Serial.printf("[WS] Error #%u: %s\n", client->id(), (char*)data);
        break;

      case WS_EVT_PONG:
        break;

      case WS_EVT_DATA:
        handleWebSocketMessage(client, data, len);
        break;
    }
  });

  server.addHandler(&ws);
  Serial.println("WebSocket configured");
}

void LevelWebServer::handleWebSocketMessage(AsyncWebSocketClient* client,
                                            uint8_t* data, size_t len) {
  if (len > 0) {
    String message = String((char*)data).substring(0, len);
    Serial.printf("[WS] Message from #%u: %s\n", client->id(), message.c_str());
  }
}

void LevelWebServer::setupRoutes() {
  // // ========== OPTIONS для всех маршрутов (preflight requests) ==========
  // server.on("/*", HTTP_OPTIONS, [](AsyncWebServerRequest* request) {
  //   AsyncWebServerResponse* response = request->beginResponse(200);
  //   response->addHeader("Access-Control-Allow-Origin", "*");
  //   response->addHeader("Access-Control-Allow-Methods",
  //                       "GET, POST, PUT, DELETE, OPTIONS");
  //   response->addHeader("Access-Control-Allow-Headers",
  //                       "Content-Type, Authorization");
  //   response->addHeader("Access-Control-Max-Age", "3600");
  //   request->send(response);
  // });

  // ========== ОСНОВНЫЕ СТРАНИЦЫ ==========

  // Главная страница
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    Serial.println(F("GET /"));
    request->send(LittleFS, "/index.html", "text/html");
  });

  // WiFi Manager страница
  server.on("/wifimanager.html", HTTP_GET, [](AsyncWebServerRequest* request) {
    Serial.println(F("GET /wifimanager.html"));
    request->send(LittleFS, "/wifimanager.html", "text/html");
  });

  // Ping endpoint
  server.on("/ping", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, "text/plain", "pong");
  });

  // ========== ДАННЫЕ ДАТЧИКОВ ==========

  // HTTP endpoint для данных датчиков (для обратной совместимости)
  server.on("/data", HTTP_GET, [this](AsyncWebServerRequest* request) {
    String json = getSensorDataJson();
    request->send(200, "application/json", json);
  });

  // WebSocket статус
  server.on("/ws/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
    StaticJsonDocument<128> doc;
    doc["clients"] = ws.count();
    doc["connected"] = ws.count() > 0;

    String output;
    serializeJson(doc, output);
    request->send(200, "application/json", output);
  });

  // ========== НАСТРОЙКИ WiFi ==========

  // Сохранение WiFi настроек
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
      request->send(200, "application/json", "{\"message\":\"success\"}");

      delay(1000);
      ESP.restart();
    } else {
      request->send(400, "application/json",
                    "{\"error\":\"Missing parameters\"}");
    }
  });

  // Очистка WiFi настроек
  server.on("/clear_credentials", HTTP_GET,
            [this](AsyncWebServerRequest* request) {
              Serial.println(F("GET /clear_credentials"));

              fileManager.writeFile(LittleFS, "/ssid.txt", "");
              fileManager.writeFile(LittleFS, "/pass.txt", "");
              fileManager.writeFile(LittleFS, "/ip.txt", "");
              fileManager.writeFile(LittleFS, "/gateway.txt", "");

              request->send(200, "application/json",
                            "{\"message\":\"Credentials cleared\"}");

              delay(1000);
              ESP.restart();
            });

  // ========== 🆕 НАСТРОЙКИ ДИАПАЗОНА УРОВНЯ ==========

  /**
   * Установка рабочего диапазона уровня
   * GET /set_level_range?min=-45&max=45
   *
   * Параметры:
   *   min - минимальный угол (градусы)
   *   max - максимальный угол (градусы)
   *
   * Пример: /set_level_range?min=-30&max=30
   */
  server.on(
      "/set_level_range", HTTP_GET, [this](AsyncWebServerRequest* request) {
        Serial.println(F("GET /set_level_range"));

        if (request->hasParam("min") && request->hasParam("max")) {
          String minStr = request->getParam("min")->value();
          String maxStr = request->getParam("max")->value();

          float minAngle = minStr.toFloat();
          float maxAngle = maxStr.toFloat();

          // Валидация диапазона
          if (minAngle >= maxAngle) {
            request->send(400, "application/json",
                          "{\"error\":\"min must be less than max\"}");
            return;
          }

          if (minAngle < -90.0f || maxAngle > 90.0f) {
            request->send(
                400, "application/json",
                "{\"error\":\"Range must be between -90 and 90 degrees\"}");
            return;
          }

          // Сохраняем в файлы
          fileManager.writeFile(LittleFS, "/level_min.txt", minStr.c_str());
          fileManager.writeFile(LittleFS, "/level_max.txt", maxStr.c_str());

          Serial.printf("Level range saved: %.1f° to %.1f°\n", minAngle,
                        maxAngle);

          // Формируем JSON ответ
          StaticJsonDocument<128> doc;
          doc["message"] = "success";
          doc["min"] = minAngle;
          doc["max"] = maxAngle;

          String output;
          serializeJson(doc, output);
          request->send(200, "application/json", output);

        } else {
          request->send(400, "application/json",
                        "{\"error\":\"Missing parameters: min, max\"}");
        }
      });

  /**
   * Получить текущий диапазон уровня
   * GET /get_level_range
   *
   * Ответ: {"min": -45.0, "max": 45.0}
   */
  server.on("/get_level_range", HTTP_GET,
            [this](AsyncWebServerRequest* request) {
              Serial.println(F("GET /get_level_range"));

              String minStr = fileManager.readFile(LittleFS, "/level_min.txt");
              String maxStr = fileManager.readFile(LittleFS, "/level_max.txt");

              // Значения по умолчанию
              float minAngle = minStr.isEmpty() ? -45.0f : minStr.toFloat();
              float maxAngle = maxStr.isEmpty() ? 45.0f : maxStr.toFloat();

              StaticJsonDocument<128> doc;
              doc["min"] = minAngle;
              doc["max"] = maxAngle;

              String output;
              serializeJson(doc, output);
              request->send(200, "application/json", output);
            });

  // ========== 🆕 КАЛИБРОВКА НУЛЯ ==========

  /**
   * Установка поправки на нулевое положение
   * GET /set_zero_offset?offset=1.5
   *
   * Параметры:
   *   offset - поправка в градусах (может быть отрицательной)
   *
   * Пример: /set_zero_offset?offset=-2.3
   */
  server.on(
      "/set_zero_offset", HTTP_GET, [this](AsyncWebServerRequest* request) {
        Serial.println(F("GET /set_zero_offset"));

        if (request->hasParam("offset")) {
          String offsetStr = request->getParam("offset")->value();
          float offset = offsetStr.toFloat();

          // Валидация
          if (abs(offset) > 45.0f) {
            request->send(
                400, "application/json",
                "{\"error\":\"Offset must be between -45 and 45 degrees\"}");
            return;
          }

          // Сохраняем в файл
          fileManager.writeFile(LittleFS, "/zero_offset.txt",
                                offsetStr.c_str());

          Serial.printf("Zero offset saved: %.2f°\n", offset);

          // Формируем JSON ответ
          StaticJsonDocument<128> doc;
          doc["message"] = "success";
          doc["offset"] = offset;

          String output;
          serializeJson(doc, output);
          request->send(200, "application/json", output);

        } else {
          request->send(400, "application/json",
                        "{\"error\":\"Missing parameter: offset\"}");
        }
      });

  /**
   * Автоматическая калибровка нуля (текущее положение = 0°)
   * GET /calibrate_zero
   *
   * Берёт текущее значение roll и сохраняет как offset
   */
  server.on(
      "/calibrate_zero", HTTP_GET, [this](AsyncWebServerRequest* request) {
        Serial.println(F("GET /calibrate_zero"));

        // Получаем текущий roll
        float currentRoll = sensorManager.getRoll();

        // Сохраняем как offset (с обратным знаком)
        String offsetStr = String(-currentRoll, 2);
        fileManager.writeFile(LittleFS, "/zero_offset.txt", offsetStr.c_str());

        Serial.printf(
            "Zero calibrated: offset = %.2f° (current roll was %.2f°)\n",
            -currentRoll, currentRoll);

        // Формируем JSON ответ
        StaticJsonDocument<128> doc;
        doc["message"] = "success";
        doc["offset"] = -currentRoll;
        doc["previous_roll"] = currentRoll;

        String output;
        serializeJson(doc, output);
        request->send(200, "application/json", output);
      });

  /**
   * Получить текущий offset
   * GET /get_zero_offset
   *
   * Ответ: {"offset": 1.5}
   */
  server.on(
      "/get_zero_offset", HTTP_GET, [this](AsyncWebServerRequest* request) {
        Serial.println(F("GET /get_zero_offset"));

        String offsetStr = fileManager.readFile(LittleFS, "/zero_offset.txt");
        float offset = offsetStr.isEmpty() ? 0.0f : offsetStr.toFloat();

        StaticJsonDocument<128> doc;
        doc["offset"] = offset;

        String output;
        serializeJson(doc, output);
        request->send(200, "application/json", output);
      });

  // ========== 🆕 ПЕРЕКЛЮЧЕНИЕ ОСЕЙ (ROLL ↔ PITCH) ==========

  /**
   * Установка режима переключения осей
   * GET /set_axis_swap?swap=true
   * GET /set_axis_swap?swap=false
   *
   * Параметры:
   *   swap - true (поменять) или false (нормально)
   *
   * Пример: /set_axis_swap?swap=true
   */
  server.on("/set_axis_swap", HTTP_GET, [this](AsyncWebServerRequest* request) {
    Serial.println(F("GET /set_axis_swap"));

    if (request->hasParam("swap")) {
      String swapStr = request->getParam("swap")->value();
      swapStr.toLowerCase();

      bool swap = (swapStr == "true" || swapStr == "1" || swapStr == "yes");

      // Сохраняем в файл (true/false)
      fileManager.writeFile(LittleFS, "/axis_swap.txt",
                            swap ? "true" : "false");

      Serial.printf("Axis swap %s\n", swap ? "ENABLED" : "DISABLED");

      // Формируем JSON ответ
      StaticJsonDocument<128> doc;
      doc["message"] = "success";
      doc["swap"] = swap;
      doc["description"] = swap ? "Roll and Pitch swapped" : "Normal mode";

      String output;
      serializeJson(doc, output);
      request->send(200, "application/json", output);

    } else {
      request->send(400, "application/json",
                    "{\"error\":\"Missing parameter: swap (true/false)\"}");
    }
  });

  /**
   * Переключение режима swap (toggle)
   * GET /toggle_axis_swap
   *
   * Меняет текущий режим на противоположный
   */
  server.on("/toggle_axis_swap", HTTP_GET,
            [this](AsyncWebServerRequest* request) {
              Serial.println(F("GET /toggle_axis_swap"));

              // Читаем текущее значение
              String swapStr = fileManager.readFile(LittleFS, "/axis_swap.txt");
              bool currentSwap = (swapStr == "true");

              // Инвертируем
              bool newSwap = !currentSwap;
              fileManager.writeFile(LittleFS, "/axis_swap.txt",
                                    newSwap ? "true" : "false");

              Serial.printf("Axis swap toggled: %s → %s\n",
                            currentSwap ? "ON" : "OFF", newSwap ? "ON" : "OFF");

              // Формируем JSON ответ
              StaticJsonDocument<128> doc;
              doc["message"] = "success";
              doc["swap"] = newSwap;
              doc["previous"] = currentSwap;

              String output;
              serializeJson(doc, output);
              request->send(200, "application/json", output);
            });

  /**
   * Получить текущий режим swap
   * GET /get_axis_swap
   *
   * Ответ: {"swap": true}
   */
  server.on("/get_axis_swap", HTTP_GET, [this](AsyncWebServerRequest* request) {
    Serial.println(F("GET /get_axis_swap"));

    String swapStr = fileManager.readFile(LittleFS, "/axis_swap.txt");
    bool swap = (swapStr == "true");

    StaticJsonDocument<128> doc;
    doc["swap"] = swap;
    doc["description"] = swap ? "Roll ↔ Pitch swapped" : "Normal mode";

    String output;
    serializeJson(doc, output);
    request->send(200, "application/json", output);
  });

  // ========== 🆕 УРОВЕНЬ ЗАРЯДА БАТАРЕИ ==========

  /**
   * Получить уровень заряда батареи
   * GET /battery
   *
   * Ответ: {
   *   "voltage": 3.85,
   *   "percentage": 75,
   *   "status": "charging" | "discharging" | "full"
   * }
   */
  server.on("/battery", HTTP_GET, [this](AsyncWebServerRequest* request) {
    Serial.println(F("GET /battery"));

    // Читаем напряжение с ADC (пример для ESP32)
    // Предполагается делитель напряжения на пине 35
    const int BATTERY_PIN = 35;  // GPIO35 (ADC1_CH7)

    // Читаем сырое значение ADC (0-4095 для 12-bit)
    int adcValue = analogRead(BATTERY_PIN);

    // Конвертируем в напряжение
    // ESP32 ADC: 0-4095 → 0-3.3V (с делителем 2:1 → 0-6.6V)
    float voltage = (adcValue / 4095.0f) * 3.3f * 2.0f;

    // Рассчитываем процент заряда (LiPo: 3.0V = 0%, 4.2V = 100%)
    float percentage = ((voltage - 3.0f) / (4.2f - 3.0f)) * 100.0f;
    if (percentage < 0.0f) percentage = 0.0f;
    if (percentage > 100.0f) percentage = 100.0f;

    // Определяем статус
    String status = "unknown";
    if (percentage >= 99.0f) {
      status = "full";
    } else if (voltage > 4.1f) {
      status = "charging";
    } else {
      status = "discharging";
    }

    // Формируем JSON ответ
    StaticJsonDocument<256> doc;
    doc["voltage"] = serialized(String(voltage, 2));
    doc["percentage"] = (int)percentage;
    doc["status"] = status;
    doc["raw_adc"] = adcValue;

    // Предупреждения
    if (percentage < 20.0f) {
      doc["warning"] = "Low battery";
    }
    if (percentage < 10.0f) {
      doc["critical"] = true;
    }

    String output;
    serializeJson(doc, output);
    request->send(200, "application/json", output);

    if (percentage < 20.0f) {
      Serial.printf("WARNING: Low battery! %.1f%% (%.2fV)\n", percentage,
                    voltage);
    }
  });

  /**
   * Получить все настройки сразу
   * GET /settings
   *
   * Ответ: {
   *   "level_range": {"min": -45, "max": 45},
   *   "zero_offset": 1.5,
   *   "axis_swap": false,
   *   "battery": {...}
   * }
   */
  server.on("/settings", HTTP_GET, [this](AsyncWebServerRequest* request) {
    Serial.println(F("GET /settings"));

    StaticJsonDocument<512> doc;

    // Level range
    String minStr = fileManager.readFile(LittleFS, "/level_min.txt");
    String maxStr = fileManager.readFile(LittleFS, "/level_max.txt");
    JsonObject range = doc["level_range"].to<JsonObject>();
    range["min"] = minStr.isEmpty() ? -45.0f : minStr.toFloat();
    range["max"] = maxStr.isEmpty() ? 45.0f : maxStr.toFloat();

    // Zero offset
    String offsetStr = fileManager.readFile(LittleFS, "/zero_offset.txt");
    doc["zero_offset"] = offsetStr.isEmpty() ? 0.0f : offsetStr.toFloat();

    // Axis swap
    String swapStr = fileManager.readFile(LittleFS, "/axis_swap.txt");
    doc["axis_swap"] = (swapStr == "true");

    // Battery (упрощённо)
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
    request->send(200, "application/json", output);
  });

  // ========== СТАТИЧЕСКИЕ ФАЙЛЫ И 404 ==========

  // Статические файлы
  server.serveStatic("/", LittleFS, "/");

  // 404 handler
  server.onNotFound([](AsyncWebServerRequest* request) {
    Serial.printf("404: %s\n", request->url().c_str());
    request->send(404, "application/json", "{\"error\":\"Not found\"}");
  });
}

String LevelWebServer::getSensorDataJson() {
  StaticJsonDocument<256> doc;

  // Получаем данные
  SensorData data = sensorManager.getCachedData();

  // Применяем настройки (offset и swap)
  float roll = data.roll;
  float pitch = data.pitch;

  // 1. Применяем offset
  String offsetStr = fileManager.readFile(LittleFS, "/zero_offset.txt");
  if (!offsetStr.isEmpty()) {
    float offset = offsetStr.toFloat();
    roll += offset;
  }

  // 2. Применяем swap
  String swapStr = fileManager.readFile(LittleFS, "/axis_swap.txt");
  if (swapStr == "true") {
    float temp = roll;
    roll = pitch;
    pitch = temp;
  }

  // Формируем JSON
  JsonObject accel = doc["accelerometer"].to<JsonObject>();
  accel["x"] = serialized(String(data.accel_x, 2));
  accel["y"] = serialized(String(data.accel_y, 2));
  accel["z"] = serialized(String(data.accel_z, 2));

  JsonObject mag = doc["magnetometer"].to<JsonObject>();
  mag["x"] = serialized(String(data.mag_x, 1));
  mag["y"] = serialized(String(data.mag_y, 1));
  mag["z"] = serialized(String(data.mag_z, 1));

  // Обработанные углы
  doc["roll"] = serialized(String(roll, 2));
  doc["pitch"] = serialized(String(pitch, 2));

  doc["timestamp"] = data.timestamp;

  String output;
  serializeJson(doc, output);
  return output;
}

void LevelWebServer::broadcastSensorData() {
  if (ws.count() == 0) return;

  if (ws.count() > 5) {
    Serial.println("[WS] WARNING: Too many clients, skipping broadcast");
    return;
  }

  String json = getSensorDataJson();
  ws.textAll(json);
}

void LevelWebServer::handleClients() {
  ws.cleanupClients();

  static unsigned long lastPing = 0;
  if (millis() - lastPing > 30000) {
    ws.pingAll();
    lastPing = millis();
  }
}