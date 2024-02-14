#include <MD_Parola.h>
#include "ASCII_Font_Data.h"
#include "Numeric7seg_Font_data.h"
#include "invader_Font.h"

#include <WiFiManager.h>
#include <ArduinoOTA.h>
#include <WiFiUdp.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <WiFiClient.h>

#include <ArduinoJson.h>
#define ARDUINOTRACE_ENABLE 1  
#include <ArduinoTrace.h>

#include <JsonListener.h>
#include <JsonStreamingParser.h>

#include <uptime.h>
#include <uptime_formatter.h>
#include <Timezone.h>

#include "secrets.h"

#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 8
#define CLK_PIN   D8 // or SCK
#define DATA_PIN  D7 // or MOSI
#define CS_PIN    D5 // or SS

// SOFTWARE SPI
MD_Parola P = MD_Parola(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);

#define PAUSE_TIME 0
#define SPEED_TIME 60

char timeBuffer[24];
#define SCROLL_BUFFER_SIZE 256
char scrollBuffer[SCROLL_BUFFER_SIZE]; // maybe a bit large...

uint8_t degC[] = { 6, 3, 3, 56, 68, 68, 68 };	 // Deg C
uint8_t euro[] = { 6, 20, 62, 85, 85, 65, 34 };  // 'Euro sign'
uint8_t bitcoin[] = { 5, 127, 73, 255, 73, 54 };
uint8_t insta[] = { 8, 126, 129, 153, 165, 165, 153, 129, 126 }; // Instagram icon
uint8_t ethereum[] = { 5, 73, 73, 73, 73, 73 };

typedef void (*DISPLAYFUN)(char* destination);

// Central European Time (Frankfurt, Paris)
TimeChangeRule CEST = {"CEST", Last, Sun, Mar, 2, 120};     // Central European Summer Time
TimeChangeRule CET = {"CET ", Last, Sun, Oct, 3, 60};       // Central European Standard Time
Timezone CE(CEST, CET);
TimeChangeRule *tcr;

void getTime(bool flasher) {
  uint16_t	h, m;
  time_t utc = now(); // triggers update if necessary
  time_t local = CE.toLocal(utc, &tcr);
  m = minute(local);
  h = hour(local);
  uint8_t m1 = m / 10;
  uint8_t m2 = m % 10;
  uint8_t h1 = h / 10;
  uint8_t h2 = h % 10;
  sprintf(timeBuffer, " %d %d %c %d %d", h1, h2, (flasher ? ':' : ';'), m1, m2);
}

void initOTA() {
  ArduinoOTA.setHostname("WemosTicker");
  ArduinoOTA.setPassword("wemos");
  ArduinoOTA.onStart([]() {
    P.begin();
    P.setIntensity(0);
  });
  ArduinoOTA.onEnd([]() {});
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    unsigned int realprogress = (progress / (total / 100));
    char buffer[16];
    sprintf(buffer, "OTA: %.2d %%", realprogress);
    P.print(buffer);
  });
  ArduinoOTA.onError([](ota_error_t) {});
  ArduinoOTA.begin();
}

void initOWM();

unsigned int localPort = 2390;      // local port to listen for UDP packets
IPAddress timeServerIP; // time.nist.gov NTP server address
const char* ntpServerName = "time.google.com";
const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets

WiFiUDP udp;

void sendNTPpacket(IPAddress& address) {
  if (WiFi.isConnected()) {
     memset(packetBuffer, 0, NTP_PACKET_SIZE);
    packetBuffer[0] = 0b11100011;   // LI, Version, Mode
    udp.beginPacket(address, 123); //NTP requests are to port 123
    udp.write(packetBuffer, NTP_PACKET_SIZE);
    udp.endPacket();
  }
}

time_t getNtpTime() {
  WiFi.hostByName(ntpServerName, timeServerIP);
  while (udp.parsePacket() > 0) ; // discard any previously received packets
  sendNTPpacket(timeServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL;
    }
  }
  return 0; // return 0 if unable to get the time
}

ESP8266WebServer webServer(80);       // Create a webserver object that listens for HTTP request on port 80

char textBuffer[128] = "";
char defaultTextBuffer[128] = "Walt \x26 Lip sont deux gentils chats (en g\xE9n\xE9ral)";

