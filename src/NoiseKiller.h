// NoiseKiller.h
// Фильтрация шума для датчиков с использованием фильтра Калмана

#ifndef NOISE_KILLER_H
#define NOISE_KILLER_H

#include <Arduino.h>
#include <SimpleKalmanFilter.h>

/**
 * @brief Многоканальный фильтр Калмана для датчиков
 *
 * Каналы:
 * 0-2: Акселерометр (X, Y, Z)
 * 3-5: Магнитометр (X, Y, Z)
 */
class MultiChannelKalman {
 public:
  // Предустановленные профили фильтрации
  enum FilterProfile {
    AGGRESSIVE,  // Сильная фильтрация, медленный отклик (q=0.01, r=0.5)
    BALANCED,    // Баланс между шумом и откликом (q=0.1, r=0.1)
    RESPONSIVE   // Быстрый отклик, больше шума (q=0.5, r=0.05)
  };

  /**
   * @brief Конструктор с профилем фильтрации
   * @param channels Количество каналов (по умолчанию 6)
   * @param profile Профиль фильтрации
   */
  MultiChannelKalman(size_t channels = 6, FilterProfile profile = BALANCED);

  /**
   * @brief Конструктор с ручными параметрами
   * @param channels Количество каналов
   * @param q Шум процесса (process noise) - насколько быстро меняется сигнал
   * @param r Шум измерения (measurement noise) - насколько зашумлены данные
   * @param p Начальная ошибка оценки
   */
  MultiChannelKalman(size_t channels, float q, float r, float p);

  ~MultiChannelKalman();

  /**
   * @brief Обновить фильтр для канала
   * @param channel Номер канала (0-5)
   * @param measurement Новое измерение
   * @return Отфильтрованное значение
   */
  float update(size_t channel, float measurement);

  /**
   * @brief Применить профиль фильтрации ко всем каналам
   */
  void setProfile(FilterProfile profile);

  /**
   * @brief Настроить параметры для всех каналов
   */
  void setParameters(float q, float r, float p);

  /**
   * @brief Настроить параметры для конкретного канала
   */
  void setChannelParameters(size_t channel, float q, float r, float p);

  /**
   * @brief Получить последнее отфильтрованное значение
   */
  float getValue(size_t channel) const;

  /**
   * @brief Сбросить фильтр канала
   */
  void reset(size_t channel, float initial_value = 0.0f);

  /**
   * @brief Сбросить все каналы
   */
  void resetAll(float initial_value = 0.0f);

  /**
   * @brief Получить количество каналов
   */
  size_t getChannelCount() const { return channelCount; }

  /**
   * @brief Вывести информацию о фильтре в Serial
   */
  void printInfo() const;

 private:
  SimpleKalmanFilter** filters;
  size_t channelCount;
  float* lastValues;

  // Текущие параметры фильтров
  float currentQ, currentR, currentP;

  void initFilters(float q, float r, float p);
  void getProfileParameters(FilterProfile profile, float& q, float& r,
                            float& p);
};

#endif  // NOISE_KILLER_H