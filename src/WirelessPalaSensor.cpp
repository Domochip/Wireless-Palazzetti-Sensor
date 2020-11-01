#include "WirelessPalaSensor.h"

//-----------------------------------------------------------------------
// Steinhart–Hart reverse function
//-----------------------------------------------------------------------
void WebPalaSensor::setDualDigiPot(float temperature)
{
  //convert temperature from Celsius to Kevin degrees
  float temperatureK = temperature + 273.15;

  //calculate and return resistance value based on provided temperature
  double x = (1 / _digipotsNTC.steinhartHartCoeffs[2]) * (_digipotsNTC.steinhartHartCoeffs[0] - (1 / temperatureK));
  double y = sqrt(pow(_digipotsNTC.steinhartHartCoeffs[1] / (3 * _digipotsNTC.steinhartHartCoeffs[2]), 3) + pow(x / 2, 2));
  setDualDigiPot((int)(exp(pow(y - (x / 2), 1.0F / 3) - pow(y + (x / 2), 1.0F / 3))));
}
//-----------------------------------------------------------------------
// Set Dual DigiPot resistance (serial rBW)
//-----------------------------------------------------------------------
void WebPalaSensor::setDualDigiPot(int resistance)
{
  float adjustedResistance = resistance - _digipotsNTC.rWTotal - (_digipotsNTC.rBW5KStep * _digipotsNTC.dp5kOffset);

  //DigiPot positions calculation
  int digiPot50k_position = floor((adjustedResistance) / (_digipotsNTC.rBW50KStep * _digipotsNTC.dp50kStepSize)) * _digipotsNTC.dp50kStepSize;
  int digiPot5k_position = round((adjustedResistance - (digiPot50k_position * _digipotsNTC.rBW50KStep)) / _digipotsNTC.rBW5KStep);
  setDualDigiPot(digiPot50k_position, digiPot5k_position + _digipotsNTC.dp5kOffset);
}

void WebPalaSensor::setDualDigiPot(unsigned int dp50kPosition, unsigned int dp5kPosition)
{
  //Set DigiPot position
  if (_mcp4151_50k.getPosition(0) != dp50kPosition)
    _mcp4151_50k.setPosition(0, dp50kPosition);
  if (_mcp4151_5k.getPosition(0) != dp5kPosition)
    _mcp4151_5k.setPosition(0, dp5kPosition);
}

