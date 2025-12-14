// ConfigManager.h
// Управление конфигурационными файлами с кешированием в памяти

#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <LittleFS.h>

#include "FileManager.h"

class ConfigManager {
 public:
  // Дефолтные значения
  static constexpr float DEFAULT_LEVEL_MIN = -5.0f;
  static constexpr float DEFAULT_LEVEL_MAX = 5.0f;
  static constexpr float DEFAULT_ZERO_OFFSET = 0.0f;
  static constexpr bool DEFAULT_AXIS_SWAP = false;

  // Пути к файлам
  static constexpr const char* LEVEL_MIN_PATH = "/level_min.txt";
  static constexpr const char* LEVEL_MAX_PATH = "/level_max.txt";
  static constexpr const char* ZERO_OFFSET_PATH = "/zero_offset.txt";
  static constexpr const char* AXIS_SWAP_PATH = "/axis_swap.txt";
  static constexpr const char* GATEWAY_PATH = "/gateway.txt";
  static constexpr const char* IP_PATH = "/ip.txt";
  static constexpr const char* SSID_PATH = "/ssid.txt";
  static constexpr const char* PASS_PATH = "/pass.txt";

  /**
   * @brief Инициализация конфигурации
   * Создаёт файлы с дефолтными значениями и загружает в кеш
   */
  static void initialize() {
    Serial.println("=== Initializing Configuration ===");

    // Создаём файлы если не существуют
    initializeFiles();

    // Загружаем в кеш
    loadFromFiles();

    Serial.println("=== Configuration initialized ===\n");
  }

  /**
   * @brief Сброс всех настроек к дефолтным значениям
   */
  static void resetToDefaults() {
    Serial.println("=== Resetting configuration to defaults ===");

    // Записываем дефолты в файлы
    writeFloatToFile(LEVEL_MIN_PATH, DEFAULT_LEVEL_MIN);
    writeFloatToFile(LEVEL_MAX_PATH, DEFAULT_LEVEL_MAX);
    writeFloatToFile(ZERO_OFFSET_PATH, DEFAULT_ZERO_OFFSET);
    writeBoolToFile(AXIS_SWAP_PATH, DEFAULT_AXIS_SWAP);

    // Сбрасываем строковые настройки к пустым значениям
    writeStringToFile(GATEWAY_PATH, "");
    writeStringToFile(IP_PATH, "");
    writeStringToFile(SSID_PATH, "");
    writeStringToFile(PASS_PATH, "");

    // Обновляем кеш
    cachedLevelMin = DEFAULT_LEVEL_MIN;
    cachedLevelMax = DEFAULT_LEVEL_MAX;
    cachedZeroOffset = DEFAULT_ZERO_OFFSET;
    cachedAxisSwap = DEFAULT_AXIS_SWAP;

    Serial.println("Configuration reset complete");
  }

  /**
   * @brief Вывести текущие настройки в Serial
   */
  static void printConfig() {
    Serial.println("=== Current Configuration (Cached) ===");
    Serial.printf("Level Min: %.1f°\n", cachedLevelMin);
    Serial.printf("Level Max: %.1f°\n", cachedLevelMax);
    Serial.printf("Zero Offset: %.2f°\n", cachedZeroOffset);
    Serial.printf("Axis Swap: %s\n", cachedAxisSwap ? "ON" : "OFF");
    Serial.println("======================================\n");
  }

  // ========== ГЕТТЕРЫ (из кеша) ==========

  static float getLevelMin() { return cachedLevelMin; }
  static float getLevelMax() { return cachedLevelMax; }
  static float getZeroOffset() { return cachedZeroOffset; }
  static bool getAxisSwap() { return cachedAxisSwap; }

  // ========== СЕТТЕРЫ (обновляют кеш И файл) ==========

  static bool setLevelMin(float value) {
    if (!validateRange(value, cachedLevelMax)) {
      return false;
    }
    cachedLevelMin = value;
    return writeFloatToFile(LEVEL_MIN_PATH, value);
  }

  static bool setLevelMax(float value) {
    if (!validateRange(cachedLevelMin, value)) {
      return false;
    }
    cachedLevelMax = value;
    return writeFloatToFile(LEVEL_MAX_PATH, value);
  }

  static bool setLevelRange(float min, float max) {
    if (!validateRange(min, max)) {
      return false;
    }
    cachedLevelMin = min;
    cachedLevelMax = max;
    writeFloatToFile(LEVEL_MIN_PATH, min);
    writeFloatToFile(LEVEL_MAX_PATH, max);
    return true;
  }

  static bool setZeroOffset(float value) {
    if (abs(value) > 45.0f) {
      Serial.println("ERROR: Offset must be between -45 and 45");
      return false;
    }
    cachedZeroOffset = value;
    return writeFloatToFile(ZERO_OFFSET_PATH, value);
  }

  static bool setAxisSwap(bool value) {
    cachedAxisSwap = value;
    return writeBoolToFile(AXIS_SWAP_PATH, value);
  }

  // ========== ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ==========

  /**
   * @brief Проверить валидность диапазона
   */
  static bool validateRange(float min, float max) {
    if (min >= max) {
      Serial.println("ERROR: Invalid range (min >= max)");
      return false;
    }
    if (min < -90.0f || max > 90.0f) {
      Serial.println("ERROR: Range outside limits (-90 to +90)");
      return false;
    }
    return true;
  }

