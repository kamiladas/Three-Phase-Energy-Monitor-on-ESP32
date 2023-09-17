#include "ESP32CaptivePortal.h"
#include <ArduinoJson.h>
#include <ESPmDNS.h>


ESP32CaptivePortal::ESP32CaptivePortal()
  : server(80), isNetworkConfigured(false), isNetworkError(false),isTimeOnline(false){}

void ESP32CaptivePortal::begin() {
  Serial.begin(115200);


  String s_ssid = loadConfig("/config.json", "dev_ssid");
  String s_pass = loadConfig("/config.json", "dev_password");


  if (s_ssid.isEmpty()) {
    s_ssid = "Digital_Multimeter";
  }
  const char *ssid = s_ssid.c_str();
  const char *password = s_pass.c_str();
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);
  Serial.print("AP IP address_ESP: ");
  Serial.println(WiFi.softAPIP());
  dnsServer.start(53, "*", WiFi.softAPIP());  
  if (!MDNS.begin("energy")) {
        Serial.println("Error setting up MDNS responder!");
        while(1) {
            delay(1000);
        }
    }
    Serial.println("mDNS responder started");    
  setupServer();            
  server.begin();
  Serial.println("Captive Portal - WiFi Configuration");
  s_ssid = loadConfig("/config.json", "SSID");
  if (!s_ssid.isEmpty()) { loadConfiguration(); }
  MDNS.addService("http", "tcp", 80);

}

void ESP32CaptivePortal::loop() {
  dnsServer.processNextRequest();
}


void ESP32CaptivePortal::readConfig(const String &fileName) {
  File configFile = SPIFFS.open(fileName, "r");
  if (configFile) {
    while (configFile.available()) {
      Serial.write(configFile.read());
    }
    configFile.close();
    Serial.println("\nPlik konfiguracyjny odczytany.");
  } else {
    Serial.println("Błąd odczytu pliku konfiguracyjnego.");
  }
}

void ESP32CaptivePortal::measureData(int phase) {
    float I = random(0, 13); // Prąd od 0 do 12
    float V = random(230, 251); // Napięcie od 230 do 250
    float cos_fi = abs(-random(0, 101) / 100.0); // Cos(fi) od 0 do -1
    float Active_Power = V * I * cos_fi;
    String time = getCurrentTime();
    
    Serial.print(I);
    Serial.println(" A");
    
    Serial.print(V);
    Serial.println(" V");
    
    Serial.print(cos_fi);
    Serial.println(" cos(fi)");
    
    Serial.print(Active_Power);
    Serial.println(" W");

    Serial.println(time);

}


void ESP32CaptivePortal::handleConfiguration(AsyncWebServerRequest *request) {
  String ssid;
  String password;

  if (request->hasParam("ssid") && request->hasParam("password")) {
    ssid = request->getParam("ssid")->value();
    password = request->getParam("password")->value();

    WiFi.begin(ssid.c_str(), password.c_str());

    int connectionAttempts = 0;
    while (WiFi.status() != WL_CONNECTED && connectionAttempts < 3) {
      delay(1000);
      connectionAttempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("");
      Serial.println("Połączono z WiFi");
      Serial.print("Adres IP: ");
      Serial.println(WiFi.localIP());
      MDNS.addService("http", "tcp", 80);

      // Ustaw flagę isNetworkConfigured na true, jeśli sieć została skonfigurowana
      if (!isNetworkConfigured) {
        isNetworkConfigured = true;
        Serial.println("isNetworkConfigured ustawione na true");
        saveConfig("/config.json", "SSID", ssid);
        saveConfig("/config.json", "password", password);
      }
    } else {
      Serial.println("");
      Serial.println("Połączenie nieudane");

      String html = "<html><body><h2>Połączenie nieudane</h2>";
      html += "<p>Nieprawidłowe hasło. Spróbuj ponownie.</p>";
      html += "<p>Przekierowanie do strony konfiguracji za 5 sekund...</p>";
      html += "<script>setTimeout(function() { window.location.href = '/disconnect'; }, 5000);</script>";
      html += "</body></html>";

      request->send(200, "text/html", html);
    }
     request->redirect("/connected");
  } else {
    request->send(400, "text/plain", "Nieprawidłowe żądanie");
  }

}



