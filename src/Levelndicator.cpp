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
      thresholdLow(0.33f),
      thresholdMedium(0.66f),
      thresholdHigh(1.0f),
      brightnessLow(85),
      brightnessMedium(170),
      brightnessHigh(255) {}

void LevelIndicator::begin() {
  Serial.println("=== Initializing LevelIndicator ===");
  setupPWM();
  clear();
  Serial.printf("Positive LEDs (RED): %d, %d, %d\n", pinPositive1, pinPositive2,
                pinPositive3);
  Serial.printf("Negative LEDs (BLUE): %d, %d, %d\n", pinNegative1,
                pinNegative2, pinNegative3);
  Serial.printf("Neutral LED (GREEN): %d\n", pinNeutral);
  Serial.printf("Range: %.1f° to %.1f°\n", rangeMin, rangeMax);
  Serial.println("=== LevelIndicator ready ===");
}

void LevelIndicator::setupPWM() {
  ledcSetup(PWM_CHANNEL_POS1, PWM_FREQ, PWM_RESOLUTION);
  ledcSetup(PWM_CHANNEL_POS2, PWM_FREQ, PWM_RESOLUTION);
  ledcSetup(PWM_CHANNEL_POS3, PWM_FREQ, PWM_RESOLUTION);
  ledcSetup(PWM_CHANNEL_NEG1, PWM_FREQ, PWM_RESOLUTION);
  ledcSetup(PWM_CHANNEL_NEG2, PWM_FREQ, PWM_RESOLUTION);
  ledcSetup(PWM_CHANNEL_NEG3, PWM_FREQ, PWM_RESOLUTION);
  ledcSetup(PWM_CHANNEL_NEUTRAL, PWM_FREQ, PWM_RESOLUTION);

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
  Serial.printf("Level range updated: %.1f° to %.1f°\n", min, max);
}

void LevelIndicator::update(float angle) {
  // ========================================
  // ЗОНА 1: НИЖЕ rangeMin - СИНИЙ градиент
  // ========================================
  if (angle < rangeMin) {
    // Выключаем красные и зелёный
    setLED(PWM_CHANNEL_POS1, 0);
    setLED(PWM_CHANNEL_POS2, 0);
    setLED(PWM_CHANNEL_POS3, 0);
    setLED(PWM_CHANNEL_NEUTRAL, 0);

    // Расстояние ниже минимума
    float distanceBelow = rangeMin - angle;

    // Нормализация (90° = 100%)
    float maxDistance = 90.0f;
    float percent = min(distanceBelow / maxDistance, 1.0f);

    // Вычисляем градиент для СИНИХ
    uint8_t level1 = 0, level2 = 0, level3 = 0;
    calculateGradient(percent, 1.0f, level1, level2, level3);

    // СИНИЕ светодиоды (NEGATIVE)
    setLED(PWM_CHANNEL_NEG1, level1);
    setLED(PWM_CHANNEL_NEG2, level2);
    setLED(PWM_CHANNEL_NEG3, level3);
  }
  // ========================================
  // ЗОНА 2: ВНУТРИ rangeMin...rangeMax - ЗЕЛЁНЫЙ на полную
  // ========================================
  else if (angle >= rangeMin && angle <= rangeMax) {
    // Выключаем все кроме зелёного
    setLED(PWM_CHANNEL_POS1, 0);
    setLED(PWM_CHANNEL_POS2, 0);
    setLED(PWM_CHANNEL_POS3, 0);
    setLED(PWM_CHANNEL_NEG1, 0);
    setLED(PWM_CHANNEL_NEG2, 0);
    setLED(PWM_CHANNEL_NEG3, 0);

    // ЗЕЛЁНЫЙ (NEUTRAL) на полную
    setLED(PWM_CHANNEL_NEUTRAL, brightnessHigh);
  }
  // ========================================
  // ЗОНА 3: ВЫШЕ rangeMax - КРАСНЫЙ градиент
  // ========================================
  else {  // angle > rangeMax
    // Выключаем синие и зелёный
    setLED(PWM_CHANNEL_NEG1, 0);
    setLED(PWM_CHANNEL_NEG2, 0);
    setLED(PWM_CHANNEL_NEG3, 0);
    setLED(PWM_CHANNEL_NEUTRAL, 0);

    // Расстояние выше максимума
    float distanceAbove = angle - rangeMax;

    // Нормализация (90° = 100%)
    float maxDistance = 90.0f;
    float percent = min(distanceAbove / maxDistance, 1.0f);

    // Вычисляем градиент для КРАСНЫХ
    uint8_t level1 = 0, level2 = 0, level3 = 0;
    calculateGradient(percent, 1.0f, level1, level2, level3);

    // КРАСНЫЕ светодиоды (POSITIVE)
    setLED(PWM_CHANNEL_POS1, level1);
    setLED(PWM_CHANNEL_POS2, level2);
    setLED(PWM_CHANNEL_POS3, level3);
  }
}

void LevelIndicator::calculateGradient(float percent, float maxRange,
                                       uint8_t& level1, uint8_t& level2,
                                       uint8_t& level3) {
  // percent: 0.0 - 1.0

  if (percent < thresholdLow) {
    // 0-33%: только первый светодиод
    float localPercent = percent / thresholdLow;
    level1 = brightnessLow * localPercent;
    level2 = 0;
    level3 = 0;
  } else if (percent < thresholdMedium) {
    // 33-66%: первый полный, второй нарастает
    float localPercent =
        (percent - thresholdLow) / (thresholdMedium - thresholdLow);
    level1 = brightnessLow;
    level2 = brightnessMedium * localPercent;
    level3 = 0;
  } else {
    // 66-100%: первые два полные, третий нарастает
    float localPercent =
        (percent - thresholdMedium) / (thresholdHigh - thresholdMedium);
    level1 = brightnessLow;
    level2 = brightnessMedium;
    level3 = brightnessHigh * localPercent;
  }
}

void LevelIndicator::setThresholds(float low, float medium, float high) {
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
  setLED(PWM_CHANNEL_POS1, 0);
  setLED(PWM_CHANNEL_POS2, 0);
  setLED(PWM_CHANNEL_POS3, 0);
  setLED(PWM_CHANNEL_NEG1, 0);
  setLED(PWM_CHANNEL_NEG2, 0);
  setLED(PWM_CHANNEL_NEG3, 0);
  setLED(PWM_CHANNEL_NEUTRAL, 0);
}