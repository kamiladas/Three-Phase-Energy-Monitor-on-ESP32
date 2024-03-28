#ifndef ESP32_CAPTIVE_PORTAL_H
#include <ESPAsyncWebSrv.h>
#include "FS.h"
#include "SPIFFS.h"
#include <NTPClient.h> 
#include <WiFi.h>
#include <DNSServer.h>
#include "MeasurementData.h"
#define ESP32_CAPTIVE_PORTAL_H
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
  void setData(const MeasurementData& data) {this->data = data;}
  void setPath(const String& newPath) {path = newPath;  }
  String Generate_FlieName(bool setup);
  void setFPath(const String& newPath) { path = newPath;  }
  String getFPath() const { return path; }
  bool data_ready=false;
  float calculateTimeDifferenceInSeconds(String dateTimeStr1, String dateTimeStr2);
  
private:
  void updateEnergyValues();
  String sumStrings(String A, String B);
  String path;
  String createJsonFileList(String path);
  void handlePhaseDataRequest(AsyncWebServerRequest *request);
  MeasurementData data;
  AsyncWebServer server;
  DNSServer dnsServer;
  void mdsnBegin() ; 
  
  bool isNetworkConfigured; // Dodane pole
  bool isNetworkError;
  static const char index_html[]; 
  static const char connected_html[];//
  static const char energy_html[] PROGMEM;///
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