#include "SDLogger.h"

SDLogger::SDLogger(int miso, int mosi, int sclk, int cs) : _sdSPI(VSPI)
{
    _SD_MISO = miso;
    _SD_MOSI = mosi;
    _SD_SCLK = sclk;
    _SD_CS = cs;
}


void SDLogger::setup()
{     
    _sdSPI.begin(_SD_SCLK, _SD_MISO, _SD_MOSI, _SD_CS);    
    if(!SD.begin(_SD_CS, _sdSPI)) {
        Serial.println("Card Mount Failed");
        sdSucces=true;         
        return;
    }
    else {Serial.println("Card Success");}

    uint8_t cardType = SD.cardType();
    if(cardType == CARD_NONE) {
        Serial.println("No SD card attached");
        return;
    }

    Serial.println("Initializing SD card...");
    if (!SD.begin(_SD_CS)) {
        Serial.println("ERROR - SD card initialization failed!");
        return;
    }

    if (!SD.exists("/main_data")) {
        if (!SD.mkdir("/main_data")) {
            Serial.println("Failed to create main_data directory");
            // Dodatkowe działania, np. zatrzymanie programu, jeśli katalog nie może być utworzony
        } else {
            Serial.println("main_data directory created");
        }
    }

    // Other setup logic...
}
void SDLogger::createDirectory(const char *path) {
    if (!SD.exists(path)) {
        SD.mkdir(path);
    }
}
void SDLogger::logSDCard()
{
    Serial.print("Save data: ");
    Serial.println(_dataMessage);
    appendFile("/data1.txt", _dataMessage.c_str());
}
void SDLogger::clearSDCard() {
    // Usuwanie zawartości katalogu /main_data
    deleteDirectory("/main_data");

    // Reszta kodu do czyszczenia katalogu głównego
    File root = SD.open("/");
    if (!root) {
        Serial.println("Failed to open SD card");
        return;
    }
    if (!root.isDirectory()) {
        Serial.println("Not a directory");
        return;
    }

    File file = root.openNextFile();
    while (file) {
        if (!SD.remove(file.name())) {
            Serial.println("Failed to remove file");
        }
        file = root.openNextFile();
    }

    root.close();
    Serial.println("SD card cleared");
}

void SDLogger::deleteDirectory(const char *path) {
    File dir = SD.open(path);
    if (!dir) {
        Serial.println("Failed to open directory for deletion");
        return;
    }
    if (!dir.isDirectory()) {
        Serial.println("Not a directory");
        return;
    }

    File file = dir.openNextFile();
    while (file) {
        if (file.isDirectory()) {
            deleteDirectory(file.name());
        } else {
            if (!SD.remove(file.name())) {
                Serial.println("Failed to remove file");
            }
        }
        file = dir.openNextFile();
    }
    dir.close();

    if (!SD.rmdir(path)) {
        Serial.println("Failed to remove directory");
    }
}


void SDLogger::writeFile(const char *path, const String &message) {  
    String dir = String(path);
    int lastSlashIndex = dir.lastIndexOf('/');
    String directoryPath = dir.substring(0, lastSlashIndex);
    
    // Sprawdzenie, czy katalog istnieje
    if (!SD.exists(directoryPath.c_str())) {
        // Tworzenie katalogu, jeśli nie istnieje
        if (!SD.mkdir(directoryPath.c_str())) {
            Serial.println("Failed to create directory");
            return;
        }
    }

    // Serial.printf("Writing file: %s\n", path);
    File file = SD.open(path, FILE_APPEND);

    if (!file) {
        Serial.println("Failed to open file for writing");
        return;
    }

    if (file.print(message)) {
        Serial.println("File written");
    } else {
        Serial.println("Write failed");
    }

    file.close();

}




void SDLogger::appendFile(const char *path, const char *message)
{
   
    File file = SD.open(path, FILE_APPEND);
    if(!file) {
        Serial.println("Failed to open file for appending");
        return;
    }
    if(file.print(message)) {
        Serial.println("Message appended");
    } else {
        Serial.println("Append failed");
    }
    file.close();
}

void SDLogger::setMessage(String msg)
{
    _dataMessage = msg;
}
