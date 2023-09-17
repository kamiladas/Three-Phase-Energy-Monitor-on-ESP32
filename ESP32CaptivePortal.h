#include <AsyncJson.h>
#include <AsyncWebSocket.h>
#include <AsyncWebSynchronization.h>
#include <ESPAsyncWebSrv.h>
#include "FS.h"
#include "SPIFFS.h"
#include <WiFiUdp.h>
#include <NTPClient.h> 
#ifndef ESP32_CAPTIVE_PORTAL_H
#define ESP32_CAPTIVE_PORTAL_H
#include <WiFi.h>
#include <AsyncTCP.h>
#include <DNSServer.h>

class ESP32CaptivePortal {
public:
  ESP32CaptivePortal();
  float getEnergyConsumption();
  void begin();
  void loop();
  void readConfig(const String &fileName);
  void measureData(int phase);
  int getCustomDateTimeInSeconds(String customDateStr, String customTimeStr);
  bool isConnectedToESP(AsyncWebServerRequest *request);
  bool saveConfig(const String& fileName, const String& field, const String& value);
  void loadConfiguration();
  String loadConfig(const String& fileName, const String& fieldName);
  bool isSSIDAvailable(const String& ssid);
  String getCurrentTime();
  bool isTimeOnline;
private:
  AsyncWebServer server;
  DNSServer dnsServer;
  void mdsnBegin() ; 
  bool isNetworkConfigured; // Dodane pole
  bool isNetworkError;
  static const char index_html[]; // chyba do skasowania
  static const char connected_html[];
  static const char energy_html[] PROGMEM;


  void handleConfiguration(AsyncWebServerRequest *request);
  void handleRoot(AsyncWebServerRequest *request);
  void handleDisconnect(AsyncWebServerRequest *request);
  void handleEnergy(AsyncWebServerRequest *request);
  void handleScanWiFi(AsyncWebServerRequest *request);
  void setupServer();
  void handleTimezones(AsyncWebServerRequest *request);
  String getNetworksJSON();

};

#endif