void ESP32CaptivePortal::handleRoot(AsyncWebServerRequest *request) {
  if (isNetworkConfigured && isConnectedToESP(request)) {
     File file = SPIFFS.open("/connected.html", "r");
    if (file) {
      file.close();
      request->redirect("/connected");
    } else {
      request->send(404);
    }
  } else if (!isNetworkConfigured && isConnectedToESP(request)) {
    File file = SPIFFS.open("/html/index_html.html", "r");
    if (file) {
      String html = file.readString();
      file.close();
      request->send(200, "text/html", html);
    } else {
      request->send(404);
    }
  } else {
    File file = SPIFFS.open("/html/energy.html", "r");
    if (file) {
      String html = file.readString();
      file.close();
      request->send(200, "text/html", html);
    } else {
      request->send(404);
    }
  }
}





void ESP32CaptivePortal::handleDisconnect(AsyncWebServerRequest *request) {
  WiFi.disconnect();
  isNetworkConfigured = false;
  Serial.println("isNetworkConfigured ustawione na false");
  // Przekieruj na stronę konfiguracji
  if (saveConfig("/config.json", "SSID", "")) {
    Serial.println("Plik konfiguracyjny usunięty.");
  } else {
    Serial.println("Błąd usuwania pliku konfiguracyjnego.");
  }
  request->redirect("/");
}

float ESP32CaptivePortal::getEnergyConsumption() {
  // Tutaj umieść logikę do odczytu aktualnych danych licznika energii
  // Może to być np. odczyt z czujnika, obliczenie na podstawie danych zewnętrznych, itp.
  // W tym przykładzie zwrócę losową wartość dla celów demonstracyjnych

  // Przykład: losowa wartość z zakresu 0.0 do 100.0
  float energy = random(0, 100);

  return energy;
}

void ESP32CaptivePortal::handleEnergy(AsyncWebServerRequest *request) {
  // Pobierz aktualną wartość konsumpcji energii
  float energy = getEnergyConsumption();

  // Przygotuj bufor na przechowywanie wyrenderowanego HTML
  const size_t bufferSize = JSON_OBJECT_SIZE(1);
  DynamicJsonDocument jsonDoc(bufferSize);
  jsonDoc["energy"] = energy;

  String jsonData;
  serializeJson(jsonDoc, jsonData);

  // Otwórz plik "energy.html" na serwerze SPIFFS
  File file = SPIFFS.open("/html/energy.html", "r");

  if (file) {
    // Jeśli plik został otwarty poprawnie, odczytaj jego zawartość
    String html = file.readString();
    file.close();

    // Zastąp placeholder `%ENERGY_CONSUMPTION%` aktualną wartością konsumpcji energii w odpowiedzi HTML
    html.replace("%ENERGY_CONSUMPTION%", jsonData);

    // Wyślij zawartość pliku z zastąpionym JSONem jako odpowiedź HTTP
    request->send(200, "text/html", html);
  } else {
    // Jeśli nie udało się otworzyć pliku, zwróć kod błędu 404 (Nie znaleziono)
    request->send(404);
  }
}





bool ESP32CaptivePortal::isConnectedToESP(AsyncWebServerRequest *request) {
  // Sprawdź adres IP klienta
  IPAddress clientIP = request->client()->remoteIP();

  // Sprawdź, czy adres IP klienta należy do zakresu adresów ESP
  if (clientIP[0] == 192 && clientIP[1] == 168 && clientIP[2] == 4) {
    return true;  // Klient jest podłączony do sieci ESP
  } else {
    return false;  // Klient jest podłączony do sieci lokalnej
  }
}
String ESP32CaptivePortal::getNetworksJSON() {
  int numNetworks = WiFi.scanNetworks();
  String json = "[";
  for (int i = 0; i < numNetworks; i++) {
    if (i > 0) json += ",";
    json += "{\"ssid\":\"" + WiFi.SSID(i) + "\",\"rssi\":" + String(WiFi.RSSI(i)) + "}";
  }
  json += "]";
  return json;
}

