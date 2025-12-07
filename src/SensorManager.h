// SensorManager.h
// Чтение датчиков + применение настроек (offset, swap)

#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Adafruit_LSM303_U.h>
#include <Adafruit_Sensor.h>
#include <Arduino.h>
#include <LittleFS.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "NoiseKiller.h"

// Структура для сырых данных датчиков
struct SensorDataRaw {
  float accel_x, accel_y, accel_z;
  float mag_x, mag_y, mag_z;
  unsigned long timestamp;
};

// Структура для обработанных данных
struct SensorData {
  float accel_x, accel_y, accel_z;
  float mag_x, mag_y, mag_z;
  unsigned long timestamp;

  float roll;   // Крен (с учётом offset и swap)
  float pitch;  // Тангаж (с учётом offset и swap)
  bool valid;
};

class SensorManager {
 public:
  SensorManager(uint8_t sda_pin, uint8_t scl_pin);
  ~SensorManager();

  /**
   * @brief Инициализация датчиков и фильтров
   */
  bool begin(MultiChannelKalman::FilterProfile filterProfile =
                 MultiChannelKalman::BALANCED);

  /**
   * @brief Обновить данные с датчиков
   * Автоматически применяет: фильтр Калмана, offset, axis swap
   */
  void update();

  /**
   * @brief Получить обработанные данные (с offset и swap)
   */
  SensorData getCachedData();

  /**
   * @brief Получить сырые данные (без обработки)
   */
  SensorDataRaw getRawData();

  /**
   * @brief Получить roll с учётом всех настроек
   */
  float getRoll();

  /**
   * @brief Получить pitch с учётом всех настроек
   */
  float getPitch();

  /**
   * @brief Загрузить настройки из файлов (offset, swap)
   * Вызывается автоматически в begin()
   */
  void loadSettings();

  /**
   * @brief Применить offset к углу
   */
  void applyOffset(float& angle);

  /**
   * @brief Применить swap (поменять roll и pitch местами)
   */
  void applySwap(float& roll, float& pitch);

  /**
   * @brief Изменить профиль фильтрации
   */
  void setFilterProfile(MultiChannelKalman::FilterProfile profile);

  /**
   * @brief Сбросить фильтры
   */
  void resetFilters();

  /**
   * @brief Проверка готовности
   */
  bool isReady() const { return initialized; }

  /**
   * @brief Включить/выключить отладку
   */
  void setDebugMode(bool enabled) { debugMode = enabled; }

  /**
   * @brief Вывести статистику
   */
  void printFilterStats();

 private:
  // Датчики
  Adafruit_LSM303_Accel_Unified accel;
  Adafruit_LSM303_Mag_Unified mag;

  // Фильтр Калмана
  MultiChannelKalman* kalmanFilter;

  // Кэш данных
  SensorData filteredCache;
  SensorDataRaw rawCache;
  SemaphoreHandle_t mutex;

  // Настройки
  uint8_t sdaPin, sclPin;
  bool initialized;
  bool debugMode;

  // Пользовательские настройки (из файлов)
  float zeroOffset;  // Поправка на 0°
  bool axisSwap;     // Поменять roll ↔ pitch

  // Контроль частоты
  unsigned long lastUpdate;
  static const uint16_t UPDATE_INTERVAL_MS = 20;  // 50 Гц

  // Статистика
  unsigned long updateCount;
  unsigned long lastStatsTime;

  // Индексы каналов фильтра
  enum FilterChannels {
    CH_ACCEL_X = 0,
    CH_ACCEL_Y = 1,
    CH_ACCEL_Z = 2,
    CH_MAG_X = 3,
    CH_MAG_Y = 4,
    CH_MAG_Z = 5
  };

  // Вспомогательные функции
  bool initSensors();
  void readRawData();
  void applyKalmanFilter();
  void calculateOrientation();
  void applyUserSettings();
  void updateCache();

  float computeRoll(float ax, float ay, float az);
  float computePitch(float ax, float ay, float az);

  // Чтение настроек из файлов (через ConfigManager)
  float readFloatFromFile(const char* path, float defaultValue);
  bool readBoolFromFile(const char* path, bool defaultValue);
};

#endif  // SENSOR_MANAGER_H