// LevelCompact.ino - ОПТИМИЗИРОВАННАЯ ВЕРСИЯ
// Уменьшена частота WebSocket до 5 Hz для стабильности

#include <LittleFS.h>
#include <WiFi.h>

#include "ConfigManager.h"
#include "FileManager.h"
#include "FileSystemManager.h"
#include "LevelIndicator.h"
#include "LevelWebServer.h"
#include "NetworkManager.h"
#include "Pins.h"
#include "Secrets.h"
#include "SensorManager.h"

// ===== КОНФИГУРАЦИЯ =====
const char* SSID_PATH = "/ssid.txt";
const char* PASS_PATH = "/pass.txt";
const char* IP_PATH = "/ip.txt";
const char* GATEWAY_PATH = "/gateway.txt";

// Профиль фильтрации Калмана
const auto FILTER_PROFILE = MultiChannelKalman::RESPONSIVE;

// Частоты обновления
const unsigned long INDICATOR_UPDATE_MS = 30;   // 33 Hz для плавной индикации
const unsigned long WEBSOCKET_UPDATE_MS = 250;  // 4 Hz (вместо 10 Hz!)

// Максимум WebSocket клиентов
const uint8_t MAX_WS_CLIENTS = 3;

// ===== МЕНЕДЖЕРЫ =====
FileSystemManager fsManager;
FileManager fileManager;
NetworkManager networkManager;

SensorManager sensorManager(I2C_SDA_PIN, I2C_SCL_PIN);
LevelWebServer webServer(sensorManager);
LevelIndicator levelIndicator(LED_POSITIVE_1, LED_POSITIVE_2, LED_POSITIVE_3,
                              LED_NEGATIVE_1, LED_NEGATIVE_2, LED_NEGATIVE_3,
                              LED_NEUTRAL);

// ===== ОТЛАДКА =====
bool DEBUG_MODE = false;
bool DEBUG_RANGE_RELOAD = false;  // Выключено для production

// ===== СТАТИСТИКА =====
struct SystemStats {
  unsigned long loopCount;
  unsigned long wsMessagesSent;
  unsigned long wsErrors;
  unsigned long rangeReloads;
  unsigned long settingsReloads;
  unsigned long lastStatsReset;
} stats = {0};

// ===== ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ =====

void startAccessPoint() {
  Serial.println("=== Starting Access Point Mode ===");
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1),
                    IPAddress(255, 255, 255, 0));
  WiFi.softAP(AP_SSID, AP_PASSWORD, 1, 0, MAX_WS_CLIENTS);
  Serial.printf("AP SSID: %s\n", AP_SSID);
  Serial.printf("AP IP: %s\n", WiFi.softAPIP().toString().c_str());
  Serial.printf("Max clients: %d\n", MAX_WS_CLIENTS);
}

