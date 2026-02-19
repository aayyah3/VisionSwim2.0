#include <Pixy2.h>
#include <math.h>   // Added for fmod
#include <limits.h>
#include <algorithm> // NEW: For std::min and std::max

#define PIXY_MAX_X 78 

// Hardware Pins
#define TURN_LEFT_PIN 5   // Changed from A2 (Nano PWM fix)
#define TURN_RIGHT_PIN 6  // Changed from A3 (Nano PWM fix)
#define GROUND_PIN_0 A0
#define GROUND_PIN_1 A1

// --- UNDERWATER TUNING ---
// Wide hue range to catch blue/cyan/teal despite water tint
const float TARGET_HUE_MIN = 160.0; 
const float TARGET_HUE_MAX = 270.0;
// Low saturation (0.1 = 10%) because underwater colors look "washed out"
const float MIN_SATURATION = 0.1; 
// Low value (15.0) because the bottom of a pool is often dim
const float MIN_VALUE = 15.0;     

const long MIN_LINE_LENGTH_SQ = 100; // Filter out tiny noise (10px min)
const int REQUIRED_FRAMES = 2;      // Faster response for moving water

Pixy2 pixy;
const int threshold = 20; 

enum Direction { DIR_NONE, DIR_LEFT, DIR_RIGHT, DIR_STRAIGHT };
Direction candidateDir = DIR_NONE;
Direction confirmedDir = DIR_NONE;
int consecutiveCount = 0;

struct HSV { float h, s, v; };  // Changed double -> float (Nano safe)

// NEW: RGB to HSV Conversion function
HSV rgbToHsv(float r, float g, float b) {
    HSV out;

    r /= 255.0f; 
    g /= 255.0f; 
    b /= 255.0f; 

    float minVal = min(r, min(g, b));
    float maxVal = max(r, max(g, b));
    float delta = maxVal - minVal;

    out.v = maxVal * 100.0f; 

    if (maxVal == 0.0f) { 
        out.s = 0.0f; 
        out.h = 0.0f; 
        return out; 
    }

    out.s = (delta / maxVal); 

    if (delta == 0.0f) {
        out.h = 0.0f;
    }
    else if (maxVal == r) {
        out.h = 60.0f * fmod(((g - b) / delta), 6.0f);
    }
    else if (maxVal == g) {
        out.h = 60.0f * (((b - r) / delta) + 2.0f);
    }
    else if (maxVal == b) {
        out.h = 60.0f * (((r - g) / delta) + 4.0f);
    }

    if (out.h < 0.0f) out.h += 360.0f;

    return out;
}

void signalDirection(Direction dir) {
  if (dir == DIR_LEFT) {
    analogWrite(TURN_LEFT_PIN, 255);
    analogWrite(TURN_RIGHT_PIN, 0);
  }
  else if (dir == DIR_RIGHT) {
    analogWrite(TURN_LEFT_PIN, 0);
    analogWrite(TURN_RIGHT_PIN, 255);
  }
  else {
    analogWrite(TURN_LEFT_PIN, 0);
    analogWrite(TURN_RIGHT_PIN, 0);
  }
}

void setup() {
  Serial.begin(115200);
  pixy.init();
  pixy.changeProg("line");
  pinMode(TURN_LEFT_PIN, OUTPUT);
  pinMode(TURN_RIGHT_PIN, OUTPUT);
  pinMode(GROUND_PIN_0, OUTPUT);
  pinMode(GROUND_PIN_1, OUTPUT);
  digitalWrite(GROUND_PIN_0, LOW);
  digitalWrite(GROUND_PIN_1, LOW);
  
  // NEW: Optimized for underwater visibility
  pixy.setCameraBrightness(60); 
}

void loop() {

  if (!pixy.line.getMainFeatures()) {
    delay(10);
    return;
  }

  Direction currentDir = DIR_NONE;
  int best_idx = -1;

  for (int i = 0; i < pixy.line.numVectors; i++) {

    int dx = pixy.line.vectors[i].m_x1 - pixy.line.vectors[i].m_x0;
    int dy = pixy.line.vectors[i].m_y1 - pixy.line.vectors[i].m_y0;
    long lenSq = (long)dx*dx + (long)dy*dy;

    if (lenSq >= MIN_LINE_LENGTH_SQ) {

      uint8_t r, g, b;
      int sx = (pixy.line.vectors[i].m_x0 + pixy.line.vectors[i].m_x1) / 2;
      int sy = (pixy.line.vectors[i].m_y0 + pixy.line.vectors[i].m_y1) / 2;
      
      // NEW: Use getRGB with 'false' to get raw, un-saturated values
      pixy.video.getRGB(sx, sy, &r, &g, &b); 

      HSV hsv = rgbToHsv(r, g, b);

      // CHANGED: Range-based filtering instead of RGB distance
      if (hsv.h >= TARGET_HUE_MIN && 
          hsv.h <= TARGET_HUE_MAX && 
          hsv.s >= MIN_SATURATION && 
          hsv.v >= MIN_VALUE) {

          best_idx = i;
          break; 
      }
    }
  }

  if (best_idx != -1) {

    int lineCenterX = 
      (pixy.line.vectors[best_idx].m_x0 + 
       pixy.line.vectors[best_idx].m_x1) / 2;

    if (lineCenterX < threshold) {
      currentDir = DIR_LEFT;
    }
    else if (lineCenterX > PIXY_MAX_X - threshold) {
      currentDir = DIR_RIGHT;
    }
    else {
      currentDir = DIR_STRAIGHT;
    }
  }

  if (currentDir == candidateDir) {
    consecutiveCount++;
  }
  else {
    candidateDir = currentDir;
    consecutiveCount = 1;
  }

  if (consecutiveCount >= REQUIRED_FRAMES && 
      confirmedDir != candidateDir) {

    confirmedDir = candidateDir;
    signalDirection(confirmedDir);
  }

  delay(10); 
}
