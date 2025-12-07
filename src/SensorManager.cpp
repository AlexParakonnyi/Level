// SensorManager.cpp
#include "SensorManager.h"

#include <Wire.h>

#include "ConfigManager.h"

SensorManager::SensorManager(uint8_t sda_pin, uint8_t scl_pin)
    : accel(12345),
      mag(12346),
      sdaPin(sda_pin),
      sclPin(scl_pin),
      initialized(false),
      debugMode(false),
      lastUpdate(0),
      updateCount(0),
      lastStatsTime(0),
      kalmanFilter(nullptr),
      zeroOffset(0.0f),
      axisSwap(false) {
  mutex = xSemaphoreCreateMutex();
  if (mutex == NULL) {
    Serial.println("ERROR: Failed to create mutex!");
  }

  filteredCache = {0};
  rawCache = {0};
  filteredCache.valid = false;
}

SensorManager::~SensorManager() {
  if (kalmanFilter) {
    delete kalmanFilter;
  }
  if (mutex) {
    vSemaphoreDelete(mutex);
  }
}

bool SensorManager::begin(MultiChannelKalman::FilterProfile filterProfile) {
  Serial.println("=== Initializing SensorManager ===");

  // Инициализируем I2C
  Wire.begin(sdaPin, sclPin);
  Wire.setClock(400000);
  Serial.printf("I2C initialized (SDA: %d, SCL: %d, 400kHz)\n", sdaPin, sclPin);

  // Сканируем шину
  Serial.println("Scanning I2C bus...");
  int nDevices = 0;
  for (byte addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  Device found at 0x%02X\n", addr);
      nDevices++;
    }
  }
  if (nDevices == 0) {
    Serial.println("WARNING: No I2C devices found!");
  }

  // Инициализируем датчики
  if (!initSensors()) {
    return false;
  }

  // Создаём фильтр Калмана
  kalmanFilter = new MultiChannelKalman(6, filterProfile);
  Serial.println("Kalman filter initialized");

  // Загружаем настройки из файлов
  loadSettings();

  initialized = true;
  Serial.println("=== SensorManager ready ===");
  return true;
}

bool SensorManager::initSensors() {
  if (!accel.begin()) {
    Serial.println("ERROR: LSM303 accelerometer not found!");
    return false;
  }
  Serial.println("Accelerometer initialized");

  if (!mag.begin()) {
    Serial.println("WARNING: LSM303 magnetometer not found!");
  } else {
    Serial.println("Magnetometer initialized");
  }

  sensor_t sensor;
  accel.getSensor(&sensor);
  Serial.println("=== Accelerometer Info ===");
  Serial.printf("  Name: %s\n", sensor.name);
  Serial.printf("  Range: ±%.1f m/s²\n", sensor.max_value);
  Serial.printf("  Resolution: %.3f m/s²\n", sensor.resolution);

  mag.getSensor(&sensor);
  Serial.println("=== Magnetometer Info ===");
  Serial.printf("  Name: %s\n", sensor.name);
  Serial.printf("  Range: ±%.1f µT\n", sensor.max_value);
  Serial.printf("  Resolution: %.3f µT\n", sensor.resolution);

  return true;
}

void SensorManager::loadSettings() {
  // Загружаем offset (с использованием ConfigManager)
  zeroOffset = ConfigManager::readFloat(ConfigManager::ZERO_OFFSET_PATH,
                                        ConfigManager::DEFAULT_ZERO_OFFSET);
  // Serial.printf("Loaded zero offset: %.2f°\n", zeroOffset);

  // Загружаем axis swap
  axisSwap = ConfigManager::readBool(ConfigManager::AXIS_SWAP_PATH,
                                     ConfigManager::DEFAULT_AXIS_SWAP);
  // Serial.printf("Loaded axis swap: %s\n", axisSwap ? "true" : "false");
}

float SensorManager::readFloatFromFile(const char* path, float defaultValue) {
  // Используем ConfigManager для чтения
  return ConfigManager::readFloat(path, defaultValue);
}

bool SensorManager::readBoolFromFile(const char* path, bool defaultValue) {
  // Используем ConfigManager для чтения
  return ConfigManager::readBool(path, defaultValue);
}

void SensorManager::update() {
  if (!initialized) return;

  unsigned long now = millis();
  if (now - lastUpdate < UPDATE_INTERVAL_MS) {
    return;
  }
  lastUpdate = now;
  updateCount++;

  // Читаем сырые данные
  readRawData();

  // Применяем фильтр Калмана
  applyKalmanFilter();

  // Вычисляем ориентацию
  calculateOrientation();

  // Применяем пользовательские настройки (offset, swap)
  applyUserSettings();

  // Обновляем кэш
  updateCache();

  // Отладочный вывод
  if (debugMode && (now - lastStatsTime >= 1000)) {
    printFilterStats();
    lastStatsTime = now;
  }
}

void SensorManager::readRawData() {
  sensors_event_t accelEvent, magEvent;

  if (accel.getEvent(&accelEvent)) {
    rawCache.accel_x = accelEvent.acceleration.x;
    rawCache.accel_y = accelEvent.acceleration.y;
    rawCache.accel_z = accelEvent.acceleration.z;
  } else {
    Serial.println("WARNING: Failed to read accelerometer");
    return;
  }

  if (mag.getEvent(&magEvent)) {
    rawCache.mag_x = magEvent.magnetic.x;
    rawCache.mag_y = magEvent.magnetic.y;
    rawCache.mag_z = magEvent.magnetic.z;
  }

  rawCache.timestamp = millis();
}

