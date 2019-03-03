#include <MD_Parola.h>
#include "JF_Font_Data.h"
#include "ASCII_Font_Data.h"
#include "Numeric7seg_Font_data.h"

#include <WiFiManager.h>
#include <ArduinoOTA.h>
#include <WiFiUdp.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPClient.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiClient.h>

#include <ArduinoJson.h>

#include <FS.h>

#include <Timezone.h>

#define HARDWARE_TYPE MD_MAX72XX::ICSTATION_HW
#define MAX_DEVICES 8
#define CLK_PIN   D8 // or SCK
#define DATA_PIN  D7 // or MOSI
#define CS_PIN    D5 // or SS

// SOFTWARE SPI
MD_Parola P = MD_Parola(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);

#define PAUSE_TIME 0
#define SPEED_TIME 60

char timeBuffer[8];
char scrollBuffer[256]; // maybe a bit large...

uint8_t degC[] = { 6, 3, 3, 56, 68, 68, 68 };	 // Deg C
uint8_t euro[] = { 6, 20, 62, 85, 85, 65, 34 };  // 'Euro sign'
uint8_t bitcoin[] = { 5, 127, 73, 255, 73, 54 };
uint8_t insta[] = { 8, 126, 129, 153, 165, 165, 153, 129, 126 }; // Instagram icon

typedef void (*DISPLAYFUN)();

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
  sprintf(timeBuffer, "%02d%c%02d", h, (flasher ? ':' : ' '), m);
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
  ArduinoOTA.onError([](ota_error_t error) {});
  ArduinoOTA.begin();
}

unsigned int localPort = 2390;      // local port to listen for UDP packets
IPAddress timeServerIP; // time.nist.gov NTP server address
const char* ntpServerName = "time.google.com";
const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets

WiFiUDP udp;

void sendNTPpacket(IPAddress& address) {
  if (WiFi.isConnected()) {
 //   Serial.println("Requesting NTP");
    memset(packetBuffer, 0, NTP_PACKET_SIZE);
    packetBuffer[0] = 0b11100011;   // LI, Version, Mode
    udp.beginPacket(address, 123); //NTP requests are to port 123
    udp.write(packetBuffer, NTP_PACKET_SIZE);
    udp.endPacket();
  }
}

