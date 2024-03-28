#ifndef SDLogger_h
#define SDLogger_h

#include "Arduino.h"
#include "FS.h"
#include <SD.h>
#include <SPI.h>

class SDLogger
{
public:
    void compressFile(const char *sourcePath, const char *destPath);
    void deleteDirectory(const char *path);
    void clearSDCard();
    SDLogger(int miso, int mosi, int sclk, int cs);
    void setup();
    void logSDCard();
    void writeFile(const char *path, const String &message);
    void appendFile(const char *path, const char *message);
    void setMessage(String msg);
    void createDirectory(const char *path);
    bool sdSucces=false; 
private:
    SPIClass _sdSPI;
    int _SD_MISO, _SD_MOSI, _SD_SCLK, _SD_CS;
    String _dataMessage;
   
};

#endif