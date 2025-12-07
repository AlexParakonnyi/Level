#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H

#include <Arduino.h>
#include <LittleFS.h>

class FileManager {
 public:
  FileManager();

  // Чтение файла
  String readFile(fs::FS &fs, const char *path);

  // Запись в файл
  void writeFile(fs::FS &fs, const char *path, const char *message);
};

#endif