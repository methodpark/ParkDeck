// Include the TFT library https://github.com/Bodmer/TFT_eSPI
#include <SPI.h>
#include <TFT_eSPI.h>


class SpotifyConnection {
public:
  SpotifyConnection(String _CLIENT_ID, String _CLIENT_SECRET, String _REDIRECT_URI, TFT_eSPI &_tft): tft (_tft) {
    CLIENT_ID = _CLIENT_ID;
    CLIENT_SECRET = _CLIENT_SECRET;
    REDIRECT_URI = _REDIRECT_URI;
    client = std::make_unique<BearSSL::WiFiClientSecure>();
    client->setInsecure();

    for (int i = 0; i < 10; i++) {
      parts[i] = (char *)malloc(sizeof(char) * 20);
    }
  }

  struct songDetails {
    int durationMs;
    String album;
    String artist;
    String song;
    String Id;
    bool isLiked;
  };

  String getValue(HTTPClient &http, String key) {
    bool found = false, look = false, seek = true;
    int ind = 0;
    String ret_str = "";

    int len = http.getSize();
    char char_buff[1];
    WiFiClient *stream = http.getStreamPtr();
    while (http.connected() && (len > 0 || len == -1)) {
      size_t size = stream->available();
      if (size) {
        int c = stream->readBytes(char_buff, ((size > sizeof(char_buff)) ? sizeof(char_buff) : size));
        if (found) {
          if (seek && char_buff[0] != ':') {
            continue;
          } else if (char_buff[0] != '\n') {
            if (seek && char_buff[0] == ':') {
              seek = false;
              int c = stream->readBytes(char_buff, 1);
            } else {
              ret_str += char_buff[0];
            }
          } else {
            break;
          }
        } else if ((!look) && (char_buff[0] == key[0])) {
          look = true;
          ind = 1;
        } else if (look && (char_buff[0] == key[ind])) {
          ind++;
          if (ind == key.length()) {
            found = true;
          }
        } else if (look && (char_buff[0] != key[ind])) {
          ind = 0;
          look = false;
        }
      }
    }

    if (*(ret_str.end() - 1) == ',') {
      ret_str = ret_str.substring(0, ret_str.length() - 1);
    }

    return ret_str;
  }

  void printSplitString(String text, int maxLineSize, int yPos) {
    int currentWordStart = 0;
    int spacedCounter = 0;
    int spaceIndex = text.indexOf(" ");

    while (spaceIndex != -1) {
      char *part = parts[spacedCounter];
      sprintf(part, text.substring(currentWordStart, spaceIndex).c_str());
      currentWordStart = spaceIndex;
      spacedCounter++;
      spaceIndex = text.indexOf(" ", spaceIndex + 1);
    }

    char *part = parts[spacedCounter];
    sprintf(part, text.substring(currentWordStart, text.length()).c_str());
    currentWordStart = spaceIndex;
    size_t counter = 0;
    currentWordStart = 0;
    while (counter <= spacedCounter) {
      char printable[maxLineSize];
      char *printablePointer = printable;
      // sprintf in word at counter always
      sprintf(printablePointer, parts[counter]);
      //get length of first word
      int currentLen = 0;
      while (parts[counter][currentLen] != '\0') {
        currentLen++;
        printablePointer++;
      }
      counter++;
      while (counter <= spacedCounter) {
        int nextLen = 0;
        while (parts[counter][nextLen] != '\0') {
          nextLen++;
        }
        if (currentLen + nextLen > maxLineSize) {
          break;
        }
        sprintf(printablePointer, parts[counter]);
        currentLen += nextLen;
        printablePointer += nextLen;
        counter++;
      }

      String output = String(printable);

      if (output[0] == ' ') {
        output = output.substring(1);
      }

      tft.setCursor((int)(tft.width() / 2 - tft.textWidth(output) / 2), tft.getCursorY());
      tft.println(output);
    }
  }

