// ConfigManager.h
// Управление конфигурационными файлами с дефолтными значениями

#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <LittleFS.h>

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

  /**
   * @brief Инициализация конфигурационных файлов
   * Создаёт файлы с дефолтными значениями, если они не существуют
   */
  static void initializeConfig() {
    Serial.println("=== Initializing Configuration ===");

    // Level Min
    if (!LittleFS.exists(LEVEL_MIN_PATH)) {
      Serial.printf("Creating %s with default value: %.1f\n", LEVEL_MIN_PATH,
                    DEFAULT_LEVEL_MIN);
      writeFloat(LEVEL_MIN_PATH, DEFAULT_LEVEL_MIN);
    } else {
      float value = readFloat(LEVEL_MIN_PATH, DEFAULT_LEVEL_MIN);
      Serial.printf("%s exists: %.1f\n", LEVEL_MIN_PATH, value);
    }

    // Level Max
    if (!LittleFS.exists(LEVEL_MAX_PATH)) {
      Serial.printf("Creating %s with default value: %.1f\n", LEVEL_MAX_PATH,
                    DEFAULT_LEVEL_MAX);
      writeFloat(LEVEL_MAX_PATH, DEFAULT_LEVEL_MAX);
    } else {
      float value = readFloat(LEVEL_MAX_PATH, DEFAULT_LEVEL_MAX);
      Serial.printf("%s exists: %.1f\n", LEVEL_MAX_PATH, value);
    }

    // Zero Offset
    if (!LittleFS.exists(ZERO_OFFSET_PATH)) {
      Serial.printf("Creating %s with default value: %.1f\n", ZERO_OFFSET_PATH,
                    DEFAULT_ZERO_OFFSET);
      writeFloat(ZERO_OFFSET_PATH, DEFAULT_ZERO_OFFSET);
    } else {
      float value = readFloat(ZERO_OFFSET_PATH, DEFAULT_ZERO_OFFSET);
      Serial.printf("%s exists: %.1f\n", ZERO_OFFSET_PATH, value);
    }

    // Axis Swap
    if (!LittleFS.exists(AXIS_SWAP_PATH)) {
      Serial.printf("Creating %s with default value: %s\n", AXIS_SWAP_PATH,
                    DEFAULT_AXIS_SWAP ? "true" : "false");
      writeBool(AXIS_SWAP_PATH, DEFAULT_AXIS_SWAP);
    } else {
      bool value = readBool(AXIS_SWAP_PATH, DEFAULT_AXIS_SWAP);
      Serial.printf("%s exists: %s\n", AXIS_SWAP_PATH,
                    value ? "true" : "false");
    }

    Serial.println("=== Configuration initialized ===\n");
  }

  /**
   * @brief Сброс всех настроек к дефолтным значениям
   */
  static void resetToDefaults() {
    Serial.println("=== Resetting configuration to defaults ===");

    writeFloat(LEVEL_MIN_PATH, DEFAULT_LEVEL_MIN);
    writeFloat(LEVEL_MAX_PATH, DEFAULT_LEVEL_MAX);
    writeFloat(ZERO_OFFSET_PATH, DEFAULT_ZERO_OFFSET);
    writeBool(AXIS_SWAP_PATH, DEFAULT_AXIS_SWAP);

    Serial.println("Configuration reset complete");
  }

  /**
   * @brief Вывести текущие настройки в Serial
   */
  static void printConfig() {
    Serial.println("=== Current Configuration ===");
    Serial.printf("Level Min: %.1f°\n",
                  readFloat(LEVEL_MIN_PATH, DEFAULT_LEVEL_MIN));
    Serial.printf("Level Max: %.1f°\n",
                  readFloat(LEVEL_MAX_PATH, DEFAULT_LEVEL_MAX));
    Serial.printf("Zero Offset: %.2f°\n",
                  readFloat(ZERO_OFFSET_PATH, DEFAULT_ZERO_OFFSET));
    Serial.printf("Axis Swap: %s\n",
                  readBool(AXIS_SWAP_PATH, DEFAULT_AXIS_SWAP) ? "ON" : "OFF");
    Serial.println("===========================\n");
  }

  // === Вспомогательные функции для чтения/записи ===

  /**
   * @brief Прочитать float из файла
   */
  static float readFloat(const char* path, float defaultValue) {
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

  /**
   * @brief Прочитать bool из файла
   */
  static bool readBool(const char* path, bool defaultValue) {
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

  /**
   * @brief Записать float в файл
   */
  static bool writeFloat(const char* path, float value) {
    File file = LittleFS.open(path, "w");
    if (!file) {
      Serial.printf("ERROR: Failed to open %s for writing\n", path);
      return false;
    }

    file.print(value, 2);  // 2 знака после запятой
    file.close();
    return true;
  }

  /**
   * @brief Записать bool в файл
   */
  static bool writeBool(const char* path, bool value) {
    File file = LittleFS.open(path, "w");
    if (!file) {
      Serial.printf("ERROR: Failed to open %s for writing\n", path);
      return false;
    }

    file.print(value ? "true" : "false");
    file.close();
    return true;
  }

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
};

#endif  // CONFIG_MANAGER_H