  /**
   * @brief Загрузить настройки из файлов в кеш (вызывать только при старте)
   */
  static void loadFromFiles() {
    cachedLevelMin = readFloatFromFile(LEVEL_MIN_PATH, DEFAULT_LEVEL_MIN);
    cachedLevelMax = readFloatFromFile(LEVEL_MAX_PATH, DEFAULT_LEVEL_MAX);
    cachedZeroOffset = readFloatFromFile(ZERO_OFFSET_PATH, DEFAULT_ZERO_OFFSET);
    cachedAxisSwap = readBoolFromFile(AXIS_SWAP_PATH, DEFAULT_AXIS_SWAP);

    Serial.println("Configuration loaded from files:");
    Serial.printf("  Level Min: %.1f°\n", cachedLevelMin);
    Serial.printf("  Level Max: %.1f°\n", cachedLevelMax);
    Serial.printf("  Zero Offset: %.2f°\n", cachedZeroOffset);
    Serial.printf("  Axis Swap: %s\n", cachedAxisSwap ? "ON" : "OFF");
  }

 private:
  // ========== КЕШИРОВАННЫЕ ЗНАЧЕНИЯ ==========
  static float cachedLevelMin;
  static float cachedLevelMax;
  static float cachedZeroOffset;
  static bool cachedAxisSwap;

  // ========== ПРИВАТНЫЕ МЕТОДЫ ==========

  static void initializeFiles() {
    // Числовые настройки
    if (!LittleFS.exists(LEVEL_MIN_PATH)) {
      Serial.printf("Creating %s with default: %.1f\n", LEVEL_MIN_PATH,
                    DEFAULT_LEVEL_MIN);
      writeFloatToFile(LEVEL_MIN_PATH, DEFAULT_LEVEL_MIN);
    }

    if (!LittleFS.exists(LEVEL_MAX_PATH)) {
      Serial.printf("Creating %s with default: %.1f\n", LEVEL_MAX_PATH,
                    DEFAULT_LEVEL_MAX);
      writeFloatToFile(LEVEL_MAX_PATH, DEFAULT_LEVEL_MAX);
    }

    if (!LittleFS.exists(ZERO_OFFSET_PATH)) {
      Serial.printf("Creating %s with default: %.1f\n", ZERO_OFFSET_PATH,
                    DEFAULT_ZERO_OFFSET);
      writeFloatToFile(ZERO_OFFSET_PATH, DEFAULT_ZERO_OFFSET);
    }

    if (!LittleFS.exists(AXIS_SWAP_PATH)) {
      Serial.printf("Creating %s with default: %s\n", AXIS_SWAP_PATH,
                    DEFAULT_AXIS_SWAP ? "true" : "false");
      writeBoolToFile(AXIS_SWAP_PATH, DEFAULT_AXIS_SWAP);
    }

    // Строковые настройки - создаем пустые файлы если не существуют
    if (!LittleFS.exists(GATEWAY_PATH)) {
      Serial.printf("Creating empty file: %s\n", GATEWAY_PATH);
      writeStringToFile(GATEWAY_PATH, "");
    }

    if (!LittleFS.exists(IP_PATH)) {
      Serial.printf("Creating empty file: %s\n", IP_PATH);
      writeStringToFile(IP_PATH, "");
    }

    if (!LittleFS.exists(SSID_PATH)) {
      Serial.printf("Creating empty file: %s\n", SSID_PATH);
      writeStringToFile(SSID_PATH, "");
    }

    if (!LittleFS.exists(PASS_PATH)) {
      Serial.printf("Creating empty file: %s\n", PASS_PATH);
      writeStringToFile(PASS_PATH, "");
    }
  }

  static float readFloatFromFile(const char* path, float defaultValue) {
    if (!LittleFS.exists(path)) {
      return defaultValue;
    }

    File file = LittleFS.open(path, "r");
    if (!file) {
      Serial.printf("ERROR: Failed to open %s for reading\n", path);
      return defaultValue;
    }

    String content = file.readString();
    file.close();

    content.trim();
    if (content.isEmpty()) {
      return defaultValue;
    }

    return content.toFloat();
  }

  static bool readBoolFromFile(const char* path, bool defaultValue) {
    if (!LittleFS.exists(path)) {
      return defaultValue;
    }

    File file = LittleFS.open(path, "r");
    if (!file) {
      Serial.printf("ERROR: Failed to open %s for reading\n", path);
      return defaultValue;
    }

    String content = file.readString();
    file.close();

    content.trim();
    content.toLowerCase();

    if (content == "true" || content == "1") {
      return true;
    } else if (content == "false" || content == "0") {
      return false;
    }

    return defaultValue;
  }

  static bool writeFloatToFile(const char* path, float value) {
    File file = LittleFS.open(path, "w");
    if (!file) {
      Serial.printf("ERROR: Failed to open %s for writing\n", path);
      return false;
    }

    file.print(value, 2);  // 2 знака после запятой
    file.close();
    return true;
  }

  static bool writeBoolToFile(const char* path, bool value) {
    File file = LittleFS.open(path, "w");
    if (!file) {
      Serial.printf("ERROR: Failed to open %s for writing\n", path);
      return false;
    }

    file.print(value ? "true" : "false");
    file.close();
    return true;
  }

  static bool writeStringToFile(const char* path, const String& value) {
    File file = LittleFS.open(path, "w");
    if (!file) {
      Serial.printf("ERROR: Failed to open %s for writing\n", path);
      return false;
    }

    file.print(value);
    file.close();
    return true;
  }
};

#endif  // CONFIG_MANAGER_H