#include "esp_camera.h"
#include <WiFi.h>

// Select camera model
#define CAMERA_MODEL_AI_THINKER // Has PSRAM
// Espressif Internal Board
#define CAMERA_MODEL_ESP32_CAM_BOARD
#include "camera_pins.h"

// Access Point credentials
const char* ssid = "ESP32-CAM Access Point";
const char* password = "changeme";

//void startCameraServer();
void setupLedFlash(int pin);

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

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
  config.frame_size = FRAMESIZE_UXGA;
  config.pixel_format = PIXFORMAT_JPEG; // for streaming
  //config.pixel_format = PIXFORMAT_RGB565; // for face detection/recognition
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  // if PSRAM IC present, init with UXGA resolution and higher JPEG quality
  //                      for larger pre-allocated frame buffer.
  if (config.pixel_format == PIXFORMAT_JPEG) {
    if (psramFound()) {
      config.jpeg_quality = 10;
      config.fb_count = 2;
      config.grab_mode = CAMERA_GRAB_LATEST;
    } else {
      // Limit the frame size when PSRAM is not available
      config.frame_size = FRAMESIZE_SVGA;
      config.fb_location = CAMERA_FB_IN_DRAM;
    }
  } else {
    // Best option for face detection/recognition
    config.frame_size = FRAMESIZE_240X240;
#if CONFIG_IDF_TARGET_ESP32S3
    config.fb_count = 2;
#endif
  }

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t * s = esp_camera_sensor_get();
  // initial sensors are flipped vertically and colors are a bit saturated
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1); // flip it back
    s->set_brightness(s, 1); // up the brightness just a bit
    s->set_saturation(s, -2); // lower the saturation
  }
  // drop down frame size for higher initial frame rate
  if (config.pixel_format == PIXFORMAT_JPEG) {
    s->set_framesize(s, FRAMESIZE_QVGA);
  }

#if defined(CAMERA_MODEL_ESP32S3_EYE)
  s->set_vflip(s, 1);
#endif

  // Setup LED FLash if LED pin is defined in camera_pins.h
#if defined(LED_GPIO_NUM)
  setupLedFlash(LED_GPIO_NUM);
#endif

  //WiFi.softAP(ssid, password);
  //startCameraServer();

  //Serial.print("Camera Ready! Use 'http://");
  //Serial.print(WiFi.localIP());
  //Serial.println("' to connect");
}

void loop() {
  readRGBImage();
  delay(5);
}

/*
 * readRGBImage() code taken from: https://esp32.com/viewtopic.php?t=10405&start=20
  */
