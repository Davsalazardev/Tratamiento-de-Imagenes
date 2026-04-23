#include "esp_camera.h"
#include "img_converters.h"
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
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) return;

  if (strcmp(filter_name, "original") == 0) {
      Serial.println("CAPTURE_START");
      Serial.write(fb->buf, fb->len);
      Serial.println("\nCAPTURE_END");
      esp_camera_fb_return(fb);
      return;
  }

  // Reservar RAM para transformar JPEG a pixel RGB (pesado para ESP32)
  uint8_t *rgb_buf = (uint8_t *)malloc(fb->width * fb->height * 3);
  if (!rgb_buf) {
      Serial.println("DEBUG: Memoria insuficiente para RGB");
      esp_camera_fb_return(fb);
      return;
  }

  fmt2rgb888(fb->buf, fb->len, fb->format, rgb_buf);
  int w = fb->width, h = fb->height, len = w * h * 3;

  // --- FILTROS 100% C++ EN ESP32 ---
  // Nota: El decodificador JPEG de la OV2640 en el ESP32 guarda los colores en orden BGR
  // (Azul = i, Verde = i+1, Rojo = i+2)
  if (strcmp(filter_name, "invert") == 0) {
      for (int i = 0; i < len; i++) rgb_buf[i] = 255 - rgb_buf[i];
  } 
  else if (strcmp(filter_name, "grayscale") == 0) {
      for (int i = 0; i < len; i += 3) {
          uint8_t g = (rgb_buf[i]*1 + rgb_buf[i+1]*6 + rgb_buf[i+2]*3) / 10;
          rgb_buf[i] = rgb_buf[i+1] = rgb_buf[i+2] = g;
      }
  } 
  else if (strcmp(filter_name, "sepia") == 0) {
      for (int i = 0; i < len; i += 3) {
          int b = rgb_buf[i], g = rgb_buf[i+1], r = rgb_buf[i+2]; // Leer BGR real
          rgb_buf[i+2] = min(255, (int)(r*.393 + g*.769 + b*.189)); // R
          rgb_buf[i+1] = min(255, (int)(r*.349 + g*.686 + b*.168)); // G
          rgb_buf[i]   = min(255, (int)(r*.272 + g*.534 + b*.131)); // B
      }
  }
  else if (strcmp(filter_name, "red_tint") == 0) {
      for (int i = 0; i < len; i += 3) {
          rgb_buf[i+2] = min(255, rgb_buf[i+2] + 80); // Sumar a Rojo (i+2)
          rgb_buf[i+1] = max(0, (int)rgb_buf[i+1] - 40); // Restar Verde
          rgb_buf[i]   = max(0, (int)rgb_buf[i] - 40); // Restar Azul (i)
      }
  }
  else if (strcmp(filter_name, "green_tint") == 0) {
      for (int i = 0; i < len; i += 3) {
          rgb_buf[i+1] = min(255, rgb_buf[i+1] + 80); // Sumar Verde (i+1)
          rgb_buf[i+2] = max(0, (int)rgb_buf[i+2] - 40); // Restar Rojo (i+2)
          rgb_buf[i]   = max(0, (int)rgb_buf[i] - 40); // Restar Azul
      }
  }
  else if (strcmp(filter_name, "blue_tint") == 0) {
      for (int i = 0; i < len; i += 3) {
          rgb_buf[i]   = min(255, rgb_buf[i] + 80); // Sumar Azul (i)
          rgb_buf[i+2] = max(0, (int)rgb_buf[i+2] - 40); // Restar Rojo
          rgb_buf[i+1] = max(0, (int)rgb_buf[i+1] - 40); // Restar Verde
      }
  }
  else if (strcmp(filter_name, "contraste") == 0) {
      float factor = 1.5;
      for (int i = 0; i < len; i++) {
          rgb_buf[i] = min(255, max(0, (int)(factor * ((int)rgb_buf[i] - 128) + 128)));
      }
  }
  else if (strcmp(filter_name, "soleado") == 0) { // Simular calidez
      for (int i = 0; i < len; i += 3) {
          rgb_buf[i+2] = min(255, rgb_buf[i+2] + 40); // Sumar Rojo (i+2)
          rgb_buf[i+1] = min(255, rgb_buf[i+1] + 20); // Sumar Verde
          rgb_buf[i]   = max(0, (int)rgb_buf[i] - 20);  // Bajar Azul! (Filtro calido)
      }
  }
  else if (strcmp(filter_name, "lineas") == 0) {
      uint8_t *tmp = (uint8_t*)malloc(len);
      if (tmp) {
          for (int y = 0; y < h - 1; y++) {
              for (int x = 0; x < w - 1; x++) {
                  int idx = (y * w + x) * 3;
                  int diff_x = abs(rgb_buf[idx] - rgb_buf[idx+3]);
                  int diff_y = abs(rgb_buf[idx] - rgb_buf[idx + w*3]);
                  uint8_t edge = (diff_x + diff_y > 40) ? 255 : 0;
                  tmp[idx] = tmp[idx+1] = tmp[idx+2] = edge;
              }
          }
          memcpy(rgb_buf, tmp, len);
          free(tmp);
      }
  }
  else if (strcmp(filter_name, "profundidad") == 0) {
      for (int i = 0; i < len; i += 3) {
          uint8_t g = (rgb_buf[i]*1 + rgb_buf[i+1]*6 + rgb_buf[i+2]*3) / 10;
          // Mapa de calor térmico real (Jet Colormap clásico en C++)
          uint8_t r = 0, gr = 0, b = 0;
          if (g < 64) {
              r = 0; gr = g * 4; b = 255;
          } else if (g < 128) {
              r = 0; gr = 255; b = 255 - (g - 64) * 4;
          } else if (g < 192) {
              r = (g - 128) * 4; gr = 255; b = 0;
          } else {
              r = 255; gr = 255 - (g - 192) * 4; b = 0;
          }
          rgb_buf[i+2] = r;  // Rojo
          rgb_buf[i+1] = gr; // Verde
          rgb_buf[i]   = b;  // Azul
      }
  }
  else if (strcmp(filter_name, "test_patron") == 0) {
      for (int y = 0; y < h; y++) {
          for (int x = 0; x < w; x++) {
              int i = (y * w + x) * 3;
              if (x < w/3) { rgb_buf[i+2]=255; rgb_buf[i+1]=0; rgb_buf[i]=0; } // R
              else if (x < 2*w/3) { rgb_buf[i+2]=0; rgb_buf[i+1]=255; rgb_buf[i]=0; } // G
              else { rgb_buf[i+2]=0; rgb_buf[i+1]=0; rgb_buf[i]=255; } // B
          }
      }
  }

  // Comprimir de nuevo RGB888 a JPEG
  uint8_t *jpg_buf = NULL;
  size_t jpg_len = 0;
  fmt2jpg(rgb_buf, len, w, h, PIXFORMAT_RGB888, 20, &jpg_buf, &jpg_len);

  Serial.println("CAPTURE_START");
  if (jpg_buf != NULL && jpg_len > 0) {
      Serial.write(jpg_buf, jpg_len);
      free(jpg_buf);
  }
  Serial.println("\nCAPTURE_END");

  free(rgb_buf);
  esp_camera_fb_return(fb);
}
