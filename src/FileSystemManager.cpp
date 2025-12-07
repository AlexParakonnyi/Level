#include "FileSystemManager.h"

FileSystemManager::FileSystemManager() {}

bool FileSystemManager::initLittleFS() {
  if (!LittleFS.begin(false)) {  // Пробуем смонтировать без форматирования
    Serial.println(F("LittleFS Mount Failed, attempting to format..."));
    if (LittleFS.format()) {
      Serial.println(F("LittleFS formatted successfully"));
      if (LittleFS.begin()) {
        Serial.println(F("LittleFS mounted successfully after formatting"));
        return true;
      } else {
        Serial.println(F("LittleFS Mount Failed even after formatting"));
        return false;
      }
    } else {
      Serial.println(F("LittleFS formatting failed"));
      return false;
    }
  }
  Serial.println(F("LittleFS mounted successfully"));
  return true;
}