//-----------------------------------------------------------------------
// Main Timer Tick (aka this should be done every 30sec)
//-----------------------------------------------------------------------
void WebPalaSensor::timerTick()
{
  if (_skipTick)
  {
    _skipTick--;
    return;
  }

  float temperatureToDisplay = 20.0;
  float previousTemperatureToDisplay;
  if (_homeAutomationTemperatureUsed)
    previousTemperatureToDisplay = _homeAutomationTemperature;
  else
    previousTemperatureToDisplay = _owTemperature;
  _stoveTemperature = 0.0;
  _homeAutomationTemperature = 0.0;
  _homeAutomationTemperatureUsed = false;

  //LOG
  LOG_SERIAL.println(F("TimerTick"));

  //read temperature from the local sensor
  _owTemperature = _ds18b20.readTemp();
  if (_owTemperature == 12.3456F)
    _owTemperature = 20.0; //if reading of local sensor failed so push 20°C
  else
  {
    //round it to tenth
    _owTemperature *= 10;
    _owTemperature = round(_owTemperature);
    _owTemperature /= 10;
  }

  //if ConnectionBox option enabled in config and WiFi connected
  if (_connectionBox.enabled && WiFi.isConnected())
  {
    WiFiClient client;
    HTTPClient http;

    //set timeOut
    http.setTimeout(5000);

    //try to get current stove temperature info ----------------------
    http.begin(client, String(F("http://")) + IPAddress(_connectionBox.ip).toString() + F("/cgi-bin/sendmsg.lua?cmd=GET%20TMPS"));

    //send request
    _stoveRequestResult = http.GET();
    //if we get successfull HTTP answer
    if (_stoveRequestResult == 200)
    {
      WiFiClient *stream = http.getStreamPtr();

      //if we found T1 in answer
      if (stream->find("\"T1\""))
      {
        char payload[8];
        //read until the comma into payload variable
        int nb = stream->readBytesUntil(',', payload, sizeof(payload) - 1);
        payload[nb] = 0; //end payload char[]
        //if we readed some bytes
        if (nb)
        {
          //look for start position of T1 value
          byte posTRW = 0;
          while ((payload[posTRW] == ' ' || payload[posTRW] == ':' || payload[posTRW] == '\t') && posTRW < nb)
            posTRW++;

          _stoveTemperature = atof(payload + posTRW); //convert
        }
      }
    }
    http.end();
  }

  //if Jeedom option enabled in config and WiFi connected
  if (_ha.enabled == 1 && WiFi.isConnected())
  {
    WiFiClient client;
    WiFiClientSecure clientSecure;

    HTTPClient http;

    //set timeOut
    http.setTimeout(5000);

    //try to get house automation sensor value -----------------
    String completeURI = String(F("http")) + (_ha.tls ? F("s") : F("")) + F("://") + _ha.hostname + F("/core/api/jeeApi.php?apikey=") + _ha.jeedom.apiKey + F("&type=cmd&id=") + _ha.temperatureId;
    if (!_ha.tls)
      http.begin(client, completeURI);
    else
    {
      char fpStr[41];
      clientSecure.setFingerprint(Utils::fingerPrintA2S(fpStr, _ha.fingerPrint));
      http.begin(clientSecure, completeURI);
    }
    //send request
    _homeAutomationRequestResult = http.GET();
    //if we get successfull HTTP answer
    if (_homeAutomationRequestResult == 200)
    {
      WiFiClient *stream = http.getStreamPtr();

      //get the answer content
      char payload[6];
      int nb = stream->readBytes(payload, sizeof(payload) - 1);
      payload[nb] = 0;

      if (nb)
      {
        //convert
        _homeAutomationTemperature = atof(payload);
        //round it to tenth
        _homeAutomationTemperature *= 10;
        _homeAutomationTemperature = round(_homeAutomationTemperature);
        _homeAutomationTemperature /= 10;
      }
    }
    http.end();
  }

  //if Fibaro option enabled in config and WiFi connected
  if (_ha.enabled == 2 && WiFi.isConnected())
  {
    WiFiClient client;
    WiFiClientSecure clientSecure;
    HTTPClient http;

    //set timeOut
    http.setTimeout(5000);

    //try to get house automation sensor value -----------------
    String completeURI = String(F("http")) + (_ha.tls ? F("s") : F("")) + F("://") + _ha.hostname + F("/api/devices?id=") + _ha.temperatureId;
    //String completeURI = String(F("http")) + (_ha.tls ? F("s") : F("")) + F("://") + _ha.hostname + F("/devices.json");
    if (!_ha.tls)
      http.begin(client, completeURI);
    else
    {
      char fpStr[41];
      clientSecure.setFingerprint(Utils::fingerPrintA2S(fpStr, _ha.fingerPrint));
      http.begin(clientSecure, completeURI);
    }

    //Pass authentication if specified in configuration
    if (_ha.fibaro.username[0])
      http.setAuthorization(_ha.fibaro.username, _ha.fibaro.password);

    //send request
    _homeAutomationRequestResult = http.GET();

    //if we get successfull HTTP answer
    if (_homeAutomationRequestResult == 200)
    {
      WiFiClient *stream = http.getStreamPtr();

      while (http.connected() && stream->find("\"value\""))
      {
        //go to first next double quote (or return false if a comma appears first)
        if (stream->findUntil("\"", ","))
        {
          char payload[60];
          //read value (read until next doublequote)
          int nb = stream->readBytesUntil('"', payload, sizeof(payload) - 1);
          payload[nb] = 0;
          if (nb)
          {
            //convert
            _homeAutomationTemperature = atof(payload);
            //round it to tenth
            _homeAutomationTemperature *= 10;
            _homeAutomationTemperature = round(_homeAutomationTemperature);
            _homeAutomationTemperature /= 10;
          }
        }
      }
    }
    http.end();
  }

  //select temperature source
  if (_ha.enabled)
  {
    //if we got an HA temperature
    if (_homeAutomationTemperature > 0.1)
    {
      _homeAutomationFailedCount = 0;
      _homeAutomationTemperatureUsed = true;
      temperatureToDisplay = _homeAutomationTemperature;
    }
    else
    {
      //else if failed count is good and previousTemperature is good too
      if (_homeAutomationFailedCount < 11 && previousTemperatureToDisplay > 0.1)
      {
        _homeAutomationFailedCount++;
        _homeAutomationTemperatureUsed = true;
        _homeAutomationTemperature = previousTemperatureToDisplay;
        temperatureToDisplay = previousTemperatureToDisplay;
      }
      //otherwise fail back to oneWire
      else
        temperatureToDisplay = _owTemperature;
    }
  }
  else
    temperatureToDisplay = _owTemperature; //HA not enable

  //if connectionBox is enabled
  if (_connectionBox.enabled)
  {

    //if _stoveTemperature is correct so failed counter reset
    if (_stoveTemperature > 0.1)
      _stoveRequestFailedCount = 0;
    else if (_stoveRequestFailedCount < 6)
      _stoveRequestFailedCount++; //else increment failed counter

    //if failed counter reached 5 then reset calculated delta
    if (_stoveRequestFailedCount >= 5)
      _stoveDelta = 0;
    //if stoveTemp is ok and previousTemperatureToDisplay also so adjust delta
    if (_stoveTemperature > 0.1 && previousTemperatureToDisplay > 0.1)
      _stoveDelta += (previousTemperatureToDisplay - _stoveTemperature) / 2.5F;
  }

  //Set DigiPot position according to resistance calculated from temperature to display with delta
  setDualDigiPot(temperatureToDisplay + _stoveDelta);

  _pushedTemperature = temperatureToDisplay + _stoveDelta;
}

