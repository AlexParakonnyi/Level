// NoiseKiller.cpp
#include "NoiseKiller.h"

// Конструктор с профилем
MultiChannelKalman::MultiChannelKalman(size_t channels, FilterProfile profile)
    : channelCount(channels), filters(nullptr), lastValues(nullptr) {
  float q, r, p;
  getProfileParameters(profile, q, r, p);

  currentQ = q;
  currentR = r;
  currentP = p;

  initFilters(q, r, p);

  Serial.printf(
      "MultiChannelKalman: %d channels, profile parameters q=%.3f r=%.3f "
      "p=%.3f\n",
      channels, q, r, p);
}

// Конструктор с ручными параметрами
MultiChannelKalman::MultiChannelKalman(size_t channels, float q, float r,
                                       float p)
    : channelCount(channels),
      currentQ(q),
      currentR(r),
      currentP(p),
      filters(nullptr),
      lastValues(nullptr) {
  initFilters(q, r, p);

  Serial.printf(
      "MultiChannelKalman: %d channels, manual parameters q=%.3f r=%.3f "
      "p=%.3f\n",
      channels, q, r, p);
}

MultiChannelKalman::~MultiChannelKalman() {
  if (filters) {
    for (size_t i = 0; i < channelCount; i++) {
      if (filters[i]) {
        delete filters[i];
      }
    }
    delete[] filters;
  }

  if (lastValues) {
    delete[] lastValues;
  }
}

void MultiChannelKalman::initFilters(float q, float r, float p) {
  filters = new SimpleKalmanFilter*[channelCount];
  lastValues = new float[channelCount];

  for (size_t i = 0; i < channelCount; i++) {
    filters[i] = new SimpleKalmanFilter(q, r, p);
    lastValues[i] = 0.0f;
  }
}

void MultiChannelKalman::getProfileParameters(FilterProfile profile, float& q,
                                              float& r, float& p) {
  switch (profile) {
    case AGGRESSIVE:
      // Сильная фильтрация - для очень зашумленных датчиков
      // Медленный отклик, но стабильный сигнал
      q = 0.01f;  // Низкий шум процесса = медленные изменения
      r = 0.5f;   // Высокий шум измерения = сильная фильтрация
      p = 0.1f;   // Начальная ошибка
      break;

    case BALANCED:
      // Баланс - хорошо для большинства случаев
      q = 0.1f;   // Средний шум процесса
      r = 0.1f;   // Средний шум измерения
      p = 0.01f;  // Низкая начальная ошибка
      break;

    case RESPONSIVE:
      // Быстрый отклик - для динамичных движений
      // Больше шума, но быстрая реакция
      q = 0.5f;   // Высокий шум процесса = быстрые изменения
      r = 0.05f;  // Низкий шум измерения = меньше фильтрации
      p = 0.01f;
      break;
  }
}

float MultiChannelKalman::update(size_t channel, float measurement) {
  if (channel >= channelCount || !filters[channel]) {
    return measurement;
  }

  lastValues[channel] = filters[channel]->updateEstimate(measurement);
  return lastValues[channel];
}

void MultiChannelKalman::setProfile(FilterProfile profile) {
  float q, r, p;
  getProfileParameters(profile, q, r, p);
  setParameters(q, r, p);

  Serial.printf("Filter profile changed: q=%.3f r=%.3f p=%.3f\n", q, r, p);
}

void MultiChannelKalman::setParameters(float q, float r, float p) {
  currentQ = q;
  currentR = r;
  currentP = p;

  for (size_t i = 0; i < channelCount; i++) {
    if (filters[i]) {
      delete filters[i];
      filters[i] = new SimpleKalmanFilter(q, r, p);
    }
  }
}

void MultiChannelKalman::setChannelParameters(size_t channel, float q, float r,
                                              float p) {
  if (channel < channelCount && filters[channel]) {
    delete filters[channel];
    filters[channel] = new SimpleKalmanFilter(q, r, p);
  }
}

float MultiChannelKalman::getValue(size_t channel) const {
  if (channel < channelCount) {
    return lastValues[channel];
  }
  return 0.0f;
}

void MultiChannelKalman::reset(size_t channel, float initial_value) {
  if (channel < channelCount && filters[channel]) {
    delete filters[channel];
    filters[channel] = new SimpleKalmanFilter(currentQ, currentR, currentP);
    lastValues[channel] = initial_value;
  }
}

void MultiChannelKalman::resetAll(float initial_value) {
  for (size_t i = 0; i < channelCount; i++) {
    reset(i, initial_value);
  }
  Serial.println("All Kalman filters reset");
}

void MultiChannelKalman::printInfo() const {
  Serial.println("=== Kalman Filter Info ===");
  Serial.printf("Channels: %d\n", channelCount);
  Serial.printf("Parameters: q=%.3f, r=%.3f, p=%.3f\n", currentQ, currentR,
                currentP);
  Serial.println("Channel values:");
  for (size_t i = 0; i < channelCount; i++) {
    Serial.printf("  Ch%d: %.3f\n", i, lastValues[i]);
  }
}