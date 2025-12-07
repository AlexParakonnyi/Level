// LevelCompact.ino
// Полная интеграция всех настроек: offset, swap, dynamic range

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
const unsigned long INDICATOR_UPDATE_MS = 30;   // 33 Гц - быстрая реакция
const unsigned long WEBSOCKET_UPDATE_MS = 100;  // 10 Гц - стабильный WiFi
const unsigned long SETTINGS_RELOAD_MS = 5000;  // 5 сек - перезагрузка настроек

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
bool DEBUG_MODE = false;  // Установите true для вывода статистики фильтрации
unsigned long lastDebugPrint = 0;

// ===== СТАТИСТИКА =====
struct SystemStats {
  unsigned long loopCount;
  unsigned long wsMessagesSent;
  unsigned long wsErrors;
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

  int retries = 0;
  while (WiFi.softAPgetStationNum() == 0 && retries < 30) {
    delay(500);
    Serial.print(".");
    retries++;
  }
  Serial.println();
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
  // Загружаем диапазон из файлов (с использованием ConfigManager)
  float rangeMin = ConfigManager::readFloat(ConfigManager::LEVEL_MIN_PATH,
                                            ConfigManager::DEFAULT_LEVEL_MIN);
  float rangeMax = ConfigManager::readFloat(ConfigManager::LEVEL_MAX_PATH,
                                            ConfigManager::DEFAULT_LEVEL_MAX);

  // Валидация диапазона
  if (!ConfigManager::validateRange(rangeMin, rangeMax)) {
    Serial.println("WARNING: Invalid range detected, using defaults");
    rangeMin = ConfigManager::DEFAULT_LEVEL_MIN;
    rangeMax = ConfigManager::DEFAULT_LEVEL_MAX;
  }

  // Применяем к индикатору
  levelIndicator.setRange(rangeMin, rangeMax);

  Serial.printf("Level range loaded: %.1f° to %.1f°\n", rangeMin, rangeMax);
}
void printSystemStats() {
  unsigned long uptime = millis() / 1000;
  unsigned long period = (millis() - stats.lastStatsReset) / 1000;

  Serial.println("\n=== SYSTEM STATISTICS ===");
  Serial.printf("Uptime: %lu sec\n", uptime);
  Serial.printf("Loop rate: %lu Hz\n",
                stats.loopCount / (period > 0 ? period : 1));
  Serial.printf("WebSocket clients: %d\n", webServer.getClientCount());
  Serial.printf("WS messages sent: %lu\n", stats.wsMessagesSent);
  Serial.printf("WS errors: %lu\n", stats.wsErrors);
  Serial.printf("Free heap: %u bytes\n", ESP.getFreeHeap());

  SensorData data = sensorManager.getCachedData();
  Serial.printf("Current Roll: %.2f° (with offset & swap)\n", data.roll);
  Serial.printf("Current Pitch: %.2f°\n", data.pitch);

  float rangeMin, rangeMax;
  levelIndicator.getRange(rangeMin, rangeMax);
  Serial.printf("Level range: %.1f° to %.1f°\n", rangeMin, rangeMax);

  Serial.println("========================\n");
}
void printSystemInfo() {
  Serial.println("\n=== SYSTEM INFO ===");
  Serial.printf("Free heap: %u bytes\n", ESP.getFreeHeap());
  Serial.printf("WiFi SSID: %s\n", WiFi.SSID().c_str());
  Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
  Serial.printf("WebSocket clients: %d\n", webServer.getClientCount());
  Serial.println("===================\n");
}

// ===== ARDUINO SETUP =====
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n\n=================================");
  Serial.println("    LEVEL GUN v3.0");
  Serial.println("    Dynamic Range + Offset + Swap");
  Serial.println("=================================\n");

  // 1. Инициализация файловой системы
  if (!fsManager.initLittleFS()) {
    Serial.println("FATAL: LittleFS init failed!");
    while (1) delay(1000);
  }

  // 2. Инициализация конфигурационных файлов
  //    Создаёт файлы с дефолтными значениями, если не существуют
  ConfigManager::initializeConfig();

  // 3. Инициализация датчиков
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

  // 4. Инициализация индикаторных светодиодов
  levelIndicator.begin();

  // Загружаем диапазон из конфигурации
  loadLevelRange();

  // 5. Настройка WiFi (AP или STA)
  setupWiFi();

  // 6. Запуск веб-сервера
  webServer.begin();

  Serial.println("\n=== System Ready ===");
  Serial.printf("WebSocket rate: %d Hz\n", 1000 / WEBSOCKET_UPDATE_MS);
  Serial.printf("Indicator rate: %d Hz\n", 1000 / INDICATOR_UPDATE_MS);
  Serial.printf("Settings reload: every %lu sec\n", SETTINGS_RELOAD_MS / 1000);

  // Печатаем текущую конфигурацию
  ConfigManager::printConfig();

  // Стабилизация фильтров
  Serial.println("Stabilizing filters (1 sec)...");
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

  // 1. ПРИОРИТЕТ: Обновление датчиков
  // Автоматически применяет: фильтр Калмана, offset, swap
  sensorManager.update();

  // 2. ПРИОРИТЕТ: Светодиодная индикация
  static unsigned long lastIndicatorUpdate = 0;
  unsigned long now = millis();

  if (now - lastIndicatorUpdate >= INDICATOR_UPDATE_MS) {
    // Получаем roll с учётом всех настроек (offset + swap)
    float roll = sensorManager.getRoll();

    // Обновляем индикатор (учитывает динамический диапазон)
    levelIndicator.update(roll);

    lastIndicatorUpdate = now;
  }

  // 3. WebSocket (пониженная частота)
  static unsigned long lastBroadcast = 0;

  if (now - lastBroadcast >= WEBSOCKET_UPDATE_MS) {
    uint8_t clientCount = webServer.getClientCount();

    if (clientCount > 0 && clientCount <= MAX_WS_CLIENTS) {
      webServer.broadcastSensorData();
      stats.wsMessagesSent++;
    } else if (clientCount > MAX_WS_CLIENTS) {
      stats.wsErrors++;
      if (DEBUG_MODE) {
        Serial.printf("WARNING: Too many clients (%d)\n", clientCount);
      }
    }
    lastBroadcast = now;
  }

  // 4. Очистка мёртвых соединений (каждые 5 секунд)
  static unsigned long lastCleanup = 0;
  if (now - lastCleanup >= 5000) {
    webServer.handleClients();
    lastCleanup = now;
  }

  // 5. Периодическая перезагрузка настроек (каждые 5 секунд)
  static unsigned long lastSettingsReload = 0;
  if (now - lastSettingsReload >= SETTINGS_RELOAD_MS) {
    // Перезагружаем настройки из файлов (если изменились через API)
    sensorManager.loadSettings();
    loadLevelRange();
    lastSettingsReload = now;
  }

  // 6. Периодический вывод статистики (каждые 30 секунд)
  static unsigned long lastInfoPrint = 0;
  if (now - lastInfoPrint >= 30000) {
    printSystemInfo();
    lastInfoPrint = now;
  }

  // Минимальная задержка
  delay(1);
}