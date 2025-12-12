// ConfigManager.cpp
// Определение статических переменных класса

#include "ConfigManager.h"

// Инициализация статических переменных
float ConfigManager::cachedLevelMin = ConfigManager::DEFAULT_LEVEL_MIN;
float ConfigManager::cachedLevelMax = ConfigManager::DEFAULT_LEVEL_MAX;
float ConfigManager::cachedZeroOffset = ConfigManager::DEFAULT_ZERO_OFFSET;
bool ConfigManager::cachedAxisSwap = ConfigManager::DEFAULT_AXIS_SWAP;