#include <Arduino.h>
// esp32_camera_github_uploader.ino
#define CAMERA_MODEL_ESP32S3_EYE
#include "esp_camera.h"
#include "camera_pins.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <base64.h>

Preferences prefs;

const uint64_t SLEEP_US = 3600ULL * 1000000ULL; // 1 hour sleep
AsyncWebServer server(80);

void startCamera() {
  camera_config_t c;
  c.ledc_channel = LEDC_CHANNEL_0;
  c.ledc_timer = LEDC_TIMER_0;
  c.pin_d0 = Y2_GPIO_NUM;
  c.pin_d1 = Y3_GPIO_NUM;
  c.pin_d2 = Y4_GPIO_NUM;
  c.pin_d3 = Y5_GPIO_NUM;
  c.pin_d4 = Y6_GPIO_NUM;
  c.pin_d5 = Y7_GPIO_NUM;
  c.pin_d6 = Y8_GPIO_NUM;
  c.pin_d7 = Y9_GPIO_NUM;
  c.pin_xclk = XCLK_GPIO_NUM;
  c.pin_pclk = PCLK_GPIO_NUM;
  c.pin_vsync = VSYNC_GPIO_NUM;
  c.pin_href = HREF_GPIO_NUM;
  c.pin_sccb_sda = SIOD_GPIO_NUM;
  c.pin_sccb_scl = SIOC_GPIO_NUM;
  c.pin_pwdn = PWDN_GPIO_NUM;
  c.pin_reset = RESET_GPIO_NUM;
  c.xclk_freq_hz = 20000000;
  c.pixel_format = PIXFORMAT_JPEG;
  c.frame_size = FRAMESIZE_UXGA;
  c.jpeg_quality = 12;
  c.fb_count = 1;
  c.fb_location = CAMERA_FB_IN_PSRAM;
  c.grab_mode = CAMERA_GRAB_LATEST;
  if (esp_camera_init(&c) != ESP_OK) {
    Serial.println("Camera init failed");
    while (1) delay(1000);
  }
}

bool githubUpload(const String &owner, const String &repo, const String &token,
                  const String &path, const uint8_t *buf, size_t len) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient https;
  String url =
      "https://api.github.com/repos/" + owner + "/" + repo + "/contents/" + path;
  String content = base64::encode(buf, len);
  String body = "{\"message\":\"ESP32 upload\",\"content\":\"" + content + "\"}";
  https.begin(client, url);
  https.addHeader("User-Agent", "esp32cam");
  https.addHeader("Authorization", "token " + token);
  https.addHeader("Content-Type", "application/json");
  int code = https.PUT((uint8_t *)body.c_str(), body.length());
  String res = https.getString();
  https.end();
  Serial.printf("Upload %s -> %d\n", path.c_str(), code);
  if (code >= 200 && code < 300) return true;
  Serial.println(res);
  return false;
}

void captureAndUpload() {
  prefs.begin("cfg", true);
  String ssid = prefs.getString("ssid");
  String pass = prefs.getString("pass");
  String owner = prefs.getString("ghowner");
  String repo = prefs.getString("ghrepo");
  String token = prefs.getString("ghtoken");
  prefs.end();

  if (ssid == "" || token == "") return;

  WiFi.begin(ssid.c_str(), pass.c_str());
  Serial.print("WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("ok");

  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Failed capture");
    return;
  }

  // Upload one full photo (you can later split client-side)
  String fname = "track-my-photo/photo-" + String(millis()) + ".jpg";
  githubUpload(owner, repo, token, fname, fb->buf, fb->len);
  esp_camera_fb_return(fb);
  WiFi.disconnect(true);
}

void setup() {
  Serial.begin(115200);
  startCamera();

  prefs.begin("cfg", true);
  if (prefs.getString("ssid", "") == "") {
    Serial.println("⚙️  Connect to ESP32-SETUP to configure WiFi & GitHub token.");
    WiFi.softAP("ESP32-SETUP");
    IPAddress ip = WiFi.softAPIP();
    AsyncWebServer server(80);
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *r) {
      String html = "<form action='/save' method='POST'>SSID:<input name='ssid'><br>"
                    "Pass:<input name='pass'><br>"
                    "Owner:<input name='ghowner'><br>"
                    "Repo:<input name='ghrepo'><br>"
                    "Token:<input name='ghtoken'><br>"
                    "<input type='submit' value='Save'></form>";
      r->send(200, "text/html", html);
    });
    server.on("/save", HTTP_POST, [](AsyncWebServerRequest *r) {
      prefs.begin("cfg", false);
      prefs.putString("ssid", r->getParam("ssid", true)->value());
      prefs.putString("pass", r->getParam("pass", true)->value());
      prefs.putString("ghowner", r->getParam("ghowner", true)->value());
      prefs.putString("ghrepo", r->getParam("ghrepo", true)->value());
      prefs.putString("ghtoken", r->getParam("ghtoken", true)->value());
      prefs.end();
      r->send(200, "text/html", "Saved! Rebooting...");
      delay(2000);
      ESP.restart();
    });
    server.begin();
    while (true) delay(1000);
  }
  prefs.end();

  captureAndUpload();
  Serial.println("Sleeping...");
  esp_sleep_enable_timer_wakeup(SLEEP_US);
  esp_deep_sleep_start();
}

void loop() {}
