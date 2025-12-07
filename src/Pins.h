// Pins.h
// Определения пинов ESP32 для проекта Level GUN

#ifndef PINS_H
#define PINS_H

// ===== I2C (датчики LSM303) =====
#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 22

// ===== ИНДИКАТОРНЫЕ СВЕТОДИОДЫ (градиент наклона) =====
// Положительный наклон (зелёные/синие)
#define LED_POSITIVE_1 27  // Слабый наклон (5-10°)
#define LED_POSITIVE_2 26  // Средний наклон (10-15°)
#define LED_POSITIVE_3 33  // Сильный наклон (>15°)

// Отрицательный наклон (красные/оранжевые)
#define LED_NEGATIVE_1 25  // Слабый наклон (-5 до -10°)
#define LED_NEGATIVE_2 32  // Средний наклон (-10 до -15°)
#define LED_NEGATIVE_3 14  // Сильный наклон (<-15°)

// Нейтральное положение
#define LED_NEUTRAL 17  // Горит при наклоне ±5°

#endif  // PINS_H