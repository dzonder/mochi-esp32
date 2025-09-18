#pragma once

// --- WiFi Configuration ---
#define WIFI_SSID "Wokwi-GUEST"
#define WIFI_PASS ""

// --- Web Server Configuration ---
#define WEB_SERVER_PORT 5001

// --- Hardware Pins (ESP32-C3) ---
// I2C for SSD1306 Display
#define OLED_SDA_PIN 8
#define OLED_SCL_PIN 9
#define OLED_RESET_PIN -1 // -1 if not used

// Buzzer Pin
#define BUZZER_PIN 6

// Touch Button Pin
#define TOUCH_PIN 1

// --- Display Configuration ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define FRAME_BUFFER_SIZE (SCREEN_WIDTH * SCREEN_HEIGHT / 8)
#define FRAME_DELAY_MS 100 // Delay between animation frames (10 FPS)
#define MAX_ANIMATION_FRAMES 20

// --- Touch Sensor Configuration ---
#define TOUCH_DEBOUNCE_MS 2000 // 2 seconds
#define TOUCH_TARGET_URL "http://host.wokwi.internal:5000/touch"