time_t getNtpTime() {
  while (udp.parsePacket() > 0) ; // discard any previously received packets
//  Serial.println("Transmit NTP Request");
  sendNTPpacket(timeServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
//      Serial.println("Receive NTP Response");
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
//  Serial.println("No NTP Response :-(");
  return 0; // return 0 if unable to get the time
}

ESP8266WebServer webServer(80);       // Create a webserver object that listens for HTTP request on port 80

char textBuffer[128] = { 0 };

const char INDEX_HTML[] =
"<!DOCTYPE HTML>"
"<html>"
"<head>"
"<meta name = \"viewport\" content = \"width = device-width, initial-scale = 1.0, maximum-scale = 1.0, user-scalable=0\">"
"<title>Ticker text</title>"
"<style>"
"\"body { background-color: #808080; font-family: Arial, Helvetica, Sans-Serif; Color: #000000; }\""
"</style>"
"</head>"
"<body>"
"<h1>Enter text</h1>"
"<FORM action=\"/\" method=\"post\">"
"<P>"
"<INPUT type=\"text\" name=\"value\"><BR>"
"</P>"
"</FORM>"
"</body>"
"</html>";

void handleRoot(){
  if (webServer.hasArg("value")) {
    strcpy(textBuffer, webServer.arg("value").c_str());
  }
    webServer.send(200, "text/html", INDEX_HTML);
}

const char* days[] = { "Dimanche", "Lundi", "Mardi", "Mercredi", "Jeudi", "Vendredi", "Samedi"  };
const char* monthes[] = { "Janvier", "F\xE9vrier", "Mars", "Avril", "Mai", "Juin", "Juillet", 
                          "Ao\xFBt", "Septembre", "Octobre", "Novembre", "D\xE9""cembre" };



void setup(void) {
  Serial.begin(57600);
  P.begin();
  P.setIntensity(4);
  P.print("Wifi...");

#if 1
  WiFiManager wifiManager;
  wifiManager.setConfigPortalTimeout(120);
  // wifiManager.setDebugOutput(true);
  wifiManager.setMinimumSignalQuality(30);
  wifiManager.autoConnect("ticker"); //  no password
#else
  WiFi.begin("fred", "fredfred");             // Connect to the network
  Serial.print("Connecting to ");
  Serial.println(" ...");

  int i = 0;
  while (WiFi.status() != WL_CONNECTED) { // Wait for the Wi-Fi to connect
    delay(1000);
    Serial.print(++i); Serial.print(' ');
  }
#endif
  
  strcpy(timeBuffer, "--:--");
  strcpy(scrollBuffer, "Fred");

  if (WiFi.isConnected()) {
    initOTA();
    initOWM();
    // start file system
    SPIFFS.begin();
    MDNS.begin("ticker");
    udp.begin(localPort);
    WiFi.hostByName(ntpServerName, timeServerIP);

    setSyncProvider(getNtpTime);
    setSyncInterval(3600);

    strcpy(scrollBuffer, WiFi.localIP().toString().c_str());

    webServer.onNotFound([](){
      webServer.send(404, "text/plain", "404: Not found");
    });
    webServer.on("/", handleRoot);
    webServer.begin();
 }

  // Parola object
  P.begin(2);

  P.setZone(0, 0, MAX_DEVICES - 4); // scrolling text
  P.setZone(1, MAX_DEVICES - 3, MAX_DEVICES - 1); // time

  P.setSpeed(50);
  P.setIntensity(0, 1);
  P.setIntensity(1, 5); // green is dimmer ?

  P.setFont(0, ExtASCII); // all utf8 chars
  //P.setFont(1, jF_Custom); // small fixed width numbers
  P.setFont(1, numeric7Se);

  P.displayZoneText(1, timeBuffer, PA_LEFT, SPEED_TIME, PAUSE_TIME, PA_PRINT, PA_NO_EFFECT);
  P.displayZoneText(0, scrollBuffer, PA_LEFT, SPEED_TIME, PAUSE_TIME, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
  //P.displayZoneText(0, scrollBuffer, PA_CENTER, SPEED_TIME, 0, PA_SPRITE, PA_SPRITE);
  //P.setSpriteData(0, invader, W_INVADER, F_INVADER, invader, W_INVADER, F_INVADER);

  P.addChar(0, '$', degC);
  P.addChar(0, '^', euro);
  P.addChar(0, '#', bitcoin);
  P.addChar(0, ';', insta);
}


void displayDate() {
    time_t utc = now(); // triggers update if necessary
    time_t local = CE.toLocal(utc, &tcr);
    int wd = weekday(local);
    int d = day(local);
    int m = month(local);
    sprintf(scrollBuffer, "%s %d %s", days[wd - 1], d, monthes[m - 1]);
}

HTTPClient owm;

struct WeatherInfo {
  float temperature;
  boolean error;
  char description[32];
  char fulldescription[64];
};

struct WeatherInfo OWMInfo;

const char* owmApiKey    = "95b2ed7cdfc0136948c0a9d499f807eb";
const char* owmArcueilId = "6613168";
const char* owmLang      = "fr"; // fr encodes accents in utf8, not easy to convert

char owmURL[128];

void initOWM() {
  sprintf(owmURL, 
    "http://api.openweathermap.org/data/2.5/weather?id=%s&lang=%s&appid=%s&units=metric", 
    owmArcueilId, 
    owmLang, 
    owmApiKey);
    OWMInfo.error = true;
}
/* UTF-8 to ISO-8859-1/ISO-8859-15 mapper.
 * Return 0..255 for valid ISO-8859-15 code points, 256 otherwise.
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
 * All invalid sequences are ignored.
 * Note: output == input is allowed,
 * but   input < output < input + length
 * is not.
 * Output has to have room for (length+1) chars, including the trailing NUL byte.
*/
size_t utf8_to_latin9(char *const output, const char *const input, const size_t length) {
    unsigned char             *out = (unsigned char *)output;
    const unsigned char       *in  = (const unsigned char *)input;
    const unsigned char *const end = (const unsigned char *)input + length;
    unsigned int               c;

    while (in < end)
        if (*in < 128)
            *(out++) = *(in++); /* Valid codepoint */
        else
        if (*in < 192)
            in++;               /* 10000000 .. 10111111 are invalid */
        else
        if (*in < 224) {        /* 110xxxxx 10xxxxxx */
            if (in + 1 >= end)
                break;
            if ((in[1] & 192U) == 128U) {
                c = to_latin9( (((unsigned int)(in[0] & 0x1FU)) << 6U)
                             |  ((unsigned int)(in[1] & 0x3FU)) );
                if (c < 256)
                    *(out++) = c;
            }
            in += 2;

        } else
        if (*in < 240) {        /* 1110xxxx 10xxxxxx 10xxxxxx */
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

        } else
        if (*in < 248) {        /* 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx */
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

        } else
        if (*in < 252) {        /* 111110xx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx */
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

        } else
        if (*in < 254) {        /* 1111110x 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx */
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

void getOWMInfo() {
  static long lastOWMQuery = 0;
  if ((lastOWMQuery == 0) || ((millis() - lastOWMQuery) >= 15 * 60 * 1000)) { // every 10 mn
    OWMInfo.error = true;
    if (WiFi.isConnected()) {
      WiFiClient client;
      owm.begin(client, owmURL);
      if (owm.GET()) {
        String json = owm.getString();
//        Serial.println(json);
//       Serial.println(json.length());
        DynamicJsonBuffer jsonBuffer(1024);
        JsonObject& root = jsonBuffer.parseObject(json);
        if (root.success()) {
          float temp = root["main"]["temp"];
          OWMInfo.temperature = round(temp);
          JsonObject& weather = root["weather"][0];
          strcpy(OWMInfo.description, weather["main"]);
          static char utf8buf[64];
          strcpy(utf8buf, weather["description"]);
           utf8_to_latin9(OWMInfo.fulldescription, utf8buf, strlen(utf8buf));
     //      displayHexString(OWMInfo.fulldescription);
     //      Serial.println(OWMInfo.temperature);
          OWMInfo.error = false;
        }
      }
      owm.end();
    }
    lastOWMQuery = millis();
  }
 }

void displayTemp() {
  getOWMInfo();
  if (OWMInfo.error == false) {
    sprintf(scrollBuffer, "Temp\xE9rature ext\xE9rieure : %d$", (int)OWMInfo.temperature);
  } else {
   strcpy(scrollBuffer, "");   
  }
  scrollBuffer[4] = 0xE9; // patch accent 
}

void displayDescription() {
  getOWMInfo();
  if (OWMInfo.error == false) {
    sprintf(scrollBuffer, "%s", OWMInfo.fulldescription);
    scrollBuffer[0] = toupper(scrollBuffer[0]);
  } else {
   strcpy(scrollBuffer, "");   
  } 
}

const char* bitcoinURL = "http://api.coindesk.com/v1/bpi/currentprice/EUR.json";
boolean btcError = true;
float btcValue = -1.0;


void getBitcoin() {
  static long lastBTCQuery = 0;
  if ((lastBTCQuery == 0) || ((millis() - lastBTCQuery) >= 10 * 60 * 1000)) { // every 10 mn
   btcError = true;
   if (WiFi.isConnected()) {
    WiFiClient client;
      owm.begin(client, bitcoinURL);
      if (owm.GET()) {
        String json = owm.getString();
       //Serial.println(json);
       //Serial.println(json.length());
        DynamicJsonBuffer jsonBuffer(1024);
        JsonObject& root = jsonBuffer.parseObject(json);
        if (root.success()) {
          JsonObject& bpi_EUR = root["bpi"]["EUR"];
          btcValue = bpi_EUR["rate_float"]; // 3285.5006
          btcError = false;
        }
      }
      owm.end();
    }
    lastBTCQuery = millis();
  }
 }

void displayBitcoin() {
  getBitcoin();
  if (btcError == false) {
    sprintf(scrollBuffer, "#itcoin : %d ^", (int)btcValue);
  } else {
    strcpy(scrollBuffer, "");
  }
}

void displayAllChars() {
  uint16_t idx;
  for (idx = 1; idx < 256; ++idx) {
    scrollBuffer[idx] = idx;
  }
  scrollBuffer[idx] = 0;
}

#define RESP_BUFFER_LENGTH 512
uint8_t _buffer[RESP_BUFFER_LENGTH];

int igFollowers;
boolean igError;

char HOST_INSTA[] = "api.instagram.com";
WiFiClientSecure igClient;
String INSTA_ACCESS_TOKEN = "371168969.6b88fad.5963587e52864c6088e2f005d8302625";
const char fingerprint[] PROGMEM = "4D 43 94 7A 0B DB 77 C1 D6 65 E1 12 8C 16 20 23 BC F9 4F 03";

void getIGFollowers() {
  static long lastIGQuery = 0;
  if ((lastIGQuery == 0) || ((millis() - lastIGQuery) >= 30 * 60 * 1000)) { // every 30 mn
    igError = true;
   if (WiFi.isConnected()) {
    igClient.setFingerprint(fingerprint);
    igClient.setInsecure();
    if (igClient.connect(HOST_INSTA, 443)) { // very slow ! why ?
//      Serial.println("Connected");
      igClient.print(String("GET /v1/users/self/?access_token=") + INSTA_ACCESS_TOKEN +
                            " HTTP/1.1\r\n" +
                            "Host: " + HOST_INSTA + "\r\n" + 
                      //      "User-Agent: arduino/1.0.0\r\n" +
                             "Connection: close\r\n\r\n");
  
       while(igClient.connected() && !igClient.available());
       while (igClient.connected()) {
          String line = igClient.readStringUntil('\n');
          if (line == "\r") {
 //           Serial.println("headers received");
            break;
          }
      }
       String json;
     // readStringUntil is very slow, because it is blocking on found char or 5s timeout...
       while (igClient.available()) {
          int actualLength = igClient.read(_buffer, RESP_BUFFER_LENGTH - 1);
          _buffer[actualLength] = 0;
            json += String((char*)_buffer);
      }
      igClient.stop();
  
      DynamicJsonBuffer jsonBuffer(600);
      JsonObject& root = jsonBuffer.parseObject(json);
      if (root.success()) {
        JsonObject& data = root["data"];
        JsonObject& data_counts = data["counts"];
        igFollowers = data_counts["followed_by"];
        igError = false;
      }
     }
   } else {
    //       Serial.println("Not connected");
    }
     lastIGQuery = millis();
   }
}

void displayIGFollowers() {
  getIGFollowers();
  if (igError == false) {
    sprintf(scrollBuffer, "; %d followers", (int)igFollowers);
  } else {
    strcpy(scrollBuffer, "");
  }
}

boolean stocksError;
char stocksBuffer[256];

char HOST_STOCKS[] = "api.iextrading.com";
WiFiClientSecure iexClient;
const char fingerprintIEX[] PROGMEM = "D1 34 42 D6 30 58 2F 09 A0 8C 48 B6 25 B4 6C 05 69 A4 2E 4E";

void getQuotes() {
  static long lastIEXQuery = 0;
  if ((lastIEXQuery == 0) || ((millis() - lastIEXQuery) >= 15 * 60 * 1000)) { // every 15 mn
    stocksError = true;
   if (WiFi.isConnected()) {
      iexClient.setFingerprint(fingerprintIEX); 
      iexClient.setInsecure();
 //     iexClient.setTimeout(10);
//      long tt = millis();
      if (iexClient.connect(HOST_STOCKS, 443)) { // very slow ! > 2 seconds
 //       Serial.print("connect time : "); Serial.println(millis() - tt); tt = millis();
          String json;
           iexClient.print(String("GET /1.0/stock/market/batch?symbols=aapl,amzn,goog,ibm,msft&types=price") +
                  " HTTP/1.1\r\n" +
                  "Host: " + HOST_STOCKS + "\r\n" +
//                  "User-Agent: arduino/1.0.0\r\n" +
                  "Connection: close\r\n\r\n");
            while(iexClient.connected() && !iexClient.available());
           while (iexClient.connected()) {
              String line = iexClient.readStringUntil('\n');
              if (line == "\r") {
                 break;
              }
          }
          // readStringUntil is very slow, because it is blocking on found char or 5s timeout...
           while (iexClient.available()) {
             int actualLength = iexClient.read(_buffer, RESP_BUFFER_LENGTH - 1);
              _buffer[actualLength] = 0;
              json += String((char*)_buffer);
          }
          iexClient.stop();

          DynamicJsonBuffer jsonBuffer(256);
          JsonObject& root = jsonBuffer.parseObject(json);
          if (root.success()) {
             String str;
              for (JsonPair& p : root) {
                  const char* stock = p.key; // is a const char* pointing to the key           
                   JsonVariant& v = p.value; // is a JsonVariant
                   JsonObject& var = v.as<JsonObject>();
                   float value = var["price"];
                   char buf[32];
                   sprintf(buf, "%s : %d    ", stock, (int)value);
                   str += String(buf);
              }
              strcpy(stocksBuffer, str.c_str());
              stocksError = false;
           }
        }
      } else {
      }
      lastIEXQuery = millis();
    }
}

void displayQuotes() {
  getQuotes();
  if (stocksError == false) {
    strcpy(scrollBuffer, stocksBuffer);
  } else {
    strcpy(scrollBuffer, "");
  }
}

boolean insideTempError;
float insideTemp;

HTTPClient adafruitClient;
// not needed (public feed)
#define AIO_KEY         "5cefb5958805493da7dc5610a4c47eb9"

void getInsideTemp() {
  static long lastAdafruitQuery = 0;
  if ((lastAdafruitQuery == 0) || ((millis() - lastAdafruitQuery) >= 15 * 60 * 1000)) { // every 15 mn
    insideTempError = true;
   if (WiFi.isConnected()) {
    WiFiClient client;
    // last is not working
    adafruitClient.begin(client, "http://io.adafruit.com/api/v2/delhoume/feeds/temperature/data?include=value&limit=1");
      if (adafruitClient.GET()) {
        String json = adafruitClient.getString();

//      Serial.println(json);
      DynamicJsonBuffer jsonBuffer(64);
      JsonArray& root = jsonBuffer.parseArray(json);
      if (root.success()) {
//        Serial.println("Success");
        insideTemp = root[0]["value"];
        insideTempError = false;
      }
     }
  } else {
//        Serial.println("Not connected");
  }
   lastAdafruitQuery = millis();
}
}

void displayInsideTemp() {
  getInsideTemp();
  if (insideTempError == false) {
    if ((fabs(round(insideTemp) - insideTemp)) <= 0000.1) // int, do not display trailing 0
      sprintf(scrollBuffer, "Temp\xE9rature int\xE9rieure : %d$", (int)round(insideTemp));
    else
      sprintf(scrollBuffer, "Temp\xE9rature int\xE9rieure : %.1f$", insideTemp); // one decimal
  } else {
    strcpy(scrollBuffer, "");
  }
}

void displayText() {
    strcpy(scrollBuffer, textBuffer);
}

DISPLAYFUN displayFuns[] = {
  displayDate,
  displayTemp,
  displayDescription,
  displayInsideTemp,
  displayBitcoin,
  displayIGFollowers,
  displayQuotes,
  displayText,
  //displayAllChars
};

uint16_t numFuns = sizeof(displayFuns) / sizeof(DISPLAYFUN);
    
void loop(void) {
  static long lastTime = 0;
  static bool	flasher = false;
  static uint8_t curPage = 0;

  ArduinoOTA.handle();
  webServer.handleClient();
  P.displayAnimate();
  if (P.getZoneStatus(0)) {
    // change scrolling contents here
    displayFuns[curPage]();
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
}