void readRGBImage() {
  /*
  uint32_t resultsToShow = 50;     // how much data to display

  String tReply = "LIVE IMAGE AS RGB DATA: ";      // reply to send to web client and serial port
  */
  // capture live image (jpg)
  camera_fb_t * fb = NULL;
  fb = esp_camera_fb_get();
  if (!fb) Serial.println("Error capturing image from camera");
  //if (!fb) tReply += " -Error capturing image from camera- ";
  //tReply += "(Image resolution=" + String(fb->width) + "x" + String(fb->height) + ")";       // display image resolution: 320 x 240 by default

  // allocate memory to store rgb data in psram
  //if (!psramFound()) tReply += " -Error no psram found- ";
  if (!psramFound()) Serial.println("Error no psram found");
  void *ptrVal = NULL;
  uint32_t ARRAY_LENGTH = fb->width * fb->height * 3;               // number of pixels in the jpg image x 3
  if (heap_caps_get_free_size( MALLOC_CAP_SPIRAM) <  ARRAY_LENGTH) Serial.println("Error not enough free psram to store rgb data");    // check free memory in psram
  //if (heap_caps_get_free_size( MALLOC_CAP_SPIRAM) <  ARRAY_LENGTH) tReply += " -Error not enough free psram to store rgb data- ";    // check free memory in psram
  ptrVal = heap_caps_malloc(ARRAY_LENGTH, MALLOC_CAP_SPIRAM);
  uint8_t *rgb = (uint8_t *)ptrVal;

  // convert jpg to rgb (store in an array 'rgb')
  bool jpeg_converted = fmt2rgb888(fb->buf, fb->len, PIXFORMAT_JPEG, rgb);
  if (!jpeg_converted) Serial.println("Error converting image to RGB");
  //if (!jpeg_converted) tReply += " -Error converting image to RGB- ";
  /*
  // display some of the resulting data
  for (uint32_t i = 0; i < resultsToShow; i++) {
    // // x and y coordinate of the pixel
    //   uint16_t x = i % fb->width;
    //   uint16_t y = floor(i / fb->width);
    if (i % 3 == 0) tReply += "B";
    else if (i % 3 == 1) tReply += "G";
    else if (i % 3 == 2) tReply += "R";
    tReply += String(rgb[i]) + ",";
  }
  */
  /* 
   * 2024/01/02: Add "3 by 2 grid" processing method 
   */
  const uint32_t TILE_ROW_NUM = 2;
  const uint32_t TILE_COL_NUM = 3;
  //uint32_t vignette = 20; // cut 20 pixels for both height and width to account for camera's vignette
  uint32_t p_width = fb->width;//  - vignette; 
  uint32_t p_height = fb->height;// - vignette;
  // for now, grid will be split evenly (i.e. into half for rows, into thirds for columns)
  float tile_lum[TILE_ROW_NUM][TILE_COL_NUM]; // stores the average luminance per tile
  uint32_t tile_size = (p_height / TILE_ROW_NUM) * (p_width / TILE_COL_NUM);
  for (uint32_t tile_row = 0; tile_row < TILE_ROW_NUM; ++tile_row) { // iterate through rows of tiles
    uint32_t start_row = tile_row * (p_height / TILE_ROW_NUM);// + (vignette / 2);
    uint32_t end_row = (tile_row + 1) * (p_height / TILE_ROW_NUM);// + (vignette / 2);
    for (uint32_t tile_col = 0; tile_col < TILE_COL_NUM; ++tile_col) { // iterate through columns of tiles
      uint32_t start_col = tile_col * (p_width / TILE_COL_NUM);// + (vignette / 2);
      uint32_t end_col = (tile_col + 1) * (p_width / TILE_COL_NUM);// + (vignette / 2);
      float total_lum = 0; // stores the total luminance of all the pixels within tile
      for (uint32_t y = start_row; y < end_row; ++y) { // iterate through rows within tile
        for (uint32_t x = start_col; x < end_col; ++x) { // iterate through columns of tiles
          // calculate luminance i.e. lightness in pixel; formula here: https://en.wikipedia.org/wiki/HSL_and_HSV
          uint32_t i = (y * p_width * 3) + (x * 3);
          float max_px = max(rgb[i], max(rgb[i+1], rgb[i+2])) / 255.0; // max of RGB values
          float min_px = min(rgb[i], min(rgb[i+1], rgb[i+2])) / 255.0; // min of RGB values
          total_lum += (max_px + min_px) / 2;
        }
      }
      tile_lum[tile_row][tile_col] = total_lum / (float)tile_size; // set average luminance in tiles
    }
  }

  // print out un-normalized tile values
  Serial.println("Before normalization:");
  for (int i = 0; i < TILE_ROW_NUM; ++i) {
    for (int j = 0; j < TILE_COL_NUM; ++j) {
      Serial.print(tile_lum[i][j]);
      Serial.print(" ");
    }
    Serial.println("");
  }

  // Retrieve min and max lum from tiles
  float min_lum = tile_lum[0][0];
  float max_lum = tile_lum[0][0];
  for (uint32_t i = 0; i < TILE_ROW_NUM; ++i) {
    for (uint32_t j = 0; j < TILE_COL_NUM; ++j) {
      if (tile_lum[i][j] < min_lum) min_lum = tile_lum[i][j];
      if (tile_lum[i][j] > max_lum) max_lum = tile_lum[i][j];
    }
  }

  // Normalize tile lum values for easier comparison
  for (uint32_t i = 0; i < TILE_ROW_NUM; ++i) {
    for (uint32_t j = 0; j < TILE_COL_NUM; ++j) {
      tile_lum[i][j] = (tile_lum[i][j] - min_lum) / (max_lum - min_lum);
    }
  }

  Serial.println("After normalization:");
  for (int i = 0; i < TILE_ROW_NUM; ++i) {
    for (int j = 0; j < TILE_COL_NUM; ++j) {
      Serial.print(tile_lum[i][j]);
      Serial.print(" ");
    }
    Serial.println("");
  }

  // Check for end of line condition: Top row lum is all >= 0.5 i.e. more light than dark
  bool top_white = true;
  for (uint32_t j = 0; j < TILE_COL_NUM; ++j) {
    if (tile_lum[0][j] < 0.5) {
      top_white = false;
      break;
    }
  }
  if (top_white) {
    Serial.println("Reached end");
  } else {
    // Check for direction: lum of bottom row
    // TODO: Check for lum of column instead
    float left_lum = (tile_lum[0][0] + tile_lum[TILE_ROW_NUM - 1][0]) / 2;
    float mid_lum = (tile_lum[0][1] + tile_lum[TILE_ROW_NUM - 1][1]) / 2;
    float right_lum = (tile_lum[0][TILE_COL_NUM - 1] + tile_lum[TILE_ROW_NUM - 1][TILE_COL_NUM - 1]) / 2;
    float darkest = min(left_lum, min(mid_lum, right_lum));
    if (darkest == left_lum) Serial.println("Turn left");
    else if (darkest == right_lum) Serial.println("Turn right");
    else Serial.println("Go straight");
    /*
    bool left_white = tile_lum[TILE_ROW_NUM - 1][0] >= 0.5;
    bool right_white = tile_lum[TILE_ROW_NUM - 1][TILE_COL_NUM - 1] >= 0.5;
    if (left_white == right_white || abs(tile_lum[TILE_ROW_NUM - 1][0] - tile_lum[TILE_ROW_NUM - 1][TILE_COL_NUM - 1]) < 0.1) Serial.println("Go straight");
    else if (left_white) Serial.println("Turn right");
    else if (right_white) Serial.println("Turn left");
    */
  }

  /*
   * End "3 by 2 grid" processing method 
   */

  // free up memory
  esp_camera_fb_return(fb);
  heap_caps_free(ptrVal);

  Serial.println("-------");
  //Serial.println(tReply);
}