void ESP32CaptivePortal::handleScanWiFi(AsyncWebServerRequest *request) {
  String networksJSON = getNetworksJSON();
  request->send(200, "application/json", networksJSON);
}


bool ESP32CaptivePortal::isSSIDAvailable(const String &ssid) {
  int numNetworks = WiFi.scanNetworks();

  for (int i = 0; i < numNetworks; i++) {
    if (WiFi.SSID(i) == ssid) {
      return true;  // Znaleziono SSID
    }
  }

  return false;  // SSID nie jest dostępne
}




bool ESP32CaptivePortal::saveConfig(const String &fileName, const String &field, const String &value) {
  // Sprawdź istnienie pliku
  if (!SPIFFS.exists(fileName)) {
    // Twórz nowy plik i zapisz początkową pustą strukturę JSON
    File configFile = SPIFFS.open(fileName, "w");
    if (!configFile) {
      Serial.println("Błąd tworzenia pliku konfiguracyjnego.");
      return false;
    }
    configFile.print("{}");
    configFile.close();
  }

  // Kontynuuj zapis do pliku
  File configFile = SPIFFS.open(fileName, "r+");
  if (configFile) {
    // Odczytaj zawartość pliku
    size_t fileSize = configFile.size();
    std::unique_ptr<char[]> buf(new char[fileSize]);
    configFile.readBytes(buf.get(), fileSize);
    configFile.close();

    // Przetwórz zawartość pliku jako JSON
    DynamicJsonDocument jsonDoc(1024);
    DeserializationError error = deserializeJson(jsonDoc, buf.get());
    if (error) {
      Serial.println("Błąd deserializacji pliku konfiguracyjnego.");
      return false;
    }

    // Sprawdź, czy pole istnieje w dokumencie JSON
    if (jsonDoc.containsKey(field)) {
      // Pole istnieje - nadpisz jego wartość
      jsonDoc[field] = value;
    } else {
      // Pole nie istnieje - dodaj nowe pole z wartością
      jsonDoc[field] = value;
    }

    // Zapisz zmodyfikowany JSON do pliku
    configFile = SPIFFS.open(fileName, "w");
    if (configFile) {
      serializeJson(jsonDoc, configFile);
      configFile.close();
      Serial.println("Plik konfiguracyjny zapisany.");
      return true;
    } else {
      Serial.println("Błąd zapisu pliku konfiguracyjnego.");
      return false;
    }
  } else {
    Serial.println("Błąd odczytu pliku konfiguracyjnego.");
    return false;
  }
}

void ESP32CaptivePortal::loadConfiguration() {


  // Odczytaj SSID z pliku konfiguracyjnego
  String ssid = loadConfig("/config.json", "SSID");
  String password = loadConfig("/config.json", "password");

  if (isSSIDAvailable(ssid)) {
    // Usuń znaki nowej linii z odczytanych wartości
    ssid.trim();
    password.trim();

    // Podłączanie do sieci WiFi
    WiFi.begin(ssid.c_str(), password.c_str());
    Serial.print("Łączenie z siecią WiFi...");

    int timeout = 10;  // Czas oczekiwania na połączenie (w sekundach)
    while (WiFi.status() != WL_CONNECTED && timeout > 0) {
      delay(1000);
      Serial.print(".");
      timeout--;
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println();
      Serial.print("Połączono z siecią WiFi. Adres IP: ");
      Serial.println(WiFi.localIP());
      isNetworkConfigured = true;
      Serial.println("isNetworkConfigured ustawione na true");
    } else {
      Serial.println();
      Serial.println("Nie udało się połączyć z siecią WiFi.");
    }
  }

  else {
    ////////// obsługa brak SSID
  }
}

/////////////////////////////////////////////////////////////////////