  bool getUserCode(String serverCode) {
    https.begin(*client, "https://accounts.spotify.com/api/token");
    String auth = "Basic " + base64::encode(String(CLIENT_ID) + ":" + String(CLIENT_SECRET));
    https.addHeader("Authorization", auth);
    https.addHeader("Content-Type", "application/x-www-form-urlencoded");
    String requestBody = "grant_type=authorization_code&code=" + serverCode + "&redirect_uri=" + String(REDIRECT_URI);
    // Send the POST request to the Spotify API
    int httpResponseCode = https.POST(requestBody);
    // Check if the request was successful
    if (httpResponseCode == HTTP_CODE_OK) {
      String response = https.getString();
      DynamicJsonDocument doc(1024);
      deserializeJson(doc, response);
      accessToken = String((const char *)doc["access_token"]);
      refreshToken = String((const char *)doc["refresh_token"]);
      tokenExpireTime = doc["expires_in"];
      tokenStartTime = millis();
      accessTokenSet = true;
      Serial.println(accessToken);
      Serial.println(refreshToken);
    } else {
      Serial.println(https.getString());
    }
    // Disconnect from the Spotify API
    https.end();
    return accessTokenSet;
  }

  bool refreshAuth() {
    https.begin(*client, "https://accounts.spotify.com/api/token");
    String auth = "Basic " + base64::encode(String(CLIENT_ID) + ":" + String(CLIENT_SECRET));
    https.addHeader("Authorization", auth);
    https.addHeader("Content-Type", "application/x-www-form-urlencoded");
    String requestBody = "grant_type=refresh_token&refresh_token=" + String(refreshToken);
    // Send the POST request to the Spotify API
    int httpResponseCode = https.POST(requestBody);
    accessTokenSet = false;
    // Check if the request was successful
    if (httpResponseCode == HTTP_CODE_OK) {
      String response = https.getString();
      DynamicJsonDocument doc(1024);
      deserializeJson(doc, response);
      accessToken = String((const char *)doc["access_token"]);
      tokenExpireTime = doc["expires_in"];
      tokenStartTime = millis();
      accessTokenSet = true;
      Serial.println(accessToken);
      Serial.println(refreshToken);
    } else {
      Serial.println(https.getString());
    }
    // Disconnect from the Spotify API
    https.end();
    return accessTokenSet;
  }

  bool getTrackInfo() {
    String url = "https://api.spotify.com/v1/me/player/currently-playing";
    https.useHTTP10(true);
    https.begin(*client, url);
    String auth = "Bearer " + String(accessToken);
    https.addHeader("Authorization", auth);
    int httpResponseCode = https.GET();
    bool success = false;
    String songId = "";
    bool refresh = false;
    // Check if the request was successful
    if (httpResponseCode == 200) {
      String currentSongProgress = getValue(https, "progress_ms");
      currentSongPositionMs = currentSongProgress.toFloat();
      String imageLink = "";
      while (imageLink.indexOf("image") == -1) {
        String height = getValue(https, "height");
        if (height.toInt() > 300) {
          imageLink = "";
          continue;
        }
        imageLink = getValue(https, "url");
      }

      String albumName = getValue(https, "name");
      String artistName = getValue(https, "name");
      String songDuration = getValue(https, "duration_ms");
      currentSong.durationMs = songDuration.toInt();
      String songName = getValue(https, "name");
      songId = getValue(https, "uri");
      String isPlay = getValue(https, "is_playing");
      isPlaying = isPlay == "true";
      Serial.println(isPlay);
      songId = songId.substring(15, songId.length() - 1);
      https.end();

      if (songId != currentSong.Id) {
        if (LittleFS.exists("/albumArt.jpg") == true) {
          LittleFS.remove("/albumArt.jpg");
        }

        bool loaded_ok = getFile(imageLink.substring(1, imageLink.length() - 1).c_str(), "/albumArt.jpg");  // Note name preceded with "/"
        Serial.println("Image load was: ");
        Serial.println(loaded_ok);
        refresh = true;
        tft.fillScreen(TFT_BLACK);
      }
      currentSong.album = albumName.substring(1, albumName.length() - 1);
      currentSong.artist = artistName.substring(1, artistName.length() - 1);
      currentSong.song = songName.substring(1, songName.length() - 1);
      currentSong.Id = songId;
      currentSong.isLiked = findLikedStatus(songId);
      success = true;
    } else {
      Serial.print("Error getting track info: ");
      Serial.println(httpResponseCode);
      // Disconnect from the Spotify API
      https.end();
    }

    if (success) {
      drawScreen(refresh);
      lastSongPositionMs = currentSongPositionMs;
    }
    return success;
  }