const char* days[] = { "Dimanche", "Lundi", "Mardi", "Mercredi", "Jeudi", "Vendredi", "Samedi"  };
const char* monthes[] = { "Janvier", "F\xE9vrier", "Mars", "Avril", "Mai", "Juin", "Juillet",
                          "Ao\xFBt", "Septembre", "Octobre", "Novembre", "D""\xE9""cembre"
                        };

void initText() {
  strcpy(textBuffer, defaultTextBuffer);
}

uint16_t defaultSpeed = 35;
uint16_t speed = defaultSpeed;

uint8_t defaultIntensity = 0;
uint8_t intensity = defaultIntensity;

void setParolaSpeed(uint16_t value) {
  speed = value;
  P.setSpeed(speed);
}

void decParolaSpeed() {
  setParolaSpeed(speed + 5);
}

void incParolaSpeed() {
  setParolaSpeed(speed > 10 ? speed - 5 : speed);
}

void setParolaIntensity(uint8_t value) {
  intensity = constrain(value, 0, 15);
  P.setIntensity(0, intensity); // 0 to 15 for red
  P.setIntensity(1, intensity + 3); // green is dimmer ?
}

// ticker.local
String dnsname("wemosTicker");

void setup(void) {
  Serial.begin(115200);
  P.begin();
  setParolaIntensity(0);
  P.print("Wifi...");

  WiFiManager wifiManager;
  wifiManager.setConfigPortalTimeout(120);
  wifiManager.setDebugOutput(true);
  wifiManager.setMinimumSignalQuality(30);
  wifiManager.autoConnect("ticker"); //  no password

  strcpy(timeBuffer, "--:--");
  strcpy(scrollBuffer, "Fred");
  if (WiFi.isConnected()) {
     WiFi.setAutoReconnect (true);
    WiFi.onStationModeDisconnected([](const WiFiEventStationModeDisconnected & event) {
     });
    WiFi.onStationModeGotIP([](const WiFiEventStationModeGotIP&) {
    });

    initOTA();
    initOWM();

    MDNS.begin(dnsname.c_str());
    udp.begin(localPort);

    setSyncProvider(getNtpTime);
    setSyncInterval(3600 * 10); // every 10 hours

    String ipinfo = WiFi.localIP().toString() + String(" (") + dnsname + String(".local)");
    strcpy(scrollBuffer, ipinfo.c_str());

    // REST APIs

    // returns all parameters as JSON
    webServer.on("/", HTTP_GET, []() {
      JsonDocument doc;
      doc["uptime"] = uptime_formatter::getUptime();
      doc["speed"] = speed;
      doc["text"] = textBuffer;
      doc["intensity"] = intensity;
      std::string json;
      serializeJsonPretty(doc, json);
      json += "\n\n";
//      json += "/speed?value=5-100   lower is faster - no value resets to default (" + std::to_string(defaultSpeed) + ")\n";
      json += "/incspeed\n";
      json += "/decspeed\n";
//      json += "/intensity?value=0-15 lower is darker - no value resets to default (" + std::to_string(defaultIntensity) + ")\n";
      json += "/text?value=TEXT      no value resets to default\n";
      webServer.send(200, "text/plain", json.c_str());
    });

    // speed?value=5
    webServer.on("/speed", HTTP_GET, []() {
      if (webServer.hasArg("value")) {
        String value = webServer.arg("value");
        setParolaSpeed((uint16_t)value.toInt());
      } else {
        setParolaSpeed(defaultSpeed);
      }
      webServer.send(200, "text/plain", String(speed));
    });

    webServer.on("/incspeed", HTTP_GET, []() {
      incParolaSpeed();
      webServer.send(200, "text/plain", String(speed));
    });
    webServer.on("/decspeed", HTTP_GET, []() {
      decParolaSpeed();
      webServer.send(200, "text/plain", String(speed));
    });

    // intensity?value=5
    webServer.on("/intensity", HTTP_GET, []() {
      if (webServer.hasArg("value")) {
        String value = webServer.arg("value");
        setParolaIntensity(value.toInt());
      } else {
        setParolaIntensity(defaultIntensity);
      }
      webServer.send(200, "text/plain", String(intensity));
    });
    // text?value=text
    // text to reset
    webServer.on("/text", HTTP_GET, []() {
      if (webServer.hasArg("value")) {
        strcpy(textBuffer, webServer.arg("value").c_str());
      } else {
        initText();
      }
      webServer.send(200, "text/plain", String(textBuffer));
    });
    webServer.begin();
  }

  // Parola object
  P.begin(2);

  P.setZone(0, 0, MAX_DEVICES - 4); // scrolling text
  P.setZone(1, MAX_DEVICES - 3, MAX_DEVICES - 1); // time

  setParolaSpeed(speed);
  setParolaIntensity(intensity);

  P.setFont(0, ExtASCII); // all utf8 chars
  P.setFont(1, numeric7Se);
  P.setCharSpacing(1, 0);
  P.setCharSpacing(0, 1);

  P.displayZoneText(1, timeBuffer, PA_LEFT, speed, PAUSE_TIME, PA_PRINT, PA_NO_EFFECT);
  P.displayZoneText(0, scrollBuffer, PA_LEFT, speed, PAUSE_TIME, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
  //P.displayZoneText(0, scrollBuffer, PA_CENTER, SPEED_TIME, 0, PA_SPRITE, PA_SPRITE);
  //P.setSpriteData(0, invader, W_INVADER, F_INVADER, invader, W_INVADER, F_INVADER);

  P.addChar(0, '$', degC);
  P.addChar(0, '^', euro);
  P.addChar(0, '#', bitcoin);
  P.addChar(0, ';', insta);
  P.addChar(0, '|', ethereum);

  initText();
}

void displayDate(char* destination) {
  time_t utc = now(); // triggers update if necessary
  time_t local = CE.toLocal(utc, &tcr);
  int wd = weekday(local);
  int d = day(local);
  int m = month(local);
  sprintf(destination, "%s %d %s", days[wd - 1], d, monthes[m - 1]);
}

String getHttpsContents(const char* host, const char* rest, const char* header = 0, const char* fingerprint = 0);

WiFiClient wclient;

struct WeatherInfo {
  float temperature = -100;
  char description[32] = { 0 };
  char fulldescription[64] = { 0 };
};

struct WeatherInfo OWMInfo;

const char* owmArcueilId = "6613168";
const char* owmLang      = "fr"; // fr encodes accents in utf8, not easy to convert

char owmURL[128];

void initOWM() {
  sprintf(owmURL,
          "http://api.openweathermap.org/data/2.5/weather?id=%s&lang=%s&appid=%s&units=metric",
          owmArcueilId,
          owmLang,
          owmApiKey);
  OWMInfo.temperature = -100;
}

/* UTF-8 to ISO-8859-1/ISO-8859-15 mapper.
   Return 0..255 for valid ISO-8859-15 code points, 256 otherwise.
*/
static inline unsigned int to_latin9(const unsigned int code) {
  /* Code points 0 to U+00FF are the same in both. */
  if (code < 256U)
    return code;
  switch (code) {
    case 0x0152U: return 188U; /* U+0152 = 0xBC: OE ligature */
    case 0x0153U: return 189U; /* U+0153 = 0xBD: oe ligature */
    case 0x0160U: return 166U; /* U+0160 = 0xA6: S with caron */
    case 0x0161U: return 168U; /* U+0161 = 0xA8: s with caron */
    case 0x0178U: return 190U; /* U+0178 = 0xBE: Y with diaresis */
    case 0x017DU: return 180U; /* U+017D = 0xB4: Z with caron */
    case 0x017EU: return 184U; /* U+017E = 0xB8: z with caron */
    case 0x20ACU: return 164U; /* U+20AC = 0xA4: Euro */
    default:      return 256U;
  }
}

/* Convert an UTF-8 string to ISO-8859-15.
   All invalid sequences are ignored.
   Note: output == input is allowed,
   but   input < output < input + length
   is not.
   Output has to have room for (length+1) chars, including the trailing NUL byte.
*/
size_t utf8_to_latin9(char *const output, const char *const input, const size_t length) {
  unsigned char             *out = (unsigned char *)output;
  const unsigned char       *in  = (const unsigned char *)input;
  const unsigned char *const end = (const unsigned char *)input + length;
  unsigned int               c;

  while (in < end)
    if (*in < 128)
      *(out++) = *(in++); /* Valid codepoint */
    else if (*in < 192)
      in++;               /* 10000000 .. 10111111 are invalid */
    else if (*in < 224) {       /* 110xxxxx 10xxxxxx */
      if (in + 1 >= end)
        break;
      if ((in[1] & 192U) == 128U) {
        c = to_latin9( (((unsigned int)(in[0] & 0x1FU)) << 6U)
                       |  ((unsigned int)(in[1] & 0x3FU)) );
        if (c < 256)
          *(out++) = c;
      }
      in += 2;

    } else if (*in < 240) {       /* 1110xxxx 10xxxxxx 10xxxxxx */
      if (in + 2 >= end)
        break;
      if ((in[1] & 192U) == 128U &&
          (in[2] & 192U) == 128U) {
        c = to_latin9( (((unsigned int)(in[0] & 0x0FU)) << 12U)
                       | (((unsigned int)(in[1] & 0x3FU)) << 6U)
                       |  ((unsigned int)(in[2] & 0x3FU)) );
        if (c < 256)
          *(out++) = c;
      }
      in += 3;

    } else if (*in < 248) {       /* 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx */
      if (in + 3 >= end)
        break;
      if ((in[1] & 192U) == 128U &&
          (in[2] & 192U) == 128U &&
          (in[3] & 192U) == 128U) {
        c = to_latin9( (((unsigned int)(in[0] & 0x07U)) << 18U)
                       | (((unsigned int)(in[1] & 0x3FU)) << 12U)
                       | (((unsigned int)(in[2] & 0x3FU)) << 6U)
                       |  ((unsigned int)(in[3] & 0x3FU)) );
        if (c < 256)
          *(out++) = c;
      }
      in += 4;

    } else if (*in < 252) {       /* 111110xx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx */
      if (in + 4 >= end)
        break;
      if ((in[1] & 192U) == 128U &&
          (in[2] & 192U) == 128U &&
          (in[3] & 192U) == 128U &&
          (in[4] & 192U) == 128U) {
        c = to_latin9( (((unsigned int)(in[0] & 0x03U)) << 24U)
                       | (((unsigned int)(in[1] & 0x3FU)) << 18U)
                       | (((unsigned int)(in[2] & 0x3FU)) << 12U)
                       | (((unsigned int)(in[3] & 0x3FU)) << 6U)
                       |  ((unsigned int)(in[4] & 0x3FU)) );
        if (c < 256)
          *(out++) = c;
      }
      in += 5;

    } else if (*in < 254) {       /* 1111110x 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx */
      if (in + 5 >= end)
        break;
      if ((in[1] & 192U) == 128U &&
          (in[2] & 192U) == 128U &&
          (in[3] & 192U) == 128U &&
          (in[4] & 192U) == 128U &&
          (in[5] & 192U) == 128U) {
        c = to_latin9( (((unsigned int)(in[0] & 0x01U)) << 30U)
                       | (((unsigned int)(in[1] & 0x3FU)) << 24U)
                       | (((unsigned int)(in[2] & 0x3FU)) << 18U)
                       | (((unsigned int)(in[3] & 0x3FU)) << 12U)
                       | (((unsigned int)(in[4] & 0x3FU)) << 6U)
                       |  ((unsigned int)(in[5] & 0x3FU)) );
        if (c < 256)
          *(out++) = c;
      }
      in += 6;

    } else
      in++;               /* 11111110 and 11111111 are invalid */

  /* Terminate the output string. */
  *out = '\0';
  return (size_t)(out - (unsigned char *)output);
}

void displayHexString(const char* string) {
  int len = strlen(string);
  for (int idx = 0; idx < len; ++idx) {
    Serial.print(string[idx], HEX);
    Serial.print(" ");
    Serial.print(string[idx]);
    Serial.print(" ");
  }
  Serial.println();
}

unsigned long lastOWMQuery = 0;

void getOWMInfo() {
  if ((lastOWMQuery == 0) || ((millis() - lastOWMQuery) >= 6 * 60 * 1000)) { // every 6 mn
    lastOWMQuery = millis();
    if (WiFi.isConnected()) {
      HTTPClient owm;
      owm.begin(wclient, owmURL);
      if (owm.GET()) {
        String json = owm.getString();
        DUMP(json);
        DUMP(json.length());
        JsonDocument doc;
        auto result = deserializeJson(doc, json);
        if (!result) {
          float temp = doc["main"]["temp"];
          OWMInfo.temperature = round(temp);
          JsonObject weather = doc["weather"][0];
          strcpy(OWMInfo.description, weather["main"]);
          char utf8buf[64];
          strcpy(utf8buf, weather["description"]);
          utf8_to_latin9(OWMInfo.fulldescription, utf8buf, strlen(utf8buf));
          //      displayHexString(OWMInfo.fulldescription);
          DUMP(OWMInfo.temperature);
        }
      }
      owm.end();
    }
  }
}

void displayTemp(char* destination) {
  getOWMInfo();
  if (OWMInfo.temperature != -100) {
    sprintf(destination, "Temp\xE9rature ext\xE9rieure : %d$", (int)OWMInfo.temperature);
  } else {
    strcpy(destination, "");
  }
}

void displayDescription(char* destination) {
  getOWMInfo();
  if (OWMInfo.fulldescription[0] != 0) {
    sprintf(destination, "%s", OWMInfo.fulldescription);
    destination[0] = toupper(destination[0]);
  } else {
    strcpy(destination, "");
  }
}

const char* cryptoHOST = "api.coingecko.com";
const char* cryptoREST = "/api/v3/simple/price?ids=bitcoin,ethereum&vs_currencies=eur&include_24hr_change=true";
int btcValue = -1;
int ethValue = -1;
float btcChange;
float ethChange;

unsigned long lastCryptoQuery = 0;

void getCryptos() {
  if ((lastCryptoQuery == 0) || ((millis() - lastCryptoQuery) >= 4 * 60 * 1000)) { // every 4 mn
    lastCryptoQuery = millis();
   if (WiFi.isConnected()) {
      String json = getHttpsContents(cryptoHOST, cryptoREST).substring(2);
//      DUMP(json);
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, json);
      if (error) {
        DUMP(error.f_str());
      } else {
          btcValue = (int)doc["bitcoin"]["eur"];
          ethValue = (int)doc["ethereum"]["eur"];
          btcChange = doc["bitcoin"]["eur_24h_change"]; 
          ethChange = doc["ethereum"]["eur_24h_change"]; 
        } 
    } 
  } 
}