String ESP32CaptivePortal::loadConfig(const String &fileName, const String &fieldName) {
  // Otwórz plik konfiguracyjny
  File configFile = SPIFFS.open(fileName, "r");
  if (configFile) {
    // Wczytaj zawartość pliku
    size_t fileSize = configFile.size();
    std::unique_ptr<char[]> buf(new char[fileSize]);
    configFile.readBytes(buf.get(), fileSize);
    configFile.close();

    // Przetwórz zawartość pliku jako JSON
    DynamicJsonDocument jsonDoc(1024);
    DeserializationError error = deserializeJson(jsonDoc, buf.get());
    if (error) {
      Serial.println("Błąd Serializacji  pliku konfiguracyjnego.");
      return "";
    }

    // Sprawdź, czy pole istnieje
    if (jsonDoc.containsKey(fieldName)) {
      // Pobierz wartość pola
      String fieldValue = jsonDoc[fieldName].as<String>();
      return fieldValue;
    } else {
      Serial.println("Pole '" + fieldName + "' nie istnieje w pliku konfiguracyjnym.");
      return "";
    }
  } else {
    Serial.println("Błąd odczytu pliku konfiguracyjnego.");
    return "";
  }
}

////////////////////////////////////////////////////////////
void ESP32CaptivePortal::handleTimezones(AsyncWebServerRequest *request) {
  if (request->method() == HTTP_POST) {
    if (request->arg("isOfflineTime") == "true") {
      isTimeOnline=true;
      // Czas Offline, odczytaj datę i godzinę z formularza
      String offlineDate = request->arg("offlineDate");
      String offlineTime = request->arg("offlineTime");  
      saveConfig("/config.json", "offlineDate", offlineDate);
      saveConfig("/config.json", "offlineTime", offlineTime);
      Serial.println(loadConfig("/config.json","onlineDate"));
      request->send(200, "text/plain", "Czas Offline został zatwierdzony.");
    }if (request->arg("isOfflineTime") == "false"){
       isTimeOnline=false;
      String selectedTimezone = request->arg("timezone");
      Serial.println("Strefa czasowa: " + selectedTimezone);
      saveConfig("/config.json", "timezone", selectedTimezone);
      request->send(200, "text/plain", "Strefa czasowa została zatwierdzona");
      
    }
  } else if (request->method() == HTTP_GET) {
    // Kod do obsługi żądania GET, jeśli to potrzebne
    if (SPIFFS.exists("/timezone.json")) {
      request->send(SPIFFS, "/timezone.json", "application/json");
    } else {
      request->send(404, "text/plain", "Plik JSON nie istnieje");
    }
  } else {
    request->send(400, "text/plain", "Nieprawidłowe żądanie.");
  }
}

///////////////////////////////////
String ESP32CaptivePortal::getCurrentTime() {
WiFiUDP ntpUDP;
  NTPClient timeClient(ntpUDP, "time.google.com");
 String dayStamp;
 String timeStamp;
 String formattedDate;
 String formattedTime;
 int splitT;
  timeClient.begin();
  timeClient.update();

  String timezone = loadConfig("/config.json", "timezone");
  if (timezone != "" && timezone != "0"&&isTimeOnline==false) {
    int timezoneValue = timezone.toInt();
    timeClient.setTimeOffset(3600 * timezoneValue);  // Ustawienie przesunięcia czasowego (w sekundach)
  }
   if  ( isTimeOnline==true)
  {    
      formattedDate = timeClient.getFormattedDate();
      formattedTime = timeClient.getFormattedTime();
      splitT = formattedDate.indexOf("T");
      dayStamp = formattedDate.substring(0, splitT);
      timeStamp = formattedDate.substring(splitT + 1, formattedDate.length() - 1);
      static int timeZeroOffset;
      static String prevTime = "";
      static String prevDate = "";
      String  offlineDate = loadConfig("/config.json", "offlineDate");
      String  offlineTime = loadConfig("/config.json", "offlineTime");   
      int timeOffset=getCustomDateTimeInSeconds(offlineDate,offlineTime);
       if (offlineTime!=prevTime||offlineDate!=prevDate){
       prevDate=offlineDate;
       prevTime=offlineTime;
       timeZeroOffset=(getCustomDateTimeInSeconds(dayStamp,timeStamp)*-1); 
       
       }
       timeClient.setTimeOffset(timeOffset+timeZeroOffset); 
  }
  timeClient.update();
  formattedDate = timeClient.getFormattedDate();
  formattedTime = timeClient.getFormattedTime();
  splitT = formattedDate.indexOf("T");
  dayStamp = formattedDate.substring(0, splitT);
  timeStamp = formattedDate.substring(splitT + 1, formattedDate.length() - 1);
  String combinedString = dayStamp + " " + formattedTime;
  
  return combinedString;

}