void setupWiFi() {
  String ssid = fileManager.readFile(LittleFS, SSID_PATH);
  String pass = fileManager.readFile(LittleFS, PASS_PATH);
  String ip = fileManager.readFile(LittleFS, IP_PATH);
  String gateway = fileManager.readFile(LittleFS, GATEWAY_PATH);

  Serial.printf("Stored SSID: %s\n", ssid.c_str());

  if (ssid.isEmpty() || pass.isEmpty() || ip.isEmpty() || gateway.isEmpty()) {
    Serial.println("WiFi credentials not found");
    startAccessPoint();
    return;
  }

  Serial.println("Attempting to connect to WiFi...");
  if (networkManager.initWiFi(ssid, pass, ip, gateway)) {
    Serial.println("=== Connected to WiFi (STA mode) ===");
    Serial.printf("IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("Failed to connect, falling back to AP mode");
    startAccessPoint();
  }
}

void loadLevelRange() {
  float rangeMin = ConfigManager::getLevelMin();
  float rangeMax = ConfigManager::getLevelMax();

  levelIndicator.setRange(rangeMin, rangeMax);

  if (DEBUG_RANGE_RELOAD) {
    Serial.printf("[RANGE] Loaded from cache: %.1f° to %.1f°\n", rangeMin,
                  rangeMax);
  }
}

void printSystemInfo() {
  Serial.println("\n=== SYSTEM INFO ===");
  Serial.printf("Free heap: %u bytes\n", ESP.getFreeHeap());
  Serial.printf("WiFi SSID: %s\n", WiFi.SSID().c_str());
  Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
  Serial.printf("WebSocket clients: %d\n", webServer.getClientCount());

  float rangeMin, rangeMax;
  levelIndicator.getRange(rangeMin, rangeMax);
  Serial.printf("Current range: %.1f° to %.1f°\n", rangeMin, rangeMax);

  SensorData data = sensorManager.getCachedData();
  Serial.printf("Current angle: %.2f°\n", data.roll);

  Serial.printf("WS messages sent: %lu\n", stats.wsMessagesSent);
  Serial.printf("WS errors: %lu\n", stats.wsErrors);
  Serial.printf("Range reloads: %lu\n", stats.rangeReloads);
  Serial.println("===================\n");
}

// ===== ARDUINO SETUP =====
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n\n=================================");
  Serial.println("    LEVEL GUN v3.2");
  Serial.println("    Optimized WebSocket");
  Serial.println("=================================\n");

  // 1. Файловая система
  if (!fsManager.initLittleFS()) {
    Serial.println("FATAL: LittleFS init failed!");
    while (1) delay(1000);
  }

  // 2. Конфигурация
  ConfigManager::initialize();
  ConfigManager::printConfig();

  // 3. Датчики
  Serial.printf("Initializing sensors with %s profile...\n",
                FILTER_PROFILE == MultiChannelKalman::AGGRESSIVE ? "AGGRESSIVE"
                : FILTER_PROFILE == MultiChannelKalman::BALANCED
                    ? "BALANCED"
                    : "RESPONSIVE");

  if (!sensorManager.begin(FILTER_PROFILE)) {
    Serial.println("FATAL: Sensor init failed!");
    while (1) delay(1000);
  }

  sensorManager.setDebugMode(DEBUG_MODE);

  // 4. Индикатор
  levelIndicator.begin();
  loadLevelRange();

  // 5. WiFi
  setupWiFi();

  // 6. Веб-сервер
  webServer.begin();

  Serial.println("\n=== System Ready ===");
  Serial.printf("Indicator update: %d Hz\n", 1000 / INDICATOR_UPDATE_MS);
  Serial.printf("WebSocket update: %d Hz\n", 1000 / WEBSOCKET_UPDATE_MS);
  Serial.printf("Max WS clients: %d\n", MAX_WS_CLIENTS);

  // Стабилизация
  Serial.println("Stabilizing filters...");
  for (int i = 0; i < 20; i++) {
    sensorManager.update();
    delay(50);
  }
  Serial.println("Ready!\n");

  stats.lastStatsReset = millis();
}

// ===== ARDUINO LOOP =====
void loop() {
  stats.loopCount++;
  unsigned long now = millis();

  // 1. Обновление датчиков (ВСЕГДА)
  sensorManager.update();

  // 2. Светодиодная индикация (33 Hz - без изменений)
  static unsigned long lastIndicatorUpdate = 0;
  if (now - lastIndicatorUpdate >= INDICATOR_UPDATE_MS) {
    float roll = sensorManager.getRoll();
    levelIndicator.update(roll);
    lastIndicatorUpdate = now;
  }

  // 3. WebSocket (5 Hz - УМЕНЬШЕНО!)
  static unsigned long lastBroadcast = 0;
  if (now - lastBroadcast >= WEBSOCKET_UPDATE_MS) {
    uint8_t clientCount = webServer.getClientCount();
    if (clientCount > 0 && clientCount <= MAX_WS_CLIENTS) {
      webServer.broadcastSensorData();
      stats.wsMessagesSent++;
    } else if (clientCount > MAX_WS_CLIENTS) {
      stats.wsErrors++;
      if (DEBUG_MODE) {
        Serial.printf("Too many clients: %d\n", clientCount);
      }
    }
    lastBroadcast = now;
  }

  // 4. Очистка соединений (каждые 10 секунд вместо 5)
  static unsigned long lastCleanup = 0;
  if (now - lastCleanup >= 10000) {
    webServer.handleClients();
    lastCleanup = now;
  }

  // 5. Статистика (каждые 60 секунд)
  static unsigned long lastInfoPrint = 0;
  if (now - lastInfoPrint >= 60000) {
    printSystemInfo();
    lastInfoPrint = now;
  }

  // Минимальная задержка для стабильности
  delay(1);
}