#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>

class NetworkManager {
 public:
  NetworkManager();

  bool initWiFi(String ssid, String pass, String ip, String gateway);
  IPAddress ipAddress(String ip);

 private:
  // Здесь могут быть приватные переменные или методы, если потребуется
};

#endif