  bool findLikedStatus(String songId) {
    String url = "https://api.spotify.com/v1/me/tracks/contains?ids=" + songId;
    https.begin(*client, url);
    String auth = "Bearer " + String(accessToken);
    https.addHeader("Authorization", auth);
    https.addHeader("Content-Type", "application/json");
    int httpResponseCode = https.GET();
    bool success = false;
    // Check if the request was successful
    if (httpResponseCode == 200) {
      String response = https.getString();
      // Disconnect from the Spotify API
      https.end();
      return (response == "[ true ]");
    } else {
      Serial.print("Error toggling liked songs: ");
      Serial.println(httpResponseCode);
      String response = https.getString();
      Serial.println(response);
    }

    // Disconnect from the Spotify API
    https.end();
    return success;
  }

  bool toggleLiked(String songId) {
    String url = "https://api.spotify.com/v1/me/tracks/contains?ids=" + songId;
    https.begin(*client, url);
    String auth = "Bearer " + String(accessToken);
    https.addHeader("Authorization", auth);
    https.addHeader("Content-Type", "application/json");
    int httpResponseCode = https.GET();
    bool success = false;
    // Check if the request was successful
    if (httpResponseCode == 200) {
      String response = https.getString();
      if (response == "[ true ]") {
        currentSong.isLiked = false;
        dislikeSong(songId);
      } else {
        currentSong.isLiked = true;
        likeSong(songId);
      }
      drawScreen(false, true);
      Serial.println(response);
      success = true;
    } else {
      Serial.print("Error toggling liked songs: ");
      Serial.println(httpResponseCode);
      String response = https.getString();
      Serial.println(response);
    }

    // Disconnect from the Spotify API
    https.end();
    return success;
  }

  bool drawScreen(bool fullRefresh = false, bool likeRefresh = false) {
    int rectWidth = 120;
    int rectHeight = 10;
    if (fullRefresh) {
      if (LittleFS.exists("/albumArt.jpg") == true) {
        TJpgDec.setSwapBytes(true);
        TJpgDec.setJpgScale(4);
        TJpgDec.drawFsJpg(26, 5, "/albumArt.jpg", LittleFS);
      } else {
        TJpgDec.setSwapBytes(false);
        TJpgDec.setJpgScale(1);
        TJpgDec.drawFsJpg(0, 0, "/Angry.jpg", LittleFS);
      }
      tft.setTextDatum(MC_DATUM);
      tft.setTextWrap(true);
      tft.setCursor(0, 85);
      printSplitString(currentSong.artist, 20, 85);
      tft.setCursor(0, 110);

      printSplitString(currentSong.song, 20, 110);

      tft.drawRoundRect(
        tft.width() / 2 - rectWidth / 2,
        140,
        rectWidth,
        rectHeight,
        4,
        TFT_DARKGREEN);
    }
    if (fullRefresh || likeRefresh) {
      if (currentSong.isLiked) {
        TJpgDec.setJpgScale(1);
        TJpgDec.drawFsJpg(128 - 20, 0, "/heart.jpg", LittleFS);
      } else {
        tft.fillRect(128 - 21, 0, 21, 21, TFT_BLACK);
      }
    }
    if (lastSongPositionMs > currentSongPositionMs) {
      tft.fillSmoothRoundRect(
        tft.width() / 2 - rectWidth / 2 + 2,
        140 + 2,
        rectWidth - 4,
        rectHeight - 4,
        10,
        TFT_BLACK);
      lastSongPositionMs = currentSongPositionMs;
    }
    tft.fillSmoothRoundRect(
      tft.width() / 2 - rectWidth / 2 + 2,
      140 + 2,
      rectWidth * (currentSongPositionMs / currentSong.durationMs) - 4,
      rectHeight - 4,
      10,
      TFT_GREEN);

    return true;
  }

  bool togglePlay() {
    String url = "https://api.spotify.com/v1/me/player/" + String(isPlaying ? "pause" : "play");
    isPlaying = !isPlaying;
    https.begin(*client, url);
    String auth = "Bearer " + String(accessToken);
    https.addHeader("Authorization", auth);
    int httpResponseCode = https.PUT("");
    bool success = false;
    // Check if the request was successful
    if (httpResponseCode == 204) {
      Serial.println((isPlaying ? "Playing" : "Pausing"));
      success = true;
    } else {
      Serial.print("Error pausing or playing: ");
      Serial.println(httpResponseCode);
      String response = https.getString();
      Serial.println(response);
    }

    // Disconnect from the Spotify API
    https.end();
    getTrackInfo();
    return success;
  }

