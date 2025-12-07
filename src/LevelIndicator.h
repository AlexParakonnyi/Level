// LevelIndicator.h
// Визуальная индикация угла наклона с динамическим диапазоном

#ifndef LEVEL_INDICATOR_H
#define LEVEL_INDICATOR_H

#include <Arduino.h>

class LevelIndicator {
 public:
  /**
   * @brief Конструктор
   * @param positive1-3 Пины для положительного наклона (зелёные)
   * @param negative1-3 Пины для отрицательного наклона (красные) 
   * @param neutral Пин для нейтрального положения (синий)
   */
  LevelIndicator(uint8_t positive1, uint8_t positive2, uint8_t positive3,
                 uint8_t negative1, uint8_t negative2, uint8_t negative3,
                 uint8_t neutral);

  /**
   * @brief Инициализация пинов
   */
  void begin();

  /**
   * @brief Обновить индикацию на основе угла
   * @param angle Угол (roll или pitch в зависимости от настроек)
   */
  void update(float angle);

  /**
   * @brief Установить рабочий диапазон (min, max)
   * Внутри диапазона: зелёные светодиоды (градиент)
   * Вне диапазона: красные/синие светодиоды (градиент)
   * 
   * @param min Минимальный угол диапазона (градусы)
   * @param max Максимальный угол диапазона (градусы)
   */
  void setRange(float min, float max);

  /**
   * @brief Установить пороги срабатывания для градиента
   * @param low Первый уровень (по умолчанию 33% от диапазона)
   * @param medium Второй уровень (по умолчанию 66% от диапазона)
   * @param high Третий уровень (по умолчанию 100% от диапазона)
   */
  void setThresholds(float low, float medium, float high);

  /**
   * @brief Установить яркость для каждого уровня
   */
  void setBrightness(uint8_t lowBrightness, uint8_t mediumBrightness,
                     uint8_t highBrightness);

  /**
   * @brief Получить текущий диапазон
   */
  void getRange(float& min, float& max) const {
    min = rangeMin;
    max = rangeMax;
  }

  /**
   * @brief Выключить все светодиоды
   */
  void clear();

 private:
  // Пины светодиодов
  uint8_t pinPositive1, pinPositive2, pinPositive3;  // Положительный (зелёные)
  uint8_t pinNegative1, pinNegative2, pinNegative3;  // Отрицательный (красные)
  uint8_t pinNeutral;                                // Нейтральное (синий)

  // Рабочий диапазон (min...max)
  float rangeMin;  // По умолчанию -45°
  float rangeMax;  // По умолчанию +45°

  // Пороги срабатывания (в процентах от диапазона)
  float thresholdLow;     // 33% диапазона
  float thresholdMedium;  // 66% диапазона
  float thresholdHigh;    // 100% диапазона

  // Яркость для каждого уровня
  uint8_t brightnessLow;
  uint8_t brightnessMedium;
  uint8_t brightnessHigh;

  // Каналы PWM для светодиодов
  static const uint8_t PWM_CHANNEL_POS1 = 1;
  static const uint8_t PWM_CHANNEL_POS2 = 2;
  static const uint8_t PWM_CHANNEL_POS3 = 3;
  static const uint8_t PWM_CHANNEL_NEG1 = 4;
  static const uint8_t PWM_CHANNEL_NEG2 = 5;
  static const uint8_t PWM_CHANNEL_NEG3 = 6;
  static const uint8_t PWM_CHANNEL_NEUTRAL = 7;

  static const uint32_t PWM_FREQ = 5000;    // 5 kHz
  static const uint8_t PWM_RESOLUTION = 8;  // 8 бит (0-255)

  // Вспомогательные функции
  void setupPWM();
  void setLED(uint8_t channel, uint8_t brightness);
  
  // Расчёт градиента внутри/вне диапазона
  void calculateGradient(float angle, float rangeSize, 
                        uint8_t& level1, uint8_t& level2, uint8_t& level3);
};

#endif  // LEVEL_INDICATOR_H