void displayCryptos(char* destination) {
  getCryptos();
  if (btcValue != -1) {
    sprintf(destination, "#itcoin : %d%c   |thereum : %d%c", btcValue, btcChange >= 0 ? 24 : 25, ethValue, ethChange >= 0 ? 24 : 25);
  } else {
    strcpy(destination, "");
  }
}
void displayAllChars(char* destination) {
  uint16_t idx;
  for (idx = 32; idx < SCROLL_BUFFER_SIZE - 1; ++idx) {
    destination[idx] = idx;
  }
  destination[idx] = 0;
}


#define RESP_BUFFER_LENGTH 256
uint8_t _buffer[RESP_BUFFER_LENGTH];

String getHttpsContents(const char* host, const char* rest, const char* header, const char* fingerprint) {
  DUMP(host);
  DUMP(rest);
  DUMP(header);
  DUMP(fingerprint);
  String contents;
  WiFiClientSecure secureClient;
  //      secureClient.setTimeout(50);
  if (fingerprint)
    secureClient.setFingerprint(fingerprint);
  else
    secureClient.setInsecure();
  if (secureClient.connect(host, 443)) { // very slow ! > 2 seconds
    TRACE();
      secureClient.print(String("GET ") + rest +
                       " HTTP/1.1\r\n" +
                       "Host: " + host + "\n" +
                       "User-Agent: arduino/1.0.0\n" +
                       (header != 0 ? header : "") + "\n" +
                       "Connection: close\n\n");
                      
    TRACE();
    uint16_t tentatives = 0;
    while (!secureClient.available()) {
       delay(50);
       tentatives++;
       DUMP(tentatives);
       if (tentatives >= 50) {
         TRACE();
        goto end;
       }
    }

    TRACE();
  while (secureClient.connected()) {
    String line = secureClient.readStringUntil('\n');
//    DUMP(line);
    if (line == "\r") {
      TRACE();
      break;
    }
  }
TRACE();
static uint8_t _buffer[RESP_BUFFER_LENGTH];
  while (secureClient.available()) {
    int actualLength = secureClient.read(_buffer, RESP_BUFFER_LENGTH  - 1);
     if (actualLength <= 0) {
      break;
     }
     _buffer[actualLength] = 0;
     contents += String((char*)_buffer).substring(0, actualLength);
    DUMP(contents);
  }
  }
end:
  DUMP(contents);
  secureClient.stop();
  return contents;
}

