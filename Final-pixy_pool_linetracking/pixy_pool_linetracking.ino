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

#include <Pixy2.h>
//max x coordinate
#define PIXY_MAX_X 78 

//Arduino pins that will be used to control motors or actuators for turning.
#define TURN_LEFT_PIN A2
#define TURN_RIGHT_PIN A3
#define GROUND_PIN_0 A0
#define GROUND_PIN_1 A1

Pixy2 pixy;
int threshold = 20; // A “dead zone” from the left or right side of the camera’s view.

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
  pixy.line.getAllFeatures(); //Requests all the line vectors Pixy sees
//  
//  Serial.print("Number on lines detected: ");
//  Serial.println(pixy.line.numVectors);
  if (pixy.line.numVectors == 0)
    return;
  uint8_t r, g, b = 0;
  int blue_idx = 0;
  uint8_t maxBlue = 0;
  pixy.video.getRGB(pixy.line.vectors->m_x1, pixy.line.vectors->m_y1, &r, &g, &maxBlue);
  
  for(int i = 1; i < pixy.line.numVectors; i++)
  {
    pixy.video.getRGB(pixy.line.vectors[i].m_x1, pixy.line.vectors[i].m_y1, &r, &g, &b);
    if(b > maxBlue)
    {
      blue_idx = i;
      maxBlue = b;
    }
  }

  if (pixy.line.vectors[blue_idx].m_x1 < threshold) {
      //turn left
      analogWrite(TURN_LEFT_PIN, 255);
      analogWrite(TURN_RIGHT_PIN, 0);
      Serial.println("Turn Left"); 
    } else if (pixy.line.vectors[blue_idx].m_x1 > PIXY_MAX_X - threshold) {
      //turn right
      analogWrite(TURN_LEFT_PIN, 0);
      analogWrite(TURN_RIGHT_PIN, 255);
      Serial.println("Turn Right");
    } else {
      //go straight
      analogWrite(TURN_LEFT_PIN, 0);
      analogWrite(TURN_RIGHT_PIN, 0);
      Serial.println("Go Straight");
    }
//  if (pixy.line.numVectors){ 
//    
//    Serial.println(pixy.line.numVectors);
//    pixy.video.getRGB(pixy.line.vectors->m_x1, pixy.line.vectors->m_y1, &r, &g, &b);
//    Serial.println("Blue value: ");
//    Serial.println(b);
//    if (pixy.line.vectors->m_x1 < threshold) {
//      //turn left
//      analogWrite(TURN_LEFT_PIN, 255);
//      analogWrite(TURN_RIGHT_PIN, 0);
//      Serial.println("Turn Left"); 
//    } else if (pixy.line.vectors->m_x1 > PIXY_MAX_X - threshold) {
//      //turn right
//      analogWrite(TURN_LEFT_PIN, 0);
//      analogWrite(TURN_RIGHT_PIN, 255);
//      Serial.println("Turn Right");
//    } else {
//      //go straight
//      analogWrite(TURN_LEFT_PIN, 0);
//      analogWrite(TURN_RIGHT_PIN, 0);
//      Serial.println("Go Straight");
//    }
//    //nothing detected 
//  } 
//  else {
//    analogWrite(TURN_LEFT_PIN, 0);
//    analogWrite(TURN_RIGHT_PIN, 0);
//    Serial.println("no vector detected");
//  }

  delay(100);
}
