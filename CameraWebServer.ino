// esp32_camera_github_tiles.ino
// Dependencies:
//  - esp_camera
//  - ESPAsyncWebServer
//  - TJpg_Decoder
//  - JPEGEncoder
//  - Preferences (built-in)

#define CAMERA_MODEL_ESP32S3_EYE

#include "esp_camera.h"
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "camera_pins.h"  // keep your pin defs here

// Third party libs (install):
// #include <TJpg_Decoder.h>   // JPEG->RGB decoder
// #include <JPEGEncoder.h>    // RGB->JPEG encoder (tile encoding)

#include <base64.h>  // Arduino base64 helper if available, else implement below

// Config keys
Preferences prefs;

// Config keys used in NVS
const char *KEY_SSID = "ssid";
const char *KEY_PASS = "pass";
const char *KEY_GHTOKEN = "ghtoken";
const char *KEY_GHOWNER = "ghowner";
const char *KEY_GHREPO = "ghrepo";
const char *KEY_BRANCH = "branch";

AsyncWebServer server(80);

// tile size
const int TILE_W = 50;
const int TILE_H = 50;

// deep sleep interval (3600 seconds = 1 hour)
const uint64_t SLEEP_US = 3600ULL * 1000000ULL;

void startCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  // pick a resolution that fits memory; try FRAMESIZE_VGA or FRAMESIZE_SVGA etc.
  config.frame_size = FRAMESIZE_VGA;  // change to UXGA ONLY if you have enough PSRAM
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 8;
  config.fb_count = 2;
  config.grab_mode = CAMERA_GRAB_LATEST;

  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("Camera init failed");
    while (true) delay(1000);
  }
  sensor_t *s = esp_camera_sensor_get();
  s->set_vflip(s, 1);
}

String makeConfigPage(String message = "") {
  String html = "<!doctype html><html><head><meta charset='utf-8'><title>ESP32-Cam Config</title></head><body>";
  html += "<h2>Configure WiFi & GitHub</h2>";
  if (message.length()) html += "<p><b>" + message + "</b></p>";
  html += "<form method='POST' action='/save'>";
  html += "SSID: <input name='ssid'><br>";
  html += "Password: <input name='pass'><br>";
  html += "GitHub owner (username/org): <input name='ghowner'><br>";
  html += "GitHub repo: <input name='ghrepo'><br>";
  html += "GitHub token (personal access token or OAuth token): <input name='ghtoken'><br>";
  html += "Branch (optional, default 'main'): <input name='branch' value='main'><br>";
  html += "<input type='submit' value='Save & Reboot'></form></body></html>";
  return html;
}

void startConfigPortal() {
  WiFi.softAP("ESP32-CAM-SETUP");
  IPAddress IP = WiFi.softAPIP();
  Serial.println("AP mode, connect to " + IP.toString() + " and open /config");

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->redirect("/config");
  });
  server.on("/config", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", makeConfigPage());
  });
  server.on("/save", HTTP_POST, [](AsyncWebServerRequest *request) {
    String ssid = "", pass = "", ghowner = "", ghrepo = "", ghtoken = "", branch = "main";
    if (request->hasParam("ssid", true)) ssid = request->getParam("ssid", true)->value();
    if (request->hasParam("pass", true)) pass = request->getParam("pass", true)->value();
    if (request->hasParam("ghowner", true)) ghowner = request->getParam("ghowner", true)->value();
    if (request->hasParam("ghrepo", true)) ghrepo = request->getParam("ghrepo", true)->value();
    if (request->hasParam("ghtoken", true)) ghtoken = request->getParam("ghtoken", true)->value();
    if (request->hasParam("branch", true)) branch = request->getParam("branch", true)->value();

    prefs.begin("cfg", false);
    prefs.putString(KEY_SSID, ssid);
    prefs.putString(KEY_PASS, pass);
    prefs.putString(KEY_GHTOKEN, ghtoken);
    prefs.putString(KEY_GHOWNER, ghowner);
    prefs.putString(KEY_GHREPO, ghrepo);
    prefs.putString(KEY_BRANCH, branch);
    prefs.end();

    request->send(200, "text/html", "<html><body><h3>Saved. Rebooting...</h3></body></html>");
    delay(1500);
    ESP.restart();
  });

  server.begin();
}