String getHttpContents(String url) {
  String contents;
  if (WiFi.isConnected()) {
    HTTPClient client;
    if (client.begin(wclient, url) == true) {
      if (client.GET()) {
        contents = client.getString();
      }
    }
    client.end();
  }
  return contents;
}

char stocksBuffer[128] = { 0 };
struct Quote {
  const char* name;
  int value;
};

// https://cloud.iexapis.com/stable/tops/last?token=pk_bb4ebad5e0d849ed94d61ee5d15e0dd7&symbols=aapl,ibm
const char* STOCKS_HOST = "cloud.iexapis.com";

struct Quote quotes[] = {
  { "aapl", -1 },
  { "amzn", -1 },
  { "goog", -1 },
  { "ibm", -1 },
  { "msft", -1 }
};

String stocks;
String stocks_rest;

//const char fingerprintIEX[] = "F5 71 04 D9 0C F6 77 A5 E3 26 10 27 25 A3 F7 B2 6A 27 7D B4"; // sha-1
const char fingerprintIEX[] = "2A 57 A6 D7 C7 8D 20 64 45 AB D3 39 75 0E 58 43 C6 A3 09 6C";

unsigned long lastIEXQuery = 0;
void getQuotes() {
  if (stocks.length() == 0) {
    uint8_t n = sizeof(quotes) / sizeof(struct Quote);
    for (uint8_t i = 0; i < n; ++i) {
      stocks += String(quotes[i].name);
      if (i < (n - 1))
        stocks += ",";
    }
    stocks_rest = String(F("/stable/tops/last?token=")) + iextoken + "&symbols=" + stocks;
  }
  if ((lastIEXQuery == 0) || ((millis() - lastIEXQuery) >= 10 * 60 * 1000)) { // every 10 mn
    lastIEXQuery = millis();
    String json = getHttpsContents(STOCKS_HOST, stocks_rest.c_str()); //fingerprintIEX);
    JsonDocument doc;
    auto result = deserializeJson(doc, json);
    if (!result) {
      String str("");
      JsonArray arr = doc.as<JsonArray>();
      uint8_t idx = 0;
      for (JsonObject quote : arr) {
        const char* stock = quote["symbol"];
        if (stock != 0) {
          float fvalue = quote["price"];
          int value = (int)fvalue;
          int pvalue = quotes[idx].value;
          char buf[16];
          int slen = sprintf(buf, "%s : %d^", stock, value);
          if (pvalue == -1) {
            buf[slen] = 32;
          } else {
            if (value > pvalue) {
              buf[slen] = 24;
            } else if (value < pvalue) {
              buf[slen] = 25;
            } else {
              buf[slen] = 18;
            }
          }
          buf[slen + 1] = 0;
          str += String(buf) + "   ";
          quotes[idx].value = value;
        }
        ++idx;
      }
      strcpy(stocksBuffer, str.c_str());
    }
  }
}

