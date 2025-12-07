#include "NetworkManager.h"

NetworkManager::NetworkManager() {}

bool NetworkManager::initWiFi(String ssid, String pass, String ip,
                              String gateway) {
  WiFi.mode(WIFI_STA);
  WiFi.config(ipAddress(ip), ipAddress(gateway), IPAddress(255, 255, 255, 0));
  WiFi.begin(ssid.c_str(), pass.c_str());

  Serial.print(F("Connecting to WiFi"));
  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 20) {
    delay(500);
    Serial.print(F("."));
    retries++;
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print(F("Connected to WiFi. IP Address: "));
    Serial.println(WiFi.localIP());
    return true;
  } else {
    Serial.println(F("Failed to connect to WiFi"));
    return false;
  }
}

IPAddress NetworkManager::ipAddress(String ip) {
  IPAddress ipAddr;
  ipAddr.fromString(ip);
  return ipAddr;
}