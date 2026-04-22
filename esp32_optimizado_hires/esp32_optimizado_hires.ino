#include "esp_camera.h"
#include <HardwareSerial.h>
#include <esp_task_wdt.h>

#define BAUD_RATE         115200

bool streaming = false;
uint32_t frame_count = 0;
uint32_t error_count = 0;
uint32_t fps_start_time = 0;
uint32_t fps_frame_count = 0;

char cmd_buffer[64];
int cmd_idx = 0;
char current_filter[32] = "original";

#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

void setup_camera();
void capture_and_send_stream();
void capture_and_filter(const char *filter_name);
void process_command(const char *cmd);

void setup() {
  Serial.begin(BAUD_RATE);
  Serial.setDebugOutput(false);
  Serial.setRxBufferSize(4096);
  
  delay(2000);
  
  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║ ESP32-CAM ULTRA-OPTIMIZADO (HiRes)     ║");
  Serial.println("║ Stream QVGA JPEG + FILTROS             ║");
  Serial.println("╚════════════════════════════════════════╝\n");
  
  setup_camera();
  
  esp_task_wdt_add(NULL);
  fps_start_time = millis();
}

void loop() {
  esp_task_wdt_reset();
  
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (cmd_idx > 0) {
        cmd_buffer[cmd_idx] = '\0';
        process_command(cmd_buffer);
        cmd_idx = 0;
      }
    } else if (cmd_idx < 63) {
      cmd_buffer[cmd_idx++] = c;
    }
  }
  
  if (streaming) {
    capture_and_send_stream();
    fps_frame_count++;
    frame_count++;
    
    if (fps_frame_count % 60 == 0) {
      fps_frame_count = 0;
      fps_start_time = millis();
    }
  } else {
    delay(5);
  }
}

void process_command(const char *cmd) {
  String s(cmd);
  s.toLowerCase();
  s.trim();
  
  if (s.startsWith("stream start")) {
    streaming = true;
    Serial.println("▶️  STREAM INICIADO");
  }
  else if (s.startsWith("stream stop")) {
    streaming = false;
    Serial.println("⏹️  STREAM DETENIDO");
  }
  else if (s.startsWith("capture")) {
    streaming = false;
    int space_idx = s.indexOf(' ');
    if (space_idx > 0) {
        String filter = s.substring(space_idx + 1);
        filter.trim();
        strncpy(current_filter, filter.c_str(), 31);
        current_filter[31] = '\0';
    } else {
        strcpy(current_filter, "original");
    }
    Serial.printf("📸 Capturando con filtro: %s\n", current_filter);
    capture_and_filter(current_filter);
  }
}

void setup_camera() {
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
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  
  config.pixel_format = PIXFORMAT_JPEG; 
  config.frame_size = FRAMESIZE_QVGA;  
  config.jpeg_quality = 12;           
  config.fb_count = 2;
  config.grab_mode = CAMERA_GRAB_LATEST;
  
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("❌ Error cámara: 0x%x\n", err);
    return;
  }
  Serial.println("✅ Cámara OK");
}

void IRAM_ATTR capture_and_send_stream() {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) return;
  Serial.write(fb->buf, fb->len);
  esp_camera_fb_return(fb);
  delay(60); 
}

void capture_and_filter(const char *filter_name) {
  sensor_t * s = esp_camera_sensor_get();
  if (!s) {
      Serial.println("DEBUG: Error obteniendo el sensor de hardware");
      return;
  }

  s->set_special_effect(s, 0);
  s->set_contrast(s, 0);
  s->set_brightness(s, 0);
  s->set_saturation(s, 0);
  s->set_wb_mode(s, 0);
  s->set_colorbar(s, 0);

  if (strcmp(filter_name, "invert") == 0) s->set_special_effect(s, 1);
  else if (strcmp(filter_name, "grayscale") == 0) s->set_special_effect(s, 2);
  else if (strcmp(filter_name, "red_tint") == 0) s->set_special_effect(s, 3);
  else if (strcmp(filter_name, "green_tint") == 0) s->set_special_effect(s, 4);
  else if (strcmp(filter_name, "blue_tint") == 0) s->set_special_effect(s, 5);
  else if (strcmp(filter_name, "sepia") == 0) s->set_special_effect(s, 6);
  else if (strcmp(filter_name, "contraste") == 0) s->set_contrast(s, 2);
  else if (strcmp(filter_name, "soleado") == 0) s->set_wb_mode(s, 1);
  else if (strcmp(filter_name, "test_patron") == 0) s->set_colorbar(s, 1); 
  
  delay(300); 

  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
      Serial.println("DEBUG: Fallo al obtener frame JPEG");
      s->set_special_effect(s, 0); 
      return;
  }
  
  Serial.println("CAPTURE_START");
  Serial.write(fb->buf, fb->len);
  Serial.println("\nCAPTURE_END");
  
  esp_camera_fb_return(fb);
  
  s->set_special_effect(s, 0);
  s->set_contrast(s, 0);
  s->set_brightness(s, 0);
  s->set_saturation(s, 0);
  s->set_wb_mode(s, 0);
  s->set_colorbar(s, 0);
}