void displayQuotes(char* destination) {
  getQuotes();
  if (stocksBuffer[0] != 0) {
    strcpy(destination, stocksBuffer);
  } else {
    strcpy(destination, "");
  }
}

char airparifBuffer[128] = { 0 };

const char* airparifHOST = "api.airparif.fr";
const char* airparifREST = "/indices/prevision/commune?insee=94003";

unsigned long lastairparifQuery = 0;

char latin9buf[64];

char* Utf8ToLatin9(const char* utf8) {
      char utf8buf[64];
      strcpy(utf8buf, utf8);
      utf8_to_latin9(latin9buf, utf8buf, strlen(utf8buf));
      return latin9buf;
}


void getAirparif() {
  if ((lastairparifQuery == 0) || ((millis() - lastairparifQuery) >= 60 * 60 * 1000)) { // every 60 mn
    lastairparifQuery = millis();
    airparifBuffer[0] = 0;
   if (WiFi.isConnected()) {
      String json = getHttpsContents(airparifHOST, airparifREST, airparifAPIKEY);
      DUMP(json);
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, json);
      if (error) {
        DUMP(error.f_str());
       } else {
         const char* detail = doc["detail"];
         if (detail) {
          sprintf(airparifBuffer, "Pollution : %s", detail);
         } else {
          auto day = doc["94003"].as<JsonArray>()[0];
          String no2 = String(Utf8ToLatin9(day["no2"]));
          String o3 = String(Utf8ToLatin9(day["o3"]));
          String pf25 = String(Utf8ToLatin9(day["pm25"]));
          String indice = String(Utf8ToLatin9(day["indice"]));
          sprintf(airparifBuffer, "Indice pollution : %s (Ozone %s - Dioxyde d'Azote %s - Particules fines %s)", indice.c_str(), o3.c_str(), no2.c_str(), pf25.c_str());
         }
      } 
    } 
  } 
}

