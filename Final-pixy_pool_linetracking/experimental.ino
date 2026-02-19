#include <Pixy2.h>
#include <math.h>

#define PIXY_MAX_X 319   // Pixy2 line tracking width

// PWM output pins (Nano compatible)
#define TURN_LEFT_PIN 5
#define TURN_RIGHT_PIN 6
#define GROUND_PIN_0 A0
#define GROUND_PIN_1 A1

// ---- UNDERWATER HSV FILTER SETTINGS ----
// Wide enough to tolerate water tint, but not crazy wide
const float TARGET_HUE_MIN = 170.0; 
const float TARGET_HUE_MAX = 250.0;

// Underwater colors are washed out
const float MIN_SATURATION = 0.12; 

// Pool bottoms can be dim
const float MIN_VALUE = 18.0;     

// Ignore tiny noise vectors
const long MIN_LINE_LENGTH_SQ = 150; 

// Require stability across frames
const int REQUIRED_FRAMES = 2;      

Pixy2 pixy;
const int threshold = 20;  // Deadband around center

// Direction states
enum Direction { DIR_NONE, DIR_LEFT, DIR_RIGHT, DIR_STRAIGHT };

Direction candidateDir = DIR_NONE;
Direction confirmedDir = DIR_NONE;
int consecutiveCount = 0;

// Simple HSV struct
struct HSV { float h, s, v; };


// Convert RGB (0–255) to HSV
HSV rgbToHsv(float r, float g, float b) {

    HSV out;

    // Normalize to 0–1
    r /= 255.0f; 
    g /= 255.0f; 
    b /= 255.0f; 

    float minVal = min(r, min(g, b));
    float maxVal = max(r, max(g, b));
    float delta = maxVal - minVal;

    // Brightness scaled 0–100
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
    else {
        out.h = 60.0f * (((r - g) / delta) + 4.0f);
    }

    if (out.h < 0.0f) out.h += 360.0f;

    return out;
}


// Output direction via PWM
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

  pixy.setCameraBrightness(60);
}


void loop() {
  // Get main line features from Pixy2
  if (!pixy.line.getMainFeatures()) {
    Serial.println("No vectors detected at all!");
    delay(100); // avoid spamming
    return;
  }

  int best_idx = -1;
  long bestLen = 0;

  // Iterate over all detected vectors
  for (int i = 0; i < pixy.line.numVectors; i++) {

    int x0 = pixy.line.vectors[i].m_x0;
    int y0 = pixy.line.vectors[i].m_y0;
    int x1 = pixy.line.vectors[i].m_x1;
    int y1 = pixy.line.vectors[i].m_y1;

    int dx = x1 - x0;
    int dy = y1 - y0;
    long lenSq = (long)dx * dx + (long)dy * dy;

    // Ignore tiny vectors
    if (lenSq < MIN_LINE_LENGTH_SQ) continue;

    // Sample only the midpoint to reduce CPU load
    int sx = (x0 + x1) / 2;
    int sy = (y0 + y1) / 2;

    uint8_t r_mid, g_mid, b_mid;
    pixy.video.getRGB(sx, sy, &r_mid, &g_mid, &b_mid);

    float r_avg = (float)r_mid;
    float g_avg = (float)g_mid;
    float b_avg = (float)b_mid;

    HSV hsv = rgbToHsv(r_avg, g_avg, b_avg);

    // Debug: see what HSV Pixy2 is reading
    Serial.print("Vector "); Serial.print(i);
    Serial.print(" H: "); Serial.print(hsv.h);
    Serial.print(" S: "); Serial.print(hsv.s);
    Serial.print(" V: "); Serial.println(hsv.v);

    // Check if vector falls inside your underwater blue range
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

  // No valid vector found
  if (best_idx == -1) {
    Serial.println("No valid line detected (after HSV filter)");
    delay(50);
    return;
  }

  // Determine horizontal center of the best vector
  int lineCenterX = (pixy.line.vectors[best_idx].m_x0 +
                     pixy.line.vectors[best_idx].m_x1) / 2;

  // Decide direction based on horizontal position
  Direction currentDir = DIR_NONE;
  if (lineCenterX < threshold)
    currentDir = DIR_LEFT;
  else if (lineCenterX > PIXY_MAX_X - threshold)
    currentDir = DIR_RIGHT;
  else
    currentDir = DIR_STRAIGHT;

  // Temporal smoothing
  if (currentDir == candidateDir)
    consecutiveCount++;
  else {
    candidateDir = currentDir;
    consecutiveCount = 1;
  }

  // Only commit to motor if stable across REQUIRED_FRAMES
  if (consecutiveCount >= REQUIRED_FRAMES &&
      confirmedDir != candidateDir) {

    confirmedDir = candidateDir;
    signalDirection(confirmedDir);
    Serial.print("Motor command: ");
    if (confirmedDir == DIR_LEFT) Serial.println("LEFT");
    else if (confirmedDir == DIR_RIGHT) Serial.println("RIGHT");
    else Serial.println("STRAIGHT");
  }

  delay(10); // small delay to stabilize loop
}
