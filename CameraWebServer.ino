// Optimized ESP32-S3 Camera WebServer with max resolution printed

#define CAMERA_MODEL_ESP32S3_EYE

#include "esp_camera.h"
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include "camera_pins.h"  // pin definitions for the selected camera

// WiFi credentials
const char* ssid = "io";
const char* password = "hhhhhh90";

// Async web server
AsyncWebServer server(80);

// Map frame sizes to readable strings
const char* getFrameSizeName(framesize_t size) {
    switch(size){
        case FRAMESIZE_QQVGA: return "QQVGA 160x120";
        case FRAMESIZE_QCIF:  return "QCIF 176x144";
        case FRAMESIZE_HQVGA: return "HQVGA 240x176";
        case FRAMESIZE_QVGA:  return "QVGA 320x240";
        case FRAMESIZE_CIF:   return "CIF 400x296";
        case FRAMESIZE_HVGA:  return "HVGA 480x320";
        case FRAMESIZE_VGA:   return "VGA 640x480";
        case FRAMESIZE_SVGA:  return "SVGA 800x600";
        case FRAMESIZE_XGA:   return "XGA 1024x768";
        case FRAMESIZE_HD:    return "HD 1280x720";
        case FRAMESIZE_UXGA:  return "UXGA 1600x1200";
        default: return "Unknown";
    }
}

// ----------------------------------------------------------------------------
// Camera config
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
    config.pixel_format = PIXFORMAT_JPEG;  // JPEG for light streaming
    config.frame_size = FRAMESIZE_VGA;     // streaming resolution
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.jpeg_quality = 12;              // moderate quality
    config.fb_count = 3;                   // triple buffer
    config.grab_mode = CAMERA_GRAB_LATEST;

    if (esp_camera_init(&config) != ESP_OK) {
        Serial.println("Camera init failed");
        while(true) delay(1000);
    }

    sensor_t* s = esp_camera_sensor_get();
    s->set_vflip(s, 1);

    // Print maximum supported resolution
    Serial.print("Max camera resolution: ");
    Serial.println(getFrameSizeName(FRAMESIZE_UXGA));  // ESP32-S3-EYE max is UXGA
}

// ----------------------------------------------------------------------------
// MJPEG stream
void handleStream(AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginChunkedResponse(
        "multipart/x-mixed-replace; boundary=frameboundary",
        [](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
            camera_fb_t *fb = esp_camera_fb_get();
            if(!fb) return 0;

            String header = "--frameboundary\r\nContent-Type: image/jpeg\r\nContent-Length: " + String(fb->len) + "\r\n\r\n";
            size_t headerLen = header.length();
            if(headerLen > maxLen) headerLen = maxLen;
            memcpy(buffer, header.c_str(), headerLen);

            size_t copyLen = fb->len;
            if(headerLen + copyLen > maxLen) copyLen = maxLen - headerLen;
            memcpy(buffer + headerLen, fb->buf, copyLen);

            esp_camera_fb_return(fb);
            return headerLen + copyLen;
        }
    );
    response->addHeader("Cache-Control", "no-cache");
    request->send(response);
}

// ----------------------------------------------------------------------------
// Root page
void handleRoot(AsyncWebServerRequest *request) {
    sensor_t* s = esp_camera_sensor_get();
    String html = "<html><head><title>ESP32 Camera</title></head><body>";
    html += "<h1>ESP32 Camera Web Server</h1>";
    html += "<p>Streaming resolution: " + String(getFrameSizeName(s->status.framesize)) + "</p>";
    html += "<p>Max resolution: " + String(getFrameSizeName(FRAMESIZE_UXGA)) + "</p>";
    html += "<p><img src='/stream' width='640'></p>";
    html += "</body></html>";
    request->send(200, "text/html", html);
}

// ----------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    Serial.println("Starting camera...");

    startCamera();

    // WiFi
    WiFi.begin(ssid, password);
    WiFi.setSleep(false);
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi connected: " + WiFi.localIP().toString());

    // Web server routes
    server.on("/", HTTP_GET, handleRoot);
    server.on("/stream", HTTP_GET, handleStream);
    server.onNotFound([](AsyncWebServerRequest *request){
        request->send(404, "text/plain", "Not found");
    });

    server.begin();
    Serial.println("Camera ready! Open browser at http://" + WiFi.localIP().toString());
}

// ----------------------------------------------------------------------------
void loop() {
    // Nothing needed here with AsyncWebServer
}
