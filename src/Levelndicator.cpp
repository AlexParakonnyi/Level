// LevelIndicator.cpp
#include "LevelIndicator.h"

LevelIndicator::LevelIndicator(uint8_t positive1, uint8_t positive2,
                               uint8_t positive3, uint8_t negative1,
                               uint8_t negative2, uint8_t negative3,
                               uint8_t neutral)
    : pinPositive1(positive1),
      pinPositive2(positive2),
      pinPositive3(positive3),
      pinNegative1(negative1),
      pinNegative2(negative2),
      pinNegative3(negative3),
      pinNeutral(neutral),
      rangeMin(-45.0f),
      rangeMax(45.0f),
      thresholdLow(0.33f),     // 33% от диапазона
      thresholdMedium(0.66f),  // 66% от диапазона
      thresholdHigh(1.0f),     // 100% от диапазона
      brightnessLow(85),       // ~33% яркости
      brightnessMedium(170),   // ~66% яркости
      brightnessHigh(255) {}   // 100% яркости

void LevelIndicator::begin() {
  Serial.println("=== Initializing LevelIndicator ===");

  // Настройка PWM для всех светодиодов
  setupPWM();

  // Выключаем все светодиоды
  clear();

  Serial.printf("Positive LEDs (GREEN): %d, %d, %d\n", pinPositive1,
                pinPositive2, pinPositive3);
  Serial.printf("Negative LEDs (RED): %d, %d, %d\n", pinNegative1, pinNegative2,
                pinNegative3);
  Serial.printf("Neutral LED (BLUE): %d\n", pinNeutral);
  Serial.printf("Range: %.1f° to %.1f°\n", rangeMin, rangeMax);
  Serial.println("=== LevelIndicator ready ===");
}

void LevelIndicator::setupPWM() {
  // Настраиваем PWM каналы для каждого светодиода
  ledcSetup(PWM_CHANNEL_POS1, PWM_FREQ, PWM_RESOLUTION);
  ledcSetup(PWM_CHANNEL_POS2, PWM_FREQ, PWM_RESOLUTION);
  ledcSetup(PWM_CHANNEL_POS3, PWM_FREQ, PWM_RESOLUTION);
  ledcSetup(PWM_CHANNEL_NEG1, PWM_FREQ, PWM_RESOLUTION);
  ledcSetup(PWM_CHANNEL_NEG2, PWM_FREQ, PWM_RESOLUTION);
  ledcSetup(PWM_CHANNEL_NEG3, PWM_FREQ, PWM_RESOLUTION);
  ledcSetup(PWM_CHANNEL_NEUTRAL, PWM_FREQ, PWM_RESOLUTION);

  // Привязываем пины к каналам
  ledcAttachPin(pinPositive1, PWM_CHANNEL_POS1);
  ledcAttachPin(pinPositive2, PWM_CHANNEL_POS2);
  ledcAttachPin(pinPositive3, PWM_CHANNEL_POS3);
  ledcAttachPin(pinNegative1, PWM_CHANNEL_NEG1);
  ledcAttachPin(pinNegative2, PWM_CHANNEL_NEG2);
  ledcAttachPin(pinNegative3, PWM_CHANNEL_NEG3);
  ledcAttachPin(pinNeutral, PWM_CHANNEL_NEUTRAL);
}

void LevelIndicator::setLED(uint8_t channel, uint8_t brightness) {
  ledcWrite(channel, brightness);
}

void LevelIndicator::setRange(float min, float max) {
  if (min >= max) {
    Serial.println("ERROR: Invalid range (min >= max)");
    return;
  }

  rangeMin = min;
  rangeMax = max;

  // Serial.printf("Level range updated: %.1f° to %.1f°\n", min, max);
}

