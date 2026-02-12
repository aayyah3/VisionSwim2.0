#include <Pixy2.h>
#include <ZumoMotors.h>
#include <limits.h>

// Max X coordinate for Pixy2 line tracking
#define PIXY_MAX_X 78 

// Hardware Pins
#define TURN_LEFT_PIN A2
#define TURN_RIGHT_PIN A3
#define GROUND_PIN_0 A0
#define GROUND_PIN_1 A1

// Pool Lane Color Signature (Tune these!)
const uint8_t POOL_R = 27;
const uint8_t POOL_G = 53;
const uint8_t POOL_B = 60;

// Optimization Constants
const int MIN_LINE_LENGTH_SQ = 0; // Vector must be ~20 pixels long (20^2)
const int COLOR_THRESHOLD = 4000;   // Distance threshold for "Blue" check
const int REQUIRED_FRAMES = 3;      // Reduced from 5 for faster response

Pixy2 pixy;
int threshold = 20; 

enum Direction { DIR_NONE, DIR_LEFT, DIR_RIGHT, DIR_STRAIGHT };

// Tracking States
Direction candidateDir = DIR_NONE;
Direction confirmedDir = DIR_NONE;
int consecutiveCount = 0;

void signalDirection(Direction dir) {
  // Logic remains the same: High on A2 for Left, A3 for Right
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
  pixy.setCameraBrightness(50);        // Try lower values

}

void loop() {
  pixy.line.getAllFeatures();
  Direction currentDir = DIR_NONE;
  int best_idx = -1;


  // STEP 1: Find a line that is "Long and Blue"
    long minDist = LONG_MAX;

    for (int i = 0; i < pixy.line.numVectors; i++) {
      // Check Length First (Flowchart: Is vector long?)
      int dx = pixy.line.vectors[i].m_x1 - pixy.line.vectors[i].m_x0;
      int dy = pixy.line.vectors[i].m_y1 - pixy.line.vectors[i].m_y0;
      long lenSq = (long)dx*dx + (long)dy*dy;

      if (lenSq >= MIN_LINE_LENGTH_SQ) {
        // Check Color (Flowchart: Is vector blue?)
        uint8_t r_mid, g_mid, b_mid, r_st, g_st, b_st, r_end, g_end, b_end;
        int sx = (pixy.line.vectors[i].m_x0 + pixy.line.vectors[i].m_x1) / 2;
        int sy = (pixy.line.vectors[i].m_y0 + pixy.line.vectors[i].m_y1) / 2;
        
        pixy.video.getRGB(sx, sy, &r_mid, &g_mid, &b_mid);
        pixy.video.getRGB(pixy.line.vectors[i].m_x0, pixy.line.vectors[i].m_y0, &r_st, &g_st, &b_st);
        pixy.video.getRGB(pixy.line.vectors[i].m_x1, pixy.line.vectors[i].m_y1, &r_end, &g_end, &b_end);

        int r_avg = (r_mid + r_st + r_end)/3;
        int g_avg = (r_mid + r_st + r_end)/3;
        int b_avg = (r_mid + r_st + r_end)/3;
        
        Serial.println(
              String("Vector ") + i +
              " RGB value: " + r_avg + "," + g_avg + "," + b_avg + "|| m index: " + pixy.line.vectors[i].m_index
            );        
        long dist = pow(r_avg-POOL_R, 2) + pow(g_avg-POOL_G, 2) + pow(b_avg-POOL_B, 2);

        if (dist < minDist) {
          minDist = dist;
          best_idx = i;
        }
      }
    }

  // STEP 2: Decide Direction (Flowchart: Check Position)
  if (best_idx != -1) {
    Serial.println(
              String("SELECTED VECTOR: ") + best_idx
            );
    int lineCenterX = (pixy.line.vectors[best_idx].m_x0 + pixy.line.vectors[best_idx].m_x1) / 2;
    if (lineCenterX < threshold) currentDir = DIR_LEFT;
    else if (lineCenterX > PIXY_MAX_X - threshold) currentDir = DIR_RIGHT;
    else currentDir = DIR_STRAIGHT;
  }

  // STEP 3: Frame Confirmation
  if (currentDir == candidateDir) consecutiveCount++;
  else { candidateDir = currentDir; consecutiveCount = 1; }

  if (consecutiveCount >= REQUIRED_FRAMES && confirmedDir != candidateDir) {
    confirmedDir = candidateDir;
    signalDirection(confirmedDir);
  }

  delay(500); // Increased frequency for better tracking responsiveness
}