int ESP32CaptivePortal::getCustomDateTimeInSeconds( String customDateStr,String customTimeStr) {
 
  int year = customDateStr.substring(0, 4).toInt();
  int month = customDateStr.substring(5, 7).toInt();
  int day = customDateStr.substring(8, 10).toInt();
  int hour = customTimeStr.substring(0, 2).toInt();
  int minute = customTimeStr.substring(3, 5).toInt();
  int second = customTimeStr.substring(6, 8).toInt();
  Serial.println(second);
  // Utwórz struct tm i ustaw podaną datę i godzinę
  struct tm timeinfo;
  timeinfo.tm_year = year - 1900;
  timeinfo.tm_mon = month - 1;
  timeinfo.tm_mday = day;
  timeinfo.tm_hour = hour;
  timeinfo.tm_min = minute;
  timeinfo.tm_sec = second;

  // Użyj mktime() do przeliczenia na liczbę sekund od czasu zero
  time_t customDateTime = mktime(&timeinfo);

  // Zwróć ilość sekund jako wartość funkcji
  return static_cast<int>(customDateTime);
}
void ESP32CaptivePortal::setupServer() {
  server.on("/", HTTP_GET, [this](AsyncWebServerRequest *request) {
    handleRoot(request);
  });

  server.on("/configure", HTTP_GET, [this](AsyncWebServerRequest *request) {
    if (isConnectedToESP(request)) {
      handleConfiguration(request);
    } else {
      request->send(SPIFFS, "/html/handle_localUser.html", "text/html");
    }
  });
  //// trzeba zabezpieczyć
  server.on("/scanWiFi", HTTP_GET, [this](AsyncWebServerRequest *request) {
    handleScanWiFi(request);
  });

     server.on("/getTimezones", HTTP_ANY, [this](AsyncWebServerRequest *request) {
    handleTimezones(request);
  });

  server.on("/disconnect", HTTP_GET, [this](AsyncWebServerRequest *request) {
    if (isConnectedToESP(request)) {
      handleDisconnect(request);
    } else {
      request->send(SPIFFS, "/html/handle_localUser.html", "text/html");
    }
  });

  server.on("/connected", HTTP_GET, [this](AsyncWebServerRequest *request) {
    if (isConnectedToESP(request)) {
      if (SPIFFS.exists("/html/connected.html")) {
        File htmlFile = SPIFFS.open("/html/connected.html", "r");
        String html = htmlFile.readString();
        htmlFile.close();
        html.replace("%IP_ADDRESS%", WiFi.localIP().toString());
        request->send(200, "text/html", html);
      } else {
        request->send(404);
      }
    } else {
      request->send(SPIFFS, "/html/handle_localUser.html", "text/html");
    }
  });

  server.onNotFound(std::bind(&ESP32CaptivePortal::handleRoot, this, std::placeholders::_1));

  server.on("/energy", HTTP_GET, std::bind(&ESP32CaptivePortal::handleEnergy, this, std::placeholders::_1));

  server.on("/settings", HTTP_GET, [this](AsyncWebServerRequest *request) {
    if (isConnectedToESP(request)) {
      request->send(SPIFFS, "/html/settings.html", "text/html");
    } else { 
      request->send(SPIFFS, "/html/handle_localUser.html", "text/html");
    }
  });

  server.on("/resetFactorySettings", HTTP_POST, [this](AsyncWebServerRequest *request) {
    if (isConnectedToESP(request)) {
      if (SPIFFS.remove("/config.json")) {
        request->send(200, "text/plain", "Ustawienia fabryczne zostały przywrócone.");
        WiFi.disconnect(true);
        delay(100);
        request->send(200, "text/plain", "resetowania urządzenia .");
        ESP.restart();  // Używamy ESP.restart() zamiast esp.restart()
      } else {
        request->send(500, "text/plain", "Błąd podczas resetowania ustawień fabrycznych.");
      }
    } else {
      request->send(SPIFFS, "/html/handle_localUser.html", "text/html");
    }
  });

  server.on("/changePassword", HTTP_POST, [this](AsyncWebServerRequest *request) {
    if (isConnectedToESP(request)) {
      // Pobierz przesłane dane formularza
      String currentPassword = request->arg("currentPassword");
      String newPassword = request->arg("newPassword");
      // Pobierz z pliku konfiguracyjnego aktualne hasło
      String savedPassword = this->loadConfig("/config.json", "dev_password");

      // Porównaj przesłane hasło z zapisanym hasłem
      if (currentPassword == savedPassword) {
        // Zapisz nowe hasło w pliku konfiguracyjnym
        bool success = this->saveConfig("/config.json", "dev_password", newPassword);

        if (success) {
          // Przygotuj treść odpowiedzi tekstowej z informacją o sukcesie
          String response = "Hasło zostało zmienione. Urządzenie zostanie zrestartowane.";
          request->send(200, "text/plain", response);
          delay(100);
          ESP.restart();
          Serial.println("savedPassword: " + newPassword);
        } else {
          // Przygotuj treść odpowiedzi tekstowej z informacją o błędzie zapisu
          String response = "Błąd podczas zapisywania hasła.";
          request->send(500, "text/plain", response);
        }
      } else {
        // Przesłane hasło nie jest poprawne
        String response = "Obecne hasło jest nieprawidłowe.";
        request->send(400, "text/plain", response);
      }
    } else {
      request->send(SPIFFS, "/html/handle_localUser.html", "text/html");
    }
  });

  server.on("/changeSSID", HTTP_POST, [this](AsyncWebServerRequest *request) {
    // Pobierz aktualną nazwę SSID
    if (isConnectedToESP(request)) {
      String currentSSID = loadConfig("/config.json", "dev_ssid");

      // Odczytaj nowe SSID z formularza
      String newSSID = request->arg("ssid");
      // Sprawdź, czy nowe SSID jest zgodne z aktualnym
      if (newSSID == currentSSID) {
        request->send(200, "text/plain", "Nowe SSID musi się różnić od aktualnego SSID.");
        return;
      }

      // Nadpisz nowe SSID w pliku konfiguracyjnym
      bool success = saveConfig("/config.json", "dev_ssid", newSSID);

      if (success) {
        request->send(200, "text/plain", "SSID zostało zmienione.");
      } else {
        request->send(500, "text/plain", "Błąd podczas zapisu pliku konfiguracyjnego.");
      }

      // Rozłącz z siecią Wi-Fi i zrestartuj moduł
      delay(100);
      ESP.restart();
    } else {
      request->send(SPIFFS, "/html/handle_localUser.html", "text/html");
    }
  });


  server.on("/reset", HTTP_POST, [this](AsyncWebServerRequest *request) {
    // Pobierz aktualną nazwę SSID
    if (isConnectedToESP(request)) {
      request->send(200, "text/plain", "Nastapi reset urzadzenia , upenij sie ze  siec sparowana z urzadzeniem jest już dostępna.");
      delay(100);
      WiFi.disconnect(true);
      ESP.restart();
    } else {
      request->send(SPIFFS, "/html/handle_localUser.html", "text/html");
    }
  });
  server.on("/NetWorkError", HTTP_GET, [this](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/html/NetWorkError.html", "text/html");
  });
    server.on("/test", HTTP_GET, [this](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/html/test.html", "text/html");
  });
     server.on("/Phase1_data.csv", HTTP_GET, [this](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/Phase1_data.csv", "text/html");
  });
}
