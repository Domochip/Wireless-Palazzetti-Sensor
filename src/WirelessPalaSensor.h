#ifndef WirelessPalaSensor_h
#define WirelessPalaSensor_h

#include "Main.h"
#include "base\Utils.h"
#include "base\Application.h"

const char appDataPredefPassword[] PROGMEM = "ewcXoCt4HHjZUvY1";

#include "data\status1.html.gz.h"
#include "data\config1.html.gz.h"

#include <ESP8266HTTPClient.h>
#include <SPI.h>
#include <math.h>
#include <Ticker.h>
#include "SingleDS18B20.h"
#include "McpDigitalPot.h"

class WebPalaSensor : public Application
{
private:
  typedef struct
  {
    float rWTotal = 240.0; //TODO
    double steinhartHartCoeffs[3] = {0, 0, 0};
    float rBW5KStep = 19.0;   //TODO
    float rBW50KStep = 190.0; //TODO
    byte dp50kStepSize = 1;   //TODO
    byte dp5kOffset = 10;     //TODO
  } DigiPotsNTC;

  typedef struct
  {
    char apiKey[48 + 1] = {0};
  } Jeedom;

  typedef struct
  {
    char username[64 + 1] = {0};
    char password[64 + 1] = {0};
  } Fibaro;

  typedef struct
  {
    byte enabled = 0; //0 : no HA; 1 : Jeedom; 2 : Fibaro
    bool tls = false;
    byte fingerPrint[20] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    char hostname[64 + 1] = {0};
    int temperatureId = 0;
    Jeedom jeedom;
    Fibaro fibaro;
  } HomeAutomation;

  typedef struct
  {
    bool enabled = false;
    uint32_t ip = 0;
  } ConnectionBox;

  DigiPotsNTC digipotsNTC;
  HomeAutomation ha;
  ConnectionBox connectionBox;

  SingleDS18B20 _ds18b20;
  McpDigitalPot _mcp4151_5k;
  McpDigitalPot _mcp4151_50k;

  bool _needTick = false;
  Ticker _refreshTicker;
  byte _skipTick = 0;
  //Used in TimerTick for logic and calculation
  int _homeAutomationRequestResult = 0;
  float _homeAutomationTemperature = 0.0;
  int _homeAutomationFailedCount = 0;
  int _stoveRequestResult = 0;
  float _stoveTemperature = 0.0;
  int _stoveRequestFailedCount = 0;
  float _owTemperature = 0.0;
  bool _homeAutomationTemperatureUsed = false;
  float _stoveDelta = 0.0;
  float _pushedTemperature = 0.0;

  void SetDualDigiPot(float temperature);
  void SetDualDigiPot(int resistance);
  void SetDualDigiPot(unsigned int dp50kPosition, unsigned int dp5kPosition);
  void TimerTick();

  void SetConfigDefaultValues();
  void ParseConfigJSON(DynamicJsonDocument &doc);
  bool ParseConfigWebRequest(AsyncWebServerRequest *request);
  String GenerateConfigJSON(bool forSaveFile);
  String GenerateStatusJSON();
  bool AppInit(bool reInit);
  const uint8_t *GetHTMLContent(WebPageForPlaceHolder wp);
  size_t GetHTMLContentSize(WebPageForPlaceHolder wp);
  void AppInitWebServer(AsyncWebServer &server, bool &shouldReboot, bool &pauseApplication);
  void AppRun();

public:
  WebPalaSensor(char appId, String fileName);
};

#endif