void SensorManager::applyKalmanFilter() {
  if (!kalmanFilter) return;

  float filtered_ax = kalmanFilter->update(CH_ACCEL_X, rawCache.accel_x);
  float filtered_ay = kalmanFilter->update(CH_ACCEL_Y, rawCache.accel_y);
  float filtered_az = kalmanFilter->update(CH_ACCEL_Z, rawCache.accel_z);

  float filtered_mx = kalmanFilter->update(CH_MAG_X, rawCache.mag_x);
  float filtered_my = kalmanFilter->update(CH_MAG_Y, rawCache.mag_y);
  float filtered_mz = kalmanFilter->update(CH_MAG_Z, rawCache.mag_z);

  if (xSemaphoreTake(mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
    filteredCache.accel_x = filtered_ax;
    filteredCache.accel_y = filtered_ay;
    filteredCache.accel_z = filtered_az;
    filteredCache.mag_x = filtered_mx;
    filteredCache.mag_y = filtered_my;
    filteredCache.mag_z = filtered_mz;
    filteredCache.timestamp = millis();
    filteredCache.valid = true;
    xSemaphoreGive(mutex);
  }
}

void SensorManager::calculateOrientation() {
  if (xSemaphoreTake(mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
    // Вычисляем базовые углы (без настроек)
    filteredCache.roll = computeRoll(
        filteredCache.accel_x, filteredCache.accel_y, filteredCache.accel_z);

    filteredCache.pitch = computePitch(
        filteredCache.accel_x, filteredCache.accel_y, filteredCache.accel_z);

    xSemaphoreGive(mutex);
  }
}

void SensorManager::applyUserSettings() {
  if (xSemaphoreTake(mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
    float roll = filteredCache.roll;
    float pitch = filteredCache.pitch;

    // 1. Применяем offset (добавляем к roll)
    roll += zeroOffset;

    // 2. Применяем swap (меняем местами)
    if (axisSwap) {
      float temp = roll;
      roll = pitch;
      pitch = temp;
    }

    // Сохраняем обработанные значения
    filteredCache.roll = roll;
    filteredCache.pitch = pitch;

    xSemaphoreGive(mutex);
  }
}

void SensorManager::applyOffset(float& angle) { angle += zeroOffset; }

void SensorManager::applySwap(float& roll, float& pitch) {
  if (axisSwap) {
    float temp = roll;
    roll = pitch;
    pitch = temp;
  }
}

void SensorManager::updateCache() {
  // Кэш уже обновлён в предыдущих функциях
}

float SensorManager::computeRoll(float ax, float ay, float az) {
  if (abs(az) < 0.01f && abs(ay) < 0.01f) {
    return 0.0f;
  }

  float roll = atan2(ay, az) * 180.0f / PI;

  while (roll > 180.0f) roll -= 360.0f;
  while (roll < -180.0f) roll += 360.0f;

  return roll;
}

float SensorManager::computePitch(float ax, float ay, float az) {
  float pitch = atan2(-ax, sqrt(ay * ay + az * az)) * 180.0f / PI;

  if (pitch > 90.0f) pitch = 90.0f;
  if (pitch < -90.0f) pitch = -90.0f;

  return pitch;
}

SensorData SensorManager::getCachedData() {
  SensorData data = {0};

  if (xSemaphoreTake(mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    data = filteredCache;
    xSemaphoreGive(mutex);
  } else {
    Serial.println("WARNING: Failed to take mutex in getCachedData()");
    data.valid = false;
  }

  return data;
}

SensorDataRaw SensorManager::getRawData() { return rawCache; }

float SensorManager::getRoll() {
  float roll = 0.0f;

  if (xSemaphoreTake(mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
    roll = filteredCache.roll;
    xSemaphoreGive(mutex);
  }

  return roll;
}

float SensorManager::getPitch() {
  float pitch = 0.0f;

  if (xSemaphoreTake(mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
    pitch = filteredCache.pitch;
    xSemaphoreGive(mutex);
  }

  return pitch;
}

void SensorManager::setFilterProfile(
    MultiChannelKalman::FilterProfile profile) {
  if (kalmanFilter) {
    kalmanFilter->setProfile(profile);
    Serial.println("Filter profile updated");
  }
}

void SensorManager::resetFilters() {
  if (kalmanFilter) {
    kalmanFilter->resetAll();
    Serial.println("All filters reset");
  }
}

void SensorManager::printFilterStats() {
  SensorData filtered = getCachedData();
  SensorDataRaw raw = getRawData();

  Serial.println("=== Sensor Statistics ===");
  Serial.printf("Update rate: %lu Hz\n", updateCount);
  Serial.printf("Settings: offset=%.2f°, swap=%s\n", zeroOffset,
                axisSwap ? "ON" : "OFF");
  Serial.printf("Raw Roll: %.2f°, Pitch: %.2f°\n",
                computeRoll(raw.accel_x, raw.accel_y, raw.accel_z),
                computePitch(raw.accel_x, raw.accel_y, raw.accel_z));
  Serial.printf("Final Roll: %.2f°, Pitch: %.2f°\n", filtered.roll,
                filtered.pitch);

  updateCount = 0;
}