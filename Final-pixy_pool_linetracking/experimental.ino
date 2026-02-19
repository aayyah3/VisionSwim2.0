#include <Pixy2.h>
#include <math.h>
#include <limits.h>

#define PIXY_MAX_X 78   // Pixy2 line tracking max X resolution

// PWM output pins (Nano compatible)
#define TURN_LEFT_PIN 5
#define TURN_RIGHT_PIN 6
#define GROUND_PIN_0 A0
#define GROUND_PIN_1 A1

// ---- UNDERWATER HSV FILTER SETTINGS ----
// Hue range widened slightly to tolerate blue/cyan shift in water
const float TARGET_HUE_MIN = 170.0; 
const float TARGET_HUE_MAX = 250.0;

// Underwater colors look washed out, so allow lower saturation
const float MIN_SATURATION = 0.12; 

// Pool bottoms can be dim — allow fairly low brightness
const float MIN_VALUE = 18.0;     

// Ignore tiny noise vectors (roughly 12px minimum length)
const long MIN_LINE_LENGTH_SQ = 150; 

// Require direction to be stable for N frames before committing
const int REQUIRED_FRAMES = 2;      

Pixy2 pixy;
const int threshold = 20;  // Deadband for center detection

// Direction states
enum Direction { DIR_NONE, DIR_LEFT, DIR_RIGHT, DIR_STRAIGHT };

Direction candidateDir = DIR_NONE;
Direction confirmedDir = DIR_NONE;
int consecutiveCount = 0;

// Simple HSV container
struct HSV { float h, s, v; };


// Convert RGB (0–255) to HSV
// We use float because Nano handles float fine, but avoid double
HSV rgbToHsv(float r, float g, float b) {

    HSV out;

    // Normalize to 0–1 range
    r /= 255.0f; 
    g /= 255.0f; 
    b /= 255.0f; 

    float minVal = min(r, min(g, b));
    float maxVal = max(r, max(g, b));
    float delta = maxVal - minVal;

    // Brightness scaled to 0–100 for easier threshold tuning
    out.v = maxVal * 100.0f; 

    // If no brightness, no color information
    if (maxVal == 0.0f) { 
        out.s = 0.0f; 
        out.h = 0.0f; 
        return out; 
    }

    // Saturation calculation
    out.s = (delta / maxVal); 

    // Hue calculation depends on which channel is dominant
    if (delta == 0.0f) {
        out.h = 0.0f;
    }
    else if (maxVal == r) {
        out.h = 60.0f * fmod(((g - b) / delta), 6.0f);
    }
    else if (maxVal == g) {
        out.h = 60.0f * (((b - r) / delta) + 2.0f);
    }
    else {
        out.h = 60.0f * (((r - g) / delta) + 4.0f);
    }

    if (out.h < 0.0f) out.h += 360.0f;

    return out;
}


// Send PWM signal indicating direction
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

  // Slightly increased brightness for underwater clarity
  pixy.setCameraBrightness(60);
}


void loop() {

  // If no line features detected this frame, skip quickly
  if (!pixy.line.getMainFeatures()) {
    delay(10);
    return;
  }

  Direction currentDir = DIR_NONE;
  int best_idx = -1;
  long bestLen = 0;  // We'll choose the longest valid blue vector

  // Iterate over all detected vectors
  for (int i = 0; i < pixy.line.numVectors; i++) {

    int dx = pixy.line.vectors[i].m_x1 - pixy.line.vectors[i].m_x0;
    int dy = pixy.line.vectors[i].m_y1 - pixy.line.vectors[i].m_y0;

    long lenSq = (long)dx*dx + (long)dy*dy;

    // Ignore tiny vectors (likely noise/reflections)
    if (lenSq >= MIN_LINE_LENGTH_SQ) {

      // Sample 3 points along the vector for stability
      uint8_t r1,g1,b1;
      uint8_t r2,g2,b2;
      uint8_t r3,g3,b3;

      int x0 = pixy.line.vectors[i].m_x0;
      int y0 = pixy.line.vectors[i].m_y0;
      int x1 = pixy.line.vectors[i].m_x1;
      int y1 = pixy.line.vectors[i].m_y1;

      int sx = (x0 + x1) / 2;
      int sy = (y0 + y1) / 2;

      pixy.video.getRGB(x0, y0, &r1, &g1, &b1);
      pixy.video.getRGB(sx, sy, &r2, &g2, &b2);
      pixy.video.getRGB(x1, y1, &r3, &g3, &b3);

      // Average the 3 samples to reduce flicker
      float r_avg = (r1 + r2 + r3) / 3.0f;
      float g_avg = (g1 + g2 + g3) / 3.0f;
      float b_avg = (b1 + b2 + b3) / 3.0f;

      HSV hsv = rgbToHsv(r_avg, g_avg, b_avg);

      // Optional debug — use when tuning thresholds
      // Serial.print("H: "); Serial.print(hsv.h);
      // Serial.print(" S: "); Serial.print(hsv.s);
      // Serial.print(" V: "); Serial.println(hsv.v);

      // Check if this vector falls inside our underwater blue range
      if (hsv.h >= TARGET_HUE_MIN &&
          hsv.h <= TARGET_HUE_MAX &&
          hsv.s >= MIN_SATURATION &&
          hsv.v >= MIN_VALUE) {

          // Keep the longest valid vector
          if (lenSq > bestLen) {
              bestLen = lenSq;
              best_idx = i;
          }
      }
    }
  }
  if (best_idx == -1) {
    Serial.println("No valid line detected");
  }
  // If we found a valid blue line
  if (best_idx != -1) {

    int lineCenterX =
      (pixy.line.vectors[best_idx].m_x0 +
       pixy.line.vectors[best_idx].m_x1) / 2;

    // Decide direction based on horizontal position
    if (lineCenterX < threshold)
      currentDir = DIR_LEFT;
    else if (lineCenterX > PIXY_MAX_X - threshold)
      currentDir = DIR_RIGHT;
    else
      currentDir = DIR_STRAIGHT;
  }

  // Simple temporal smoothing
  if (currentDir == candidateDir)
    consecutiveCount++;
  else {
    candidateDir = currentDir;
    consecutiveCount = 1;
  }

  // Only commit if stable across required frames
  if (consecutiveCount >= REQUIRED_FRAMES &&
      confirmedDir != candidateDir) {

    confirmedDir = candidateDir;
    signalDirection(confirmedDir);
  }

  delay(10);  // Small delay to prevent overloading serial + stabilize loop
}
