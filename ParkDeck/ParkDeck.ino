
/*========================================
|Include all libraries needed for program|
========================================*/

// Include the jpeg decoder library
#include <TJpg_Decoder.h>

// Include LittleFS
#include <FS.h>
#include <LittleFS.h>

//Include JSON
#include <ArduinoJson.h>

//Include base 64 encode
#include <base64.h>

// Include WiFi and http client
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <WiFiClientSecureBearSSL.h>

// Load tabs attached to this sketch
#include "List_SPIFFS.h"
#include "Web_Fetch.h"
#include "index.h"
#include "SpotifyConnection.h"

// Include the TFT library https://github.com/Bodmer/TFT_eSPI
#include <SPI.h>
#include <TFT_eSPI.h>

/*=========================
|User modifiable variables|
=========================*/

// WiFi credentials
#define WIFI_SSID "WIFI_SSID"
#define PASSWORD "PASSWORD"

// Spotify API credentials
#define CLIENT_ID "CLIENT_ID"
#define CLIENT_SECRET "CLIENT_SECRET"
#define REDIRECT_URI "http://YOUR_ESP_IP/callback"

/*=========================
|Non - modifiable variables|
==========================*/

//Vars for keys, play state, last song, etc.
bool buttonStates[] = { 1, 1, 1, 1, 1, 1 };
int debounceDelay = 50;
unsigned long debounceTimes[] = { 0, 0, 0, 0, 0, 0 };
int buttonPins[] = { D0, D1, D2, D7, D4, D5 };
int trackRefreshTimeInMilliseconds = 5000;

//Object instances
ESP8266WebServer server(80);  //Server on port 80
TFT_eSPI tft = TFT_eSPI();  // Invoke custom library
SpotifyConnection spotifyConnection(CLIENT_ID, CLIENT_SECRET, REDIRECT_URI, tft);

int imageOffsetX = 26, imageOffsetY = 20;

long timeLoop;
long refreshLoop;
bool serverOn = true;

//Web server callbacks
void handleRoot() {
  Serial.println("handling root");
  char page[500];
  sprintf(page, mainPage, CLIENT_ID, REDIRECT_URI);
  server.send(200, "text/html", String(page) + "\r\n");  //Send web page
}

void handleCallbackPage() {
  if (!spotifyConnection.accessTokenSet) {
    if (server.arg("code") == "") {  //Parameter not found
      char page[500];
      sprintf(page, errorPage, CLIENT_ID, REDIRECT_URI);
      server.send(200, "text/html", String(page));  //Send web page
    } else {
      //Parameter found
      if (spotifyConnection.getUserCode(server.arg("code"))) {
        server.send(200, "text/html", "Spotify setup complete Auth refresh in :" + String(spotifyConnection.tokenExpireTime));
      } else {
        char page[500];
        sprintf(page, errorPage, CLIENT_ID, REDIRECT_URI);
        server.send(200, "text/html", String(page));  //Send web page
      }
    }
  } else {
    server.send(200, "text/html", "Spotify setup complete");
  }
}

// This next function will be called during decoding of the jpeg file to
// render each block to the TFT.  If you use a different TFT library
// you will need to adapt this function to suit.
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap) {
  // Stop further decoding as image is running off bottom of screen
  if (y >= tft.height()) {
    return 0;
  }

  // This function will clip the image block rendering automatically at the TFT boundaries
  tft.pushImage(x, y, w, h, bitmap);

  // Return 1 to decode next block
  return 1;
}

/*==============
|Setup function|
==============*/
// * Sets up WiFi
// * Initializes screen
// * Shows Ip on screen

void setup() {
  Serial.begin(115200);

  // Initialise LittleFS
  if (!LittleFS.begin()) {
    Serial.println("LittleFS initialisation failed!");
    while (1) yield();  // Stay here twiddling thumbs waiting
  }
  Serial.println("\r\nInitialisation done.");

  // Initialise the TFT
  tft.begin();
  tft.fillScreen(TFT_BLACK);
  tft.setRotation(2);
  // The jpeg image can be scaled by a factor of 1, 2, 4, or 8
  TJpgDec.setJpgScale(4);

  // The byte order can be swapped (set true for TFT_eSPI)
  TJpgDec.setSwapBytes(true);

  // The decoder must be given the exact name of the rendering function above
  TJpgDec.setCallback(tft_output);

  WiFi.begin(WIFI_SSID, PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi\n Ip is: ");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);                  //Which routine to handle at root location
  server.on("/callback", handleCallbackPage);  //Which routine to handle at root location
  server.begin();                              //Start server
  Serial.println("HTTP server started");
  for (int i = 0; i < 6; i++) {
    pinMode(buttonPins[i], INPUT_PULLUP);
  }

  tft.println(WiFi.localIP());
  Serial.println("Setup done");
}

void loop() {
  if (!spotifyConnection.accessTokenSet) {
    server.handleClient();
    return;
  }

  if (serverOn) {
    server.close();
    serverOn = false;
  }

  if ((millis() - spotifyConnection.tokenStartTime) / 1000 > spotifyConnection.tokenExpireTime) {
    Serial.println("refreshing token");
    if (spotifyConnection.refreshAuth()) {
      Serial.println("refreshed token");
    }
  }

  if ((millis() - refreshLoop) > trackRefreshTimeInMilliseconds) {
    spotifyConnection.getTrackInfo();

    refreshLoop = millis();
  }

  for (int i = 0; i < 6; i++) {
    int reading = digitalRead(buttonPins[i]);
    if (reading != buttonStates[i]) {
      if (millis() - debounceTimes[i] > debounceDelay) {
        buttonStates[i] = reading;
        if (reading == LOW) {
          switch (i) {
            case 0:
              Serial.println("Button 0: not in use");
              //listSPIFFS();
              break;
            case 1:
              Serial.println("Button 1: toggleLiked");
              spotifyConnection.toggleLiked(spotifyConnection.currentSong.Id);
              break;
            case 2:
              Serial.println("Button 2: not in use");
              break;
            case 3:
              Serial.println("Button 3: skipBack");
              spotifyConnection.skipBack();
              break;
            case 4:
              Serial.println("Button 4 togglePlay");
              spotifyConnection.togglePlay();
              break;
            case 5:
              Serial.println("Button 5 skipForward");
              spotifyConnection.skipForward();
              break;
            default:
              break;
          }
        }
        debounceTimes[i] = millis();
      }
    }
  }

  int volumeRequest = map(analogRead(A0), 0, 1023, 100, 0);
  if (abs(volumeRequest - spotifyConnection.currVol) > 2) {
    spotifyConnection.adjustVolume(volumeRequest);
  }
  timeLoop = millis();
}