  bool adjustVolume(int vol) {
    String url = "https://api.spotify.com/v1/me/player/volume?volume_percent=" + String(vol);
    https.begin(*client, url);
    String auth = "Bearer " + String(accessToken);
    https.addHeader("Authorization", auth);
    int httpResponseCode = https.PUT("");
    bool success = false;
    // Check if the request was successful
    if (httpResponseCode == 204) {
      currVol = vol;
      success = true;
    } else if (httpResponseCode == 403) {
      currVol = vol;
      success = false;
      Serial.print("Error setting volume: ");
      Serial.println(httpResponseCode);
      String response = https.getString();
      Serial.println(response);
    } else {
      Serial.print("Error setting volume: ");
      Serial.println(httpResponseCode);
      String response = https.getString();
      Serial.println(response);
    }

    // Disconnect from the Spotify API
    https.end();
    return success;
  }

  bool skipForward() {
    String url = "https://api.spotify.com/v1/me/player/next";
    https.begin(*client, url);
    String auth = "Bearer " + String(accessToken);
    https.addHeader("Authorization", auth);
    int httpResponseCode = https.POST("");
    bool success = false;
    // Check if the request was successful
    if (httpResponseCode == 204) {
      Serial.println("skipping forward");
      success = true;
    } else {
      Serial.print("Error skipping forward: ");
      Serial.println(httpResponseCode);
      String response = https.getString();
      Serial.println(response);
    }

    // Disconnect from the Spotify API
    https.end();
    getTrackInfo();
    return success;
  }

  bool skipBack() {
    String url = "https://api.spotify.com/v1/me/player/previous";
    https.begin(*client, url);
    String auth = "Bearer " + String(accessToken);
    https.addHeader("Authorization", auth);
    int httpResponseCode = https.POST("");
    bool success = false;
    // Check if the request was successful
    if (httpResponseCode == 204) {
      Serial.println("skipping backward");
      success = true;
    } else {
      Serial.print("Error skipping backward: ");
      Serial.println(httpResponseCode);
      String response = https.getString();
      Serial.println(response);
    }

    // Disconnect from the Spotify API
    https.end();
    getTrackInfo();
    return success;
  }

  bool likeSong(String songId) {
    String url = "https://api.spotify.com/v1/me/tracks?ids=" + songId;
    https.begin(*client, url);
    String auth = "Bearer " + String(accessToken);
    https.addHeader("Authorization", auth);
    https.addHeader("Content-Type", "application/json");
    char requestBody[] = "{\"ids\":[\"string\"]}";
    int httpResponseCode = https.PUT(requestBody);
    bool success = false;
    // Check if the request was successful
    if (httpResponseCode == 200) {
      Serial.println("added track to liked songs");
      success = true;
    } else {
      Serial.print("Error adding to liked songs: ");
      Serial.println(httpResponseCode);
      String response = https.getString();
      Serial.println(response);
    }

    // Disconnect from the Spotify API
    https.end();
    return success;
  }

  bool dislikeSong(String songId) {
    String url = "https://api.spotify.com/v1/me/tracks?ids=" + songId;
    https.begin(*client, url);
    String auth = "Bearer " + String(accessToken);
    https.addHeader("Authorization", auth);
    int httpResponseCode = https.DELETE();
    bool success = false;
    // Check if the request was successful
    if (httpResponseCode == 200) {
      Serial.println("removed liked songs");
      success = true;
    } else {
      Serial.print("Error removing from liked songs: ");
      Serial.println(httpResponseCode);
      String response = https.getString();
      Serial.println(response);
    }

    // Disconnect from the Spotify API
    https.end();
    return success;
  }

  bool accessTokenSet = false;
  long tokenStartTime;
  int tokenExpireTime;
  songDetails currentSong;
  float currentSongPositionMs;
  float lastSongPositionMs;
  int currVol;
  String CLIENT_ID;
  String CLIENT_SECRET;
  String REDIRECT_URI;
  TFT_eSPI &tft;
  char *parts[10];
private:
  std::unique_ptr<BearSSL::WiFiClientSecure> client;
  HTTPClient https;
  bool isPlaying = false;
  String accessToken;
  String refreshToken;
};