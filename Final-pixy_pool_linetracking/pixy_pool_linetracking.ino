#include <SoftwareSerial.h>

SoftwareSerial Serial1(8, 9);  // RX = 8, TX = 9 (you can change pins)


#include <PIDLoop.h>
#include <Pixy2.h>
#include <Pixy2CCC.h>
#include <Pixy2I2C.h>
#include <Pixy2Line.h>
#include <Pixy2SPI_SS.h>
#include <Pixy2UART.h>
#include <Pixy2Video.h>
#include <TPixy2.h>
#include <ZumoBuzzer.h>
#include <ZumoMotors.h>

//
// begin license header
//
// This file is part of Pixy CMUcam5 or "Pixy" for short
//
// All Pixy source code is provided under the terms of the
// GNU General Public License v2 (http://www.gnu.org/licenses/gpl-2.0.html).
// Those wishing to use Pixy source code, software and/or
// technologies under different licensing terms should contact us at
// cmucam@cs.cmu.edu. Such licensing terms are available for
// all portions of the Pixy codebase presented here.
//
// end license header
//

//max x coordinate
#define PIXY_MAX_X 78 

//Arduino pins that will be used to control motors or actuators for turning.
#define TURN_LEFT_PIN A2
#define TURN_RIGHT_PIN A3
#define GROUND_PIN_0 A0
#define GROUND_PIN_1 A1

Pixy2 pixy;
int threshold = 20; // A “dead zone” from the left or right side of the camera’s view.

enum Direction {
  DIR_NONE,
  DIR_LEFT,
  DIR_RIGHT,
  DIR_STRAIGHT
};

Direction candidateDir = DIR_NONE;
Direction confirmedDir = DIR_NONE;
int consecutiveCount = 0;

const int REQUIRED_FRAMES = 5;

void signalDirection(Direction dir)
{
  switch (dir) {
    case DIR_LEFT:
      Serial.println("LEFT");
      analogWrite(TURN_LEFT_PIN, 255);
      analogWrite(TURN_RIGHT_PIN, 0);
      break;

    case DIR_RIGHT:
      Serial.println("RIGHT");
      analogWrite(TURN_LEFT_PIN, 0);
      analogWrite(TURN_RIGHT_PIN, 255);
      break;

    case DIR_STRAIGHT:
      Serial.println("STRAIGHT");
      analogWrite(TURN_LEFT_PIN, 0);
      analogWrite(TURN_RIGHT_PIN, 0);
      break;

    case DIR_NONE:
      Serial.println("NO LINE");
      analogWrite(TURN_LEFT_PIN, 0);
      analogWrite(TURN_RIGHT_PIN, 0);
      break;
  }
}

void setup()
{
  Serial.begin(115200); //serial communication so the Arduino can print debug info to the computer
  Serial.print("Starting...\n");

  pixy.init(); //Initializes the Pixy camera so it’s ready to detect lines.
  // change to the line_tracking program.  Note, changeProg can use partial strings, so for example,
  // you can change to the line_tracking program by calling changeProg("line") instead of the whole
  // string changeProg("line_tracking")
  Serial.println(pixy.changeProg("line")); //Tells the Pixy camera to switch to line tracking mode.
  analogWrite(GROUND_PIN_1, 0);
  analogWrite(GROUND_PIN_0, 0);
}

void loop()
{
  pixy.line.getAllFeatures();

  Direction currentDir = DIR_NONE;

  // No line detected
  if (pixy.line.numVectors == 0) {
    currentDir = DIR_NONE;
  } 
  else {
    // Find bluest line
    uint8_t r, g, b;
    uint8_t maxBlue = 0;
    int blue_idx = 0;

    // Check first vector
    pixy.video.getRGB(
      pixy.line.vectors[0].m_x1,
      pixy.line.vectors[0].m_y1,
      &r, &g, &maxBlue
    );

    // Check remaining vectors
    for (int i = 1; i < pixy.line.numVectors; i++) {
      pixy.video.getRGB(
        pixy.line.vectors[i].m_x1,
        pixy.line.vectors[i].m_y1,
        &r, &g, &b
      );
      if (b > maxBlue) {
        maxBlue = b;
        blue_idx = i;
      }
    }

    // Use center of line
    int x0 = pixy.line.vectors[blue_idx].m_x0;
    int x1 = pixy.line.vectors[blue_idx].m_x1;
    int lineCenterX = (x0 + x1) / 2;

    // Decide direction for this change
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

  // Consecutive frame confirmation
  if (currentDir == candidateDir) {
    consecutiveCount++;
  } else {
    candidateDir = currentDir;
    consecutiveCount = 1;
  }

  if (consecutiveCount >= REQUIRED_FRAMES &&
      confirmedDir != candidateDir) {

    confirmedDir = candidateDir;
    signalDirection(confirmedDir);
  }

  delay(100);  // ~10 Hz update rate
}



