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
  // -------------------- DigiPots Classes--------------------
  typedef struct
  {
    float rWTotal = 0;
    double steinhartHartCoeffs[3] = {0, 0, 0};
    float rBW5KStep = 0;
    float rBW50KStep = 0;
    byte dp50kStepSize = 0;
    byte dp5kOffset = 0;
  } DigiPotsNTC;

  // -------------------- HomeAutomation Classes --------------------

#define HA_HTTP_JEEDOM 0
#define HA_HTTP_FIBARO 1

  typedef struct
  {
    byte type = HA_HTTP_JEEDOM;
    bool tls = false;
    byte fingerPrint[20] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    //Ids of light indicator in Home Automation
    int temperatureId = 0;

    struct
    {
      char apiKey[48 + 1] = {0};
    } jeedom;

    struct
    {
      char username[64 + 1] = {0};
      char password[64 + 1] = {0};
    } fibaro;
  } HTTP;

#define HA_PROTO_DISABLED 0
#define HA_PROTO_HTTP 1
#define HA_PROTO_MQTT 2

  typedef struct
  {
    byte protocol = HA_PROTO_DISABLED;
    char hostname[64 + 1] = {0};
    HTTP http;
  } HomeAutomation;

  // -------------------- ConnectionBox Classes --------------------

  typedef struct
  {
    uint32_t ip = 0;
  } CBoxHTTP;

#define CBOX_PROTO_DISABLED 0
#define CBOX_PROTO_HTTP 1
#define CBOX_PROTO_MQTT 2

  typedef struct
  {
    bool protocol = CBOX_PROTO_DISABLED;
    CBoxHTTP cboxhttp;
  } ConnectionBox;

  // --------------------

  DigiPotsNTC _digipotsNTC;
  HomeAutomation _ha;
  ConnectionBox _connectionBox;

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

  WiFiClient _wifiClient;
  WiFiClientSecure _wifiClientSecure;

  void setDualDigiPot(float temperature);
  void setDualDigiPot(int resistance);
  void setDualDigiPot(unsigned int dp50kPosition, unsigned int dp5kPosition);
  void timerTick();

  void setConfigDefaultValues();
  void parseConfigJSON(DynamicJsonDocument &doc);
  bool parseConfigWebRequest(AsyncWebServerRequest *request);
  String generateConfigJSON(bool forSaveFile);
  String generateStatusJSON();
  bool appInit(bool reInit);
  const uint8_t *getHTMLContent(WebPageForPlaceHolder wp);
  size_t getHTMLContentSize(WebPageForPlaceHolder wp);
  void appInitWebServer(AsyncWebServer &server, bool &shouldReboot, bool &pauseApplication);
  void appRun();

public:
  WebPalaSensor(char appId, String fileName);
};

#endif