// Utility: base64 encode (if library not avail)
String base64Encode(const uint8_t *buf, size_t len) {
  // use Arduino base64 library if available; fallback simple encoder
  String out;
  size_t olen = 4 * ((len + 2) / 3);
  out.reserve(olen + 1);
  static const char *b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  uint32_t val = 0;
  int valb = -6;
  for (size_t i = 0; i < len; i++) {
    val = (val << 8) + buf[i];
    valb += 8;
    while (valb >= 0) {
      out += b64[(val >> valb) & 0x3F];
      valb -= 6;
    }
  }
  if (valb > -6) {
    out += b64[((val << 8) >> (valb + 8)) & 0x3F];
  }
  while (out.length() % 4) out += '=';
  return out;
}

// Upload a single JPEG buffer to GitHub (PUT /repos/{owner}/{repo}/contents/{path})
bool githubUploadFile(const String &owner, const String &repo, const String &branch, const String &path, const uint8_t *jpegBuf, size_t jpegLen, const String &token) {
  String url = "https://api.github.com/repos/" + owner + "/" + repo + "/contents/" + path;
  // Prepare JSON payload
  String contentBase64 = base64Encode(jpegBuf, jpegLen);
  String msg = "Upload by ESP32 - " + path;

  String payload = "{";
  payload += "\"message\":\"" + msg + "\",";
  payload += "\"content\":\"" + contentBase64 + "\"";
  if (branch.length()) payload += ",\"branch\":\"" + branch + "\"";
  payload += "}";

  // Use WiFiClientSecure + HTTPClient
  WiFiClientSecure client;
  client.setInsecure();  // NOTE: for production, replace with real CA verification
  HTTPClient https;
  https.begin(client, url);
  https.addHeader("User-Agent", "ESP32-Cam-Uploader");
  https.addHeader("Accept", "application/vnd.github+json");
  https.addHeader("Authorization", "token " + token);
  https.addHeader("Content-Type", "application/json");

  int code = https.PUT((uint8_t *)payload.c_str(), payload.length());
  String resp = https.getString();
  Serial.printf("PUT %s -> %d\n", path.c_str(), code);
  if (code >= 200 && code < 300) {
    https.end();
    return true;
  } else {
    Serial.println("GitHub upload failed: " + String(code) + " / " + resp);
    https.end();
    return false;
  }
}

// NOTE: the following is a conceptual implementation: decoding and re-encoding require TJpg_Decoder and JPEGEncoder.
// Here we show how you'd orchestrate things; please install TJpg_Decoder and JPEGEncoder and adapt callback functions per the libraries' API.

