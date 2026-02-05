#include <Pixy2.h>
#include <ZumoMotors.h>
#include <limits.h>

#define PIXY_MAX_X 78 
#define TURN_LEFT_PIN A2
#define TURN_RIGHT_PIN A3
#define GROUND_PIN_0 A0
#define GROUND_PIN_1 A1

const uint8_t POOL_R = 28;
const uint8_t POOL_G = 71;
const uint8_t POOL_B = 97;

const int MIN_LINE_LENGTH_SQ = 400;
const int COLOR_THRESHOLD = 4000;
const int REQUIRED_FRAMES = 3;

Pixy2 pixy;
int threshold = 20; 

enum Direction { DIR_NONE, DIR_LEFT, DIR_RIGHT, DIR_STRAIGHT };

Direction candidateDir = DIR_NONE;
Direction confirmedDir = DIR_NONE;
int consecutiveCount = 0;
int lockedIndex = -1;

// NEW: Non-blocking timing
unsigned long lastUpdate = 0;
const int UPDATE_INTERVAL = 20; // 50 Hz instead of 20 Hz

void signalDirection(Direction dir) {
  analogWrite(TURN_LEFT_PIN, (dir == DIR_LEFT) ? 255 : 0);
  analogWrite(TURN_RIGHT_PIN, (dir == DIR_RIGHT) ? 255 : 0);
  
  if(dir == DIR_LEFT) Serial.println("LEFT");
  else if(dir == DIR_RIGHT) Serial.println("RIGHT");
  else if(dir == DIR_STRAIGHT) Serial.println("STRAIGHT");
  else Serial.println("NO LINE");
}

void setup() {
  Serial.begin(115200);
  pixy.init();
  pixy.changeProg("line");
  pinMode(TURN_LEFT_PIN, OUTPUT);
  pinMode(TURN_RIGHT_PIN, OUTPUT);
  analogWrite(GROUND_PIN_0, 0);
  analogWrite(GROUND_PIN_1, 0);
}

void loop() {
  // CHANGE 1: Non-blocking timing (removes delay)
  if (millis() - lastUpdate < UPDATE_INTERVAL) return;
  lastUpdate = millis();
  
  pixy.line.getAllFeatures();
  Direction currentDir = DIR_NONE;
  int best_idx = -1;

  // STEP 1: Check if locked line still exists
  if (lockedIndex != -1) {
    for (int i = 0; i < pixy.line.numVectors; i++) {
      if (pixy.line.vectors[i].m_index == lockedIndex) {
        best_idx = i;
        break;
      }
    }
  }

  // STEP 2: Find new line if locked is lost
  if (best_idx == -1) {
    lockedIndex = -1;
    long minDist = LONG_MAX;
    
    for (int i = 0; i < pixy.line.numVectors; i++) {
      // Length check (early exit)
      int dx = pixy.line.vectors[i].m_x1 - pixy.line.vectors[i].m_x0;
      int dy = pixy.line.vectors[i].m_y1 - pixy.line.vectors[i].m_y0;
      long lenSq = (long)dx*dx + (long)dy*dy;
      
      if (lenSq < MIN_LINE_LENGTH_SQ) continue;
      
      // CHANGE 2: Check BOTH endpoints for color (more robust)
      uint8_t r0, g0, b0, r1, g1, b1;
      pixy.video.getRGB(pixy.line.vectors[i].m_x0, pixy.line.vectors[i].m_y0, &r0, &g0, &b0);
      pixy.video.getRGB(pixy.line.vectors[i].m_x1, pixy.line.vectors[i].m_y1, &r1, &g1, &b1);
      
      long dist0 = (long)pow(r0-POOL_R, 2) + pow(g0-POOL_G, 2) + pow(b0-POOL_B, 2);
      long dist1 = (long)pow(r1-POOL_R, 2) + pow(g1-POOL_G, 2) + pow(b1-POOL_B, 2);
      
      // CHANGE 3: BOTH endpoints must be blue
      if (dist0 < COLOR_THRESHOLD && dist1 < COLOR_THRESHOLD) {
        long avgDist = (dist0 + dist1) / 2;
        if (avgDist < minDist) {
          minDist = avgDist;
          best_idx = i;
        }
      }
    }
    
    if (best_idx != -1) lockedIndex = pixy.line.vectors[best_idx].m_index;
  }

  // STEP 3: Decide Direction
  if (best_idx != -1) {
    int lineCenterX = (pixy.line.vectors[best_idx].m_x0 + pixy.line.vectors[best_idx].m_x1) / 2;
    
    if (lineCenterX < threshold) currentDir = DIR_LEFT;
    else if (lineCenterX > PIXY_MAX_X - threshold) currentDir = DIR_RIGHT;
    else currentDir = DIR_STRAIGHT;
  }

  // STEP 4: Frame Confirmation
  if (currentDir == candidateDir) {
    consecutiveCount++;
  } else {
    candidateDir = currentDir;
    consecutiveCount = 1;
  }

  if (consecutiveCount >= REQUIRED_FRAMES && confirmedDir != candidateDir) {
    confirmedDir = candidateDir;
    signalDirection(confirmedDir);
  }
  
  // CHANGE 1: delay() removed - handled by non-blocking timing above
}
