#include <HardwareSerial.h>
#include "esp_camera.h"
#include "quirc.h"

// QR Code handling
struct quirc *q = NULL;
camera_fb_t *fb = NULL;
struct quirc_code code;
struct quirc_data qrData;

// GPIO Definitions
#define BUTTON_PIN 15 // GPIO for push button

// Button and scan handling
bool buttonPressed = false;
unsigned long debounceTime = 0;
const unsigned long debounceDelay = 50;

bool qrPaused = true; // Initial state: QR scanning is paused
unsigned long scanCooldownStart = 0;
const unsigned long scanCooldownDelay = 10000; // 10-second cooldown after scanning

String currentQRData = ""; // Variable to store current QR data

// Function to initialize the camera
void initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = 5;
  config.pin_d1 = 18;
  config.pin_d2 = 19;
  config.pin_d3 = 21;
  config.pin_d4 = 36;
  config.pin_d5 = 39;
  config.pin_d6 = 34;
  config.pin_d7 = 35;
  config.pin_xclk = 0;
  config.pin_pclk = 22;
  config.pin_vsync = 25;
  config.pin_href = 23;
  config.pin_sscb_sda = 26;
  config.pin_sscb_scl = 27;
  config.pin_pwdn = 32;
  config.pin_reset = -1;
  config.xclk_freq_hz = 10000000;
  config.pixel_format = PIXFORMAT_GRAYSCALE;
  config.frame_size = FRAMESIZE_QVGA;
  config.jpeg_quality = 15;
  config.fb_count = 1;

  if (esp_camera_init(&config) != ESP_OK) {
    ESP.restart(); // Restart if initialization fails
  }
}

// Function to read QR code data
String readQRCode() {
  if (q != NULL) {
    quirc_destroy(q);
  }
  q = quirc_new();
  if (!q) {
    return "";
  }

  fb = esp_camera_fb_get();
  if (!fb) {
    return "";
  }

  quirc_resize(q, fb->width, fb->height);
  uint8_t *image = quirc_begin(q, NULL, NULL);
  memcpy(image, fb->buf, fb->len);
  quirc_end(q);

  int count = quirc_count(q);
  String qrResult = "";

  for (int i = 0; i < count; i++) {
    quirc_extract(q, i, &code);
    if (quirc_decode(&code, &qrData) == QUIRC_SUCCESS) {
      qrResult = String((char *)qrData.payload);
    }
  }

  esp_camera_fb_return(fb);
  return qrResult;
}

// Function to handle button press
void handleButtonPress() {
  static bool lastButtonState = HIGH;
  bool currentButtonState = digitalRead(BUTTON_PIN);

  if (currentButtonState != lastButtonState) {
    debounceTime = millis();
  }

  if ((millis() - debounceTime) > debounceDelay) {
    if (currentButtonState == LOW && !buttonPressed) {
      buttonPressed = true;
      if (qrPaused) {
        qrPaused = false;      // Resume scanning
        currentQRData = "";    // Clear the previous QR data
      } else {
        qrPaused = true;       // Pause scanning
      }
    } else if (currentButtonState == HIGH) {
      buttonPressed = false; // Reset for the next press
    }
  }

  lastButtonState = currentButtonState;
}

void setup() {
  Serial.begin(115200);         // Start UART communication
  pinMode(BUTTON_PIN, INPUT_PULLUP); // Set button pin as input with pull-up
  initCamera();                 // Initialize the camera
}

void loop() {
  // Handle button press
  handleButtonPress();

  // Check if cooldown period has ended
  if (!qrPaused && millis() - scanCooldownStart >= scanCooldownDelay) {
    currentQRData = ""; // Clear the previous QR data after cooldown
  }

  // Skip QR reading if paused
  if (qrPaused) {
    return;
  }

  // Read QR code data
  String qrData = readQRCode();

  // Check if QR data is valid and not the same as the previous scan
  if (qrData.length() > 0 && qrData != currentQRData) {
    currentQRData = qrData;       // Update current QR data
    Serial.println(currentQRData); // Print QR data
    qrPaused = true;             // Pause QR reading
    scanCooldownStart = millis(); // Start cooldown timer
  }
}