void displayAirparif(char* destination) {
   getAirparif();
  if (airparifBuffer[0] != 0) {
    strcpy(destination, airparifBuffer);
  } else {
    strcpy(destination, "");
  }
}

float insideTemp = -100.0;
unsigned long lastAdafruitQuery = 0;
const char* ADAFRUIT_HOST = "io.adafruit.com";
const char* ADAFRUIT_REST = "/api/v2/delhoume/feeds/temperature/data?include=value&limit=1";
//const char fingerprintAIO[] = "77 00 54 2D DA E7 D8 03 27 31 23 99 EB 27 DB CB A5 4C 57 18"; // sha-1
const char fingerprintAIO[] = "59 3C 48 0A B1 8B 39 4E 0D 58 50 47 9A 13 55 60 CC A0 1D AF"; // new july 2020
const char* localurl = "http://192.168.0.28/info"; // TODO: use DNS ?

void getInsideTemp() {
  if ((lastAdafruitQuery == 0) || ((millis() - lastAdafruitQuery) >= 17 * 60 * 1000)) { // every 17 mn
    if (WiFi.isConnected()) {
      //      String json = getHttpsContents(ADAFRUIT_HOST, ADAFRUIT_REST); // , fingerprintAIO);
      String json = getHttpContents(localurl);
      DUMP(json);
      JsonDocument doc;
      auto result = deserializeJson(doc, json);
      if (!result) {
        insideTemp = doc[0]["value"];
      } else {
        insideTemp = -110.0;
      }
    } else {
      insideTemp = -105.0;
    }
    lastAdafruitQuery = millis();
  }
}