//------------------------------------------
//Used to initialize configuration properties to default values
void WebPalaSensor::setConfigDefaultValues()
{
  _digipotsNTC.rWTotal = 240.0;
  _digipotsNTC.steinhartHartCoeffs[0] = 0.001067860568;
  _digipotsNTC.steinhartHartCoeffs[1] = 0.0002269969431;
  _digipotsNTC.steinhartHartCoeffs[2] = 0.0000002641627999;
  _digipotsNTC.rBW5KStep = 19.0; //TODO
  _digipotsNTC.rBW50KStep = 190.0;
  _digipotsNTC.dp50kStepSize = 1;
  _digipotsNTC.dp5kOffset = 10; //TODO

  _ha.enabled = 0;
  _ha.tls = true;
  memset(_ha.fingerPrint, 0, 20);
  _ha.hostname[0] = 0;
  _ha.temperatureId = 0;
  _ha.jeedom.apiKey[0] = 0;
  _ha.fibaro.username[0] = 0;
  _ha.fibaro.password[0] = 0;

  _connectionBox.enabled = false;
  _connectionBox.ip = 0;
};
//------------------------------------------
//Parse JSON object into configuration properties
void WebPalaSensor::parseConfigJSON(DynamicJsonDocument &doc)
{
  if (!doc[F("sha")].isNull())
    _digipotsNTC.steinhartHartCoeffs[0] = doc[F("sha")];
  if (!doc[F("shb")].isNull())
    _digipotsNTC.steinhartHartCoeffs[1] = doc[F("shb")];
  if (!doc[F("shc")].isNull())
    _digipotsNTC.steinhartHartCoeffs[2] = doc[F("shc")];

  if (!doc[F("hae")].isNull())
    _ha.enabled = doc[F("hae")];
  if (!doc[F("hatls")].isNull())
    _ha.tls = doc[F("hatls")];
  if (!doc[F("hah")].isNull())
    strlcpy(_ha.hostname, doc["hah"], sizeof(_ha.hostname));
  if (!doc[F("hatid")].isNull())
    _ha.temperatureId = doc[F("hatid")];
  if (!doc["hafp"].isNull())
    Utils::fingerPrintS2A(_ha.fingerPrint, doc["hafp"]);

  if (!doc["ja"].isNull())
    strlcpy(_ha.jeedom.apiKey, doc["ja"], sizeof(_ha.jeedom.apiKey));

  if (!doc["fu"].isNull())
    strlcpy(_ha.fibaro.username, doc["fu"], sizeof(_ha.fibaro.username));
  if (!doc["fp"].isNull())
    strlcpy(_ha.fibaro.password, doc["fp"], sizeof(_ha.fibaro.password));

  if (!doc[F("cbe")].isNull())
    _connectionBox.enabled = doc[F("cbe")];
  if (!doc[F("cbi")].isNull())
    _connectionBox.ip = doc[F("cbi")];
};
//------------------------------------------
//Parse HTTP POST parameters in request into configuration properties
bool WebPalaSensor::parseConfigWebRequest(AsyncWebServerRequest *request)
{
  //Find Steinhart-Hart coeff then convert to double
  //AND handle scientific notation
  if (request->hasParam(F("sha"), true))
    _digipotsNTC.steinhartHartCoeffs[0] = request->getParam(F("sha"), true)->value().toFloat();
  if (request->hasParam(F("shb"), true))
    _digipotsNTC.steinhartHartCoeffs[1] = request->getParam(F("shb"), true)->value().toFloat();
  if (request->hasParam(F("shc"), true))
    _digipotsNTC.steinhartHartCoeffs[2] = request->getParam(F("shc"), true)->value().toFloat();

  if (request->hasParam(F("hae"), true))
    _ha.enabled = request->getParam(F("hae"), true)->value().toInt();
  //if an home Automation system is enabled then get common param
  if (_ha.enabled)
  {
    if (request->hasParam(F("hatls"), true))
      _ha.tls = (request->getParam(F("hatls"), true)->value() == F("on"));
    else
      _ha.tls = false;
    if (request->hasParam(F("hah"), true) && request->getParam(F("hah"), true)->value().length() < sizeof(_ha.hostname))
      strcpy(_ha.hostname, request->getParam(F("hah"), true)->value().c_str());
    if (request->hasParam(F("hatid"), true))
      _ha.temperatureId = request->getParam(F("hatid"), true)->value().toInt();
    if (request->hasParam(F("hafp"), true))
      Utils::fingerPrintS2A(_ha.fingerPrint, request->getParam(F("hafp"), true)->value().c_str());
  }

  //Now get specific param
  switch (_ha.enabled)
  {
  case 1: //Jeedom
    char tempApiKey[48 + 1];
    //put apiKey into temporary one for predefpassword
    if (request->hasParam(F("ja"), true) && request->getParam(F("ja"), true)->value().length() < sizeof(tempApiKey))
      strcpy(tempApiKey, request->getParam(F("ja"), true)->value().c_str());
    //check for previous apiKey (there is a predefined special password that mean to keep already saved one)
    if (strcmp_P(tempApiKey, appDataPredefPassword))
      strcpy(_ha.jeedom.apiKey, tempApiKey);
    if (!_ha.hostname[0] || !_ha.jeedom.apiKey[0])
      _ha.enabled = 0;
    break;
  case 2: //Fibaro
    char tempFibaroPassword[64 + 1];
    if (request->hasParam(F("fu"), true) && request->getParam(F("fu"), true)->value().length() < sizeof(_ha.fibaro.username))
      strcpy(_ha.fibaro.username, request->getParam(F("fu"), true)->value().c_str());
    if (request->hasParam(F("fp"), true) && request->getParam(F("fp"), true)->value().length() < sizeof(_ha.fibaro.password))
      strcpy(tempFibaroPassword, request->getParam(F("fp"), true)->value().c_str());
    //check for previous fibaro password (there is a predefined special password that mean to keep already saved one)
    if (strcmp_P(tempFibaroPassword, appDataPredefPassword))
      strcpy(_ha.fibaro.password, tempFibaroPassword);
    if (!_ha.hostname[0])
      _ha.enabled = 0;
    break;
  }

  if (request->hasParam(F("cbe"), true))
    _connectionBox.enabled = (request->getParam(F("cbe"), true)->value() == F("on"));
  else
    _connectionBox.enabled = false;

  if (request->hasParam(F("cbi"), true))
  {
    IPAddress ipParser;
    if (ipParser.fromString(request->getParam(F("cbi"), true)->value()))
      _connectionBox.ip = static_cast<uint32_t>(ipParser);
    else
      _connectionBox.ip = 0;
  }

  return true;
};
//------------------------------------------
//Generate JSON from configuration properties
String WebPalaSensor::generateConfigJSON(bool forSaveFile = false)
{
  String gc('{');

  char fpStr[60];

  gc = gc + F("\"sha\":") + String(_digipotsNTC.steinhartHartCoeffs[0], 16);
  gc = gc + F(",\"shb\":") + String(_digipotsNTC.steinhartHartCoeffs[1], 16);
  gc = gc + F(",\"shc\":") + String(_digipotsNTC.steinhartHartCoeffs[2], 16);

  gc = gc + F(",\"hae\":") + _ha.enabled;
  gc = gc + F(",\"hatls\":") + _ha.tls;
  gc = gc + F(",\"hah\":\"") + _ha.hostname + '"';
  gc = gc + F(",\"hatid\":") + _ha.temperatureId;
  gc = gc + F(",\"hafp\":\"") + Utils::fingerPrintA2S(fpStr, _ha.fingerPrint, ':') + '"';
  if (forSaveFile)
  {
    if (_ha.enabled == 1)
      gc = gc + F(",\"ja\":\"") + _ha.jeedom.apiKey + '"';
  }
  else
    //Jeedom apiKey: there is a predefined special password (mean to keep already saved one)
    gc = gc + F(",\"ja\":\"") + (__FlashStringHelper *)appDataPredefPassword + '"';

  gc = gc + F(",\"fu\":\"") + _ha.fibaro.username + '"';
  if (forSaveFile)
  {
    if (_ha.enabled == 2)
      gc = gc + F(",\"fp\":\"") + _ha.fibaro.password + '"';
  }
  else
    //Fibaro password : there is a predefined special password (mean to keep already saved one)
    gc = gc + F(",\"fp\":\"") + (__FlashStringHelper *)appDataPredefPassword + '"';

  gc = gc + F(",\"cbe\":") + _connectionBox.enabled;
  if (forSaveFile)
    gc = gc + F(",\"cbi\":") + _connectionBox.ip;
  else if (_connectionBox.ip)
    gc = gc + F(",\"cbi\":\"") + IPAddress(_connectionBox.ip).toString() + '"';

  gc += '}';

  return gc;
};
//------------------------------------------
//Generate JSON of application status
String WebPalaSensor::generateStatusJSON()
{
  String gs('{');

  //Home Automation infos
  if (_ha.enabled)
  {
    gs = gs + F("\"lhar\":") + _homeAutomationRequestResult;
    gs = gs + F(",\"lhat\":") + _homeAutomationTemperature;
    gs = gs + F(",\"hafc\":") + _homeAutomationFailedCount;
  }
  else
    gs = gs + F("\"lhar\":\"NA\",\"lhat\":\"NA\",\"hafc\":\"NA\"");

  //stove(ConnectionBox) infos
  if (_connectionBox.enabled)
  {
    gs = gs + F(",\"lcr\":") + _stoveRequestResult;
    gs = gs + F(",\"lct\":") + _stoveTemperature;
    gs = gs + F(",\"cfc\":") + _stoveRequestFailedCount;
  }
  else
    gs = gs + F(",\"lcr\":\"NA\",\"lct\":\"NA\",\"cfc\":\"NA\"");

  gs = gs + F(",\"low\":") + _owTemperature;
  gs = gs + F(",\"owu\":\"") + (_homeAutomationTemperatureUsed ? F("Not ") : F("")) + '"';
  gs = gs + F(",\"pt\":") + _pushedTemperature;
  gs = gs + F(",\"p50\":") + _mcp4151_50k.getPosition(0);
  gs = gs + F(",\"p5\":") + _mcp4151_5k.getPosition(0);

  gs += '}';

  return gs;
};
//------------------------------------------
//code to execute during initialization and reinitialization of the app
bool WebPalaSensor::appInit(bool reInit)
{
  //stop Ticker
  _refreshTicker.detach();

  if (reInit)
  {
    //reset run variables to initial values
    _homeAutomationRequestResult = 0;
    _homeAutomationTemperature = 0.0;
    _homeAutomationFailedCount = 0;
    _stoveRequestResult = 0;
    _stoveTemperature = 0.0;
    _stoveRequestFailedCount = 0;
    _owTemperature = 0.0;
    _homeAutomationTemperatureUsed = false;
    _stoveDelta = 0.0;
    _pushedTemperature = 0.0;
  }

  //first call
  timerTick();

  //then next will be done by refreshTicker
  _refreshTicker.attach_scheduled(REFRESH_PERIOD, [this]() { this->_needTick = true; });

  return _ds18b20.getReady();
};
//------------------------------------------
//Return HTML Code to insert into Status Web page
const uint8_t *WebPalaSensor::getHTMLContent(WebPageForPlaceHolder wp)
{
  switch (wp)
  {
  case status:
    return (const uint8_t *)status1htmlgz;
    break;
  case config:
    return (const uint8_t *)config1htmlgz;
    break;
  default:
    return nullptr;
    break;
  };
  return nullptr;
};
//and his Size
size_t WebPalaSensor::getHTMLContentSize(WebPageForPlaceHolder wp)
{
  switch (wp)
  {
  case status:
    return sizeof(status1htmlgz);
    break;
  case config:
    return sizeof(config1htmlgz);
    break;
  default:
    return 0;
    break;
  };
  return 0;
};
//------------------------------------------
//code to register web request answer to the web server
void WebPalaSensor::appInitWebServer(AsyncWebServer &server, bool &shouldReboot, bool &pauseApplication)
{
  server.on("/calib.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse_P(200, F("text/html"), (const uint8_t *)calibhtmlgz, sizeof(calibhtmlgz));
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
  });

  //GetDigiPot
  server.on("/gdp", HTTP_GET, [this](AsyncWebServerRequest *request) {
    String dpJSON('{');
    dpJSON = dpJSON + F("\"r\":") + (_mcp4151_50k.getPosition(0) * _digipotsNTC.rBW50KStep + _mcp4151_5k.getPosition(0) * _digipotsNTC.rBW5KStep + _digipotsNTC.rWTotal);
#if DEVELOPPER_MODE
    dpJSON = dpJSON + F(",\"dp5k\":") + _mcp4151_5k.getPosition(0);
    dpJSON = dpJSON + F(",\"dp50k\":") + _mcp4151_50k.getPosition(0);
#endif
    dpJSON += '}';

    request->send(200, F("text/json"), dpJSON);
  });

  //SetDigiPot
  server.on("/sdp", HTTP_POST, [this](AsyncWebServerRequest *request) {

#define TICK_TO_SKIP 20
    //look for temperature to apply
    if (request->hasParam(F("temperature"), true))
    {
      //convert and set it
      setDualDigiPot(request->getParam(F("temperature"), true)->value().toFloat());
      //go for timer tick skipped (time to look a value on stove)
      _skipTick = TICK_TO_SKIP;
    }

    //look for increase of digipots
    if (request->hasParam(F("up"), true))
    {
      //go one step up
      setDualDigiPot((int)(_mcp4151_50k.getPosition(0) * _digipotsNTC.rBW50KStep + _mcp4151_5k.getPosition(0) * _digipotsNTC.rBW5KStep + _digipotsNTC.rWTotal + _digipotsNTC.rBW5KStep));
      //go for timer tick skipped (time to look a value on stove)
      _skipTick = TICK_TO_SKIP;
    }

    //look for decrease of digipots
    if (request->hasParam(F("down"), true))
    {
      //go one step down
      setDualDigiPot((int)(_mcp4151_50k.getPosition(0) * _digipotsNTC.rBW50KStep + _mcp4151_5k.getPosition(0) * _digipotsNTC.rBW5KStep + _digipotsNTC.rWTotal - _digipotsNTC.rBW5KStep));
      //go for timer tick skipped (time to look a value on stove)
      _skipTick = TICK_TO_SKIP;
    }

#if DEVELOPPER_MODE
    //look for 5k digipot requested position
    if (request->hasParam(F("dp5k"), true))
    {
      //convert and set it
      _mcp4151_5k.setPosition(0, request->getParam(F("dp5k"), true)->value().toInt());
      //go for timer tick skipped (time to look a value on stove)
      _skipTick = TICK_TO_SKIP;
    }

    //look for 50k digipot requested position
    if (request->hasParam(F("dp50k"), true))
    {
      //convert and set it
      _mcp4151_50k.setPosition(0, request->getParam(F("dp50k"), true)->value().toInt());
      //go for timer tick skipped (time to look a value on stove)
      _skipTick = TICK_TO_SKIP;
    }

    //look for resistance to apply
    if (request->hasParam(F("resistance"), true))
    {
      //convert resistance value and call right function
      setDualDigiPot(0, request->getParam(F("resistance"), true)->value().toInt());
      //go for timer tick skipped (time to look a value on stove)
      _skipTick = TICK_TO_SKIP;
    }
#endif

    request->send(200);
  });
};

//------------------------------------------
//Run for timer
void WebPalaSensor::appRun()
{
  if (_needTick)
  {
    //disable needTick
    _needTick = false;
    //then run
    timerTick();
  }
}

//------------------------------------------
//Constructor
WebPalaSensor::WebPalaSensor(char appId, String appName) : Application(appId, appName), _ds18b20(ONEWIRE_BUS_PIN), _mcp4151_5k(MCP4151_5k_SSPIN), _mcp4151_50k(MCP4151_50k_SSPIN)
{
  //Init SPI for DigiPot
  SPI.begin();
  //Init DigiPots @20°C
  _mcp4151_50k.setPosition(0, 61);
  _mcp4151_5k.setPosition(0, 5);
}