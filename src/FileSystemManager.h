#ifndef FILE_SYSTEM_MANAGER_H
#define FILE_SYSTEM_MANAGER_H

#include <Arduino.h>
#include <LittleFS.h>

class FileSystemManager {
 public:
  FileSystemManager();

  // Инициализация LittleFS
  bool initLittleFS();
};

#endif