void displayInsideTemp(char* destination) {
  getInsideTemp();
  if (insideTemp > -100.0) {
    if ((fabs(round(insideTemp) - insideTemp)) <= 0000.1) // int, do not display trailing 0
      sprintf(destination, "Temp\xE9rature int\xE9rieure : %d$", (int)round(insideTemp));
    else
      sprintf(destination, "Temp\xE9rature int\xE9rieure : %.1f$", insideTemp); // one decimal
  } else {
    if (insideTemp == -110.0)
      strcpy(destination, "Temp\xE9rature int\xE9rieure : Erreur");
    else if (insideTemp == -105.0)
      strcpy(destination, "Temp\xE9rature int\xE9rieure : Pas de connection");
    else
      strcpy(destination, "");
  }
}

void displayText(char* destination) {
  strcpy(destination, textBuffer);
}

void displayInvaders(char* destination) {
  P.setFont(0, invaderData);
  strcpy(destination, "abcde");
}

static String GetUptime() {
  uptime::calculateUptime();
  int d = uptime::getDays();
  String ret = "Uptime : ";
  if (d > 0) {
    ret +=  (String)(d) + "d";
  }
  ret += (String)(uptime::getHours()  ) + "h";
  if (d == 0) {
    ret += (String)(uptime::getMinutes()) + "m";
  }
  if (!WiFi.isConnected())
    ret += "-o";
  return ret;
}