bool captureAndUploadTiles() {
  prefs.begin("cfg", true);
  String token = prefs.getString(KEY_GHTOKEN, "");
  String owner = prefs.getString(KEY_GHOWNER, "");
  String repo = prefs.getString(KEY_GHREPO, "");
  String branch = prefs.getString(KEY_BRANCH, "main");
  prefs.end();

  if (token.length() < 10 || owner.length() == 0 || repo.length() == 0) {
    Serial.println("Missing GitHub config");
    return false;
  }

  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    return false;
  }
  if (fb->format != PIXFORMAT_JPEG) {
    Serial.println("Frame not JPEG");
    esp_camera_fb_return(fb);
    return false;
  }

  // Use TJpg_Decoder to decode JPEG bytes into raw RGB buffer in PSRAM
  // PSEUDOCODE: The actual TJpgDec API requires setting output buffer and callback.
  // We'll outline the steps here:
  //
  // 1) Decode fb->buf (length fb->len) into an RGB buffer `rgbBuf` sized width*height*3
  // 2) For each tile (tx,ty), create tile buffer tileW*tileH*3 by copying the right region from rgbBuf
  // 3) Call JPEGEncoder to encode the RGB tile -> jpeg bytes
  // 4) Call githubUploadFile(owner, repo, branch, "track-my-photo/x-y.jpg", jpegBuf, jpegLen, token)
  //
  // Because the exact API of the libraries may vary, you must adapt the callback names and encoding calls to what's installed.

  // *** Example pseudo-implementation below - adapt with the real library calls ***

  int width = fb->width;
  int height = fb->height;
  Serial.printf("Captured image: %dx%d, %d bytes\n", width, height, fb->len);

  // Check memory: allocate raw buffer
  size_t rgbSize = (size_t)width * height * 3;
  uint8_t *rgbBuf = (uint8_t *)ps_malloc(rgbSize);
  if (!rgbBuf) {
    Serial.println("Failed to allocate RGB buffer. Try lower resolution or enable PSRAM.");
    esp_camera_fb_return(fb);
    return false;
  }

  // Decode JPEG into rgbBuf (library specific)
  // --- pseudo-call: decodeJpegToRGB(fb->buf, fb->len, rgbBuf, width, height);
  // Replace with your TJpgDec usage. Example TJpgDec usage:
  // TJpgDec.decodeArray(fb->buf, fb->len, tjpg_output_function);
  // where tjpg_output_function writes to rgbBuf

  bool decodeOk = false;
  // --- TODO: do actual decode here ---
  // For now, show a message and abort:
  Serial.println("DECODE STEP: You must implement decoding from JPEG to raw RGB using TJpg_Decoder.");
  free(rgbBuf);
  esp_camera_fb_return(fb);
  return false;

  // Once rgbBuf is filled, continue as below (pseudocode):
  /*
    for (int y=0; y < height; y += TILE_H) {
        for (int x=0; x < width; x += TILE_W) {
            int tileW = min(TILE_W, width - x);
            int tileH = min(TILE_H, height - y);
            // allocate tileRGB
            uint8_t *tileRGB = (uint8_t*)malloc(tileW * tileH * 3);
            // copy data from rgbBuf to tileRGB
            for (int ty=0; ty<tileH; ++ty) {
                memcpy(tileRGB + ty * tileW * 3,
                       rgbBuf + ((y + ty) * width + x) * 3,
                       tileW*3);
            }
            // encode tileRGB -> JPEG (use JPEGEncoder)
            // size_t jpegLen = jpeg_encode(tileRGB, tileW, tileH, &jpegBuf);
            // upload to GitHub path "track-my-photo/x-y.jpg"
            String path = "track-my-photo/" + String(x) + "-" + String(y) + ".jpg";
            bool ok = githubUploadFile(owner, repo, branch, path, jpegBuf, jpegLen, token);
            free(tileRGB); free(jpegBuf);
            if (!ok) { Serial.println("Upload failed for " + path); }
            delay(200); // small delay between uploads
        }
    }
    free(rgbBuf);
    esp_camera_fb_return(fb);
    return true;
    */
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n=== ESP32 Camera GitHub Tile Uploader ===");
  startCamera();

  prefs.begin("cfg", true);
  String ssid = prefs.getString(KEY_SSID, "");
  prefs.end();

  if (ssid.length() == 0) {
    // no config -> start config portal
    startConfigPortal();
    Serial.println("Started config portal. Connect to WiFi: ESP32-CAM-SETUP");
    // block here - portal handles saving and rebooting
    // keep loop empty
    while (true) { delay(1000); }
  }

  // connect to WiFi
  prefs.begin("cfg", true);
  String password = prefs.getString(KEY_PASS, "");
  prefs.end();

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  WiFi.setSleep(false);
  Serial.print("Connecting to WiFi");
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (millis() - start > 20000) {
      Serial.println("\nFailed to connect, rebooting to config portal.");
      delay(500);
      // erase saved SSID to trigger portal at next boot (optional)
      prefs.begin("cfg", false);
      prefs.remove(KEY_SSID);
      prefs.end();
      ESP.restart();
    }
  }
  Serial.println("\nWiFi connected: " + WiFi.localIP().toString());

  // If we woke from deep sleep, perform capture/upload then sleep
  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER) {
    Serial.println("Woke from deep sleep - capturing and uploading...");
    bool ok = captureAndUploadTiles();
    Serial.println(ok ? "Upload done" : "Upload encountered errors");
    Serial.println("Entering deep sleep for 1 hour...");
    esp_sleep_enable_timer_wakeup(SLEEP_US);
    delay(100);
    esp_deep_sleep_start();
  } else {
    // Normal boot: schedule first upload now and then deep sleep
    Serial.println("Normal boot, performing first capture/upload...");
    bool ok = captureAndUploadTiles();
    Serial.println(ok ? "Upload done" : "Upload encountered errors");
    Serial.println("Entering deep sleep for 1 hour...");
    esp_sleep_enable_timer_wakeup(SLEEP_US);
    delay(100);
    esp_deep_sleep_start();
  }
}

void loop() {
  // Nothing here - deep sleep cycles handle behavior
}