void LevelIndicator::update(float angle) {
  // Размер рабочего диапазона
  float rangeSize = rangeMax - rangeMin;
  float rangeCenter = (rangeMin + rangeMax) / 2.0f;

  // Проверяем, находимся ли в нейтральной зоне (±5% от диапазона)
  float neutralZone = rangeSize * 0.05f;
  if (abs(angle - rangeCenter) < neutralZone) {
    // НЕЙТРАЛЬНОЕ ПОЛОЖЕНИЕ: только центральный синий светодиод
    setLED(PWM_CHANNEL_NEUTRAL, brightnessHigh);
    setLED(PWM_CHANNEL_POS1, 0);
    setLED(PWM_CHANNEL_POS2, 0);
    setLED(PWM_CHANNEL_POS3, 0);
    setLED(PWM_CHANNEL_NEG1, 0);
    setLED(PWM_CHANNEL_NEG2, 0);
    setLED(PWM_CHANNEL_NEG3, 0);
    return;
  }

  // Выключаем нейтральный светодиод
  setLED(PWM_CHANNEL_NEUTRAL, 0);

  // ВНУТРИ ДИАПАЗОНА: зелёные светодиоды (POSITIVE)
  if (angle >= rangeMin && angle <= rangeMax) {
    // Определяем сторону относительно центра
    bool isPositive = (angle > rangeCenter);

    // Расстояние от центра
    float distanceFromCenter = abs(angle - rangeCenter);
    float halfRange = rangeSize / 2.0f;

    // Процент от половины диапазона
    float percent = distanceFromCenter / halfRange;

    // Вычисляем яркость для градиента
    uint8_t level1 = 0, level2 = 0, level3 = 0;
    calculateGradient(percent, 1.0f, level1, level2, level3);

    if (isPositive) {
      // Положительная сторона (зелёные)
      setLED(PWM_CHANNEL_POS1, level1);
      setLED(PWM_CHANNEL_POS2, level2);
      setLED(PWM_CHANNEL_POS3, level3);
      setLED(PWM_CHANNEL_NEG1, 0);
      setLED(PWM_CHANNEL_NEG2, 0);
      setLED(PWM_CHANNEL_NEG3, 0);
    } else {
      // Отрицательная сторона (зелёные)
      setLED(PWM_CHANNEL_POS1, 0);
      setLED(PWM_CHANNEL_POS2, 0);
      setLED(PWM_CHANNEL_POS3, 0);
      setLED(PWM_CHANNEL_NEG1, level1);
      setLED(PWM_CHANNEL_NEG2, level2);
      setLED(PWM_CHANNEL_NEG3, level3);
    }
  }
  // ВНЕ ДИАПАЗОНА: красные/синие светодиоды (NEGATIVE)
  else {
    // Определяем, с какой стороны вышли за диапазон
    bool isAboveMax = (angle > rangeMax);

    // Расстояние за пределами диапазона
    float distanceOutside =
        isAboveMax ? (angle - rangeMax) : (rangeMin - angle);

    // Нормализуем расстояние (считаем, что 90° за пределами = максимум)
    float maxOutside = 90.0f;
    float percent = min(distanceOutside / maxOutside, 1.0f);

    // Вычисляем яркость для градиента
    uint8_t level1 = 0, level2 = 0, level3 = 0;
    calculateGradient(percent, 1.0f, level1, level2, level3);

    if (isAboveMax) {
      // Выше максимума (красные на POSITIVE стороне)
      setLED(PWM_CHANNEL_POS1, level1);
      setLED(PWM_CHANNEL_POS2, level2);
      setLED(PWM_CHANNEL_POS3, level3);
      setLED(PWM_CHANNEL_NEG1, 0);
      setLED(PWM_CHANNEL_NEG2, 0);
      setLED(PWM_CHANNEL_NEG3, 0);
    } else {
      // Ниже минимума (синие на NEGATIVE стороне)
      setLED(PWM_CHANNEL_POS1, 0);
      setLED(PWM_CHANNEL_POS2, 0);
      setLED(PWM_CHANNEL_POS3, 0);
      setLED(PWM_CHANNEL_NEG1, level1);
      setLED(PWM_CHANNEL_NEG2, level2);
      setLED(PWM_CHANNEL_NEG3, level3);
    }
  }
}

void LevelIndicator::calculateGradient(float percent, float maxRange,
                                       uint8_t& level1, uint8_t& level2,
                                       uint8_t& level3) {
  // percent: 0.0 - 1.0

  if (percent < thresholdLow) {
    // ПЕРВЫЙ УРОВЕНЬ: 0-33%
    float localPercent = percent / thresholdLow;
    level1 = brightnessLow * localPercent;
    level2 = 0;
    level3 = 0;
  } else if (percent < thresholdMedium) {
    // ВТОРОЙ УРОВЕНЬ: 33-66%
    float localPercent =
        (percent - thresholdLow) / (thresholdMedium - thresholdLow);
    level1 = brightnessLow;
    level2 = brightnessMedium * localPercent;
    level3 = 0;
  } else {
    // ТРЕТИЙ УРОВЕНЬ: 66-100%
    float localPercent =
        (percent - thresholdMedium) / (thresholdHigh - thresholdMedium);
    level1 = brightnessLow;
    level2 = brightnessMedium;
    level3 = brightnessHigh * localPercent;
  }
}

void LevelIndicator::setThresholds(float low, float medium, float high) {
  // Проверяем корректность порогов
  if (low >= medium || medium >= high || high > 1.0f) {
    Serial.println("ERROR: Invalid thresholds");
    return;
  }

  thresholdLow = low;
  thresholdMedium = medium;
  thresholdHigh = high;

  Serial.printf("Thresholds updated: %.2f, %.2f, %.2f\n", low, medium, high);
}

void LevelIndicator::setBrightness(uint8_t lowBrightness,
                                   uint8_t mediumBrightness,
                                   uint8_t highBrightness) {
  brightnessLow = lowBrightness;
  brightnessMedium = mediumBrightness;
  brightnessHigh = highBrightness;

  Serial.printf("Brightness updated: %d, %d, %d\n", lowBrightness,
                mediumBrightness, highBrightness);
}

void LevelIndicator::clear() {
  // Выключаем все светодиоды
  setLED(PWM_CHANNEL_POS1, 0);
  setLED(PWM_CHANNEL_POS2, 0);
  setLED(PWM_CHANNEL_POS3, 0);
  setLED(PWM_CHANNEL_NEG1, 0);
  setLED(PWM_CHANNEL_NEG2, 0);
  setLED(PWM_CHANNEL_NEG3, 0);
  setLED(PWM_CHANNEL_NEUTRAL, 0);
}