void displayUptime(char* destination) {
  String uptime = GetUptime();
  strcpy(destination, uptime.c_str());
}


unsigned long lastBusQuery = 0L;
char BusBuffer[256] = { 0 };
const char* IDFM_HOST = "api-iv.iledefrance-mobilites.fr";
const char* BUS184_REST = "/lines/line:IDFM:C01205/stops/stop_area:IDFM:70364/realtime";
// https://api-iv.iledefrance-mobilites.fr/lines/line:IDFM:C01205/stops/stop_area:IDFM:70364/realtime

JsonDocument doc;
void getBus() {
  unsigned long m = millis();
  if ((lastBusQuery == 0) || ((m - lastBusQuery) >= 1 * 60 * 1000)) { // every 1 mn
    String textScroller = "Bus 184 ";
    lastBusQuery = m;
    TRACE();
    String json = getHttpsContents(IDFM_HOST, BUS184_REST); 
    DUMP(json);
    DeserializationError error = deserializeJson(doc, json);
    TRACE();
    if (error) {
      textScroller += error.f_str();
      DUMP(error.f_str());
    } else {
      TRACE();
      // test for error
      if (doc["nextDepartures"]["statusCode"] == 200) {
        char previousDirection[64] = { 0 };
        for (JsonObject item : doc["nextDepartures"]["data"].as<JsonArray>()) {
            const char* lineDirection = item["lineDirection"];
            DUMP(lineDirection);
            boolean isSameDirection = !strcmp(lineDirection, previousDirection);
            if (isSameDirection) {
              textScroller += ",";
             } else {
              textScroller += "  ";
              textScroller += String(lineDirection) + ": ";
              strcpy(previousDirection, lineDirection);
            }
           const char* code = item["code"]; // "duration", "message"
            if (!strcmp(code, "message")) {
               char utf8buf[64];
              strcpy(utf8buf, item["schedule"]);
              char latin9buf[64];
              utf8_to_latin9(latin9buf, utf8buf, strlen(utf8buf));
             textScroller += String(latin9buf);
            
            } else if (!strcmp(code, "duration")) {
              const char* thetime = item["time"];
              textScroller += String(thetime) + "mn";
            }
            }
      } else {
 //       const char* errorMessage = doc["nextDepartures"]["errorMessage"];
        textScroller += String(F("info indisponible"));
      }
    }
    strcpy(BusBuffer, textScroller.c_str());
  }
}

void displayBus(char* destination) {
  TRACE();
  getBus();
  if (BusBuffer[0] != 0) {
    strcpy(destination, BusBuffer);
  } else {
    strcpy(destination, "");
  }
}

DISPLAYFUN displayFuns[] = {
  displayDate,
  displayTemp,
  displayDescription,
  displayInsideTemp,
  displayAirparif,
  displayCryptos,
   displayQuotes,
  displayBus,
  displayText,
  displayUptime,
  displayInvaders
  //displayAllChars
};

uint8_t numFuns = sizeof(displayFuns) / sizeof(DISPLAYFUN);

void loop() {
  static unsigned long lastTime = 0;
  static unsigned long lastWifi = 0;
  static bool	flasher = false;
  static uint8_t curPage = 0;
  ArduinoOTA.handle();
  webServer.handleClient();
  P.displayAnimate();
  if (P.getZoneStatus(0)) {
    // change scrolling contents here
    // reset default font
    P.setFont(0, ExtASCII); // all utf8 chars
    displayFuns[curPage](scrollBuffer);
    curPage = (curPage + 1) % numFuns;
    P.displayReset(0);
  }
  if (millis() - lastTime >= 1000) {
    lastTime = millis();
    // query time here and blink
    flasher = !flasher;
    getTime(flasher);
    P.displayReset(1);
  }
  delay(5);
  if (millis() - lastWifi >= 1000 * 60 * 10) { // every 10 minute check wifi
    lastWifi = millis();
    if (!WiFi.isConnected()) {
      //     WiFiManager wifiManager;
      //    wifiManager.setConfigPortalTimeout(5);
      //    wifiManager.autoConnect("ticker"); //  no password
      ESP.restart();
    }
  }
}
