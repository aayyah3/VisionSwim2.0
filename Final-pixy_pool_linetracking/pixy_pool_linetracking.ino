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
#define TURN_LEFT_PIN A0 
#define TURN_RIGHT_PIN A2

Pixy2 pixy;
int threshold = 20; // A “dead zone” from the left or right side of the camera’s view.
int Counter = 0;
int Direction = -1; // straight = 0, left = 1, right = 2
int consec_frames = 15;

void setup()
{
  Serial.begin(115200); //serial communication so the Arduino can print debug info to the computer
  Serial.print("Starting...\n");

  pixy.init(); //Initializes the Pixy camera so it’s ready to detect lines.
  // change to the line_tracking program.  Note, changeProg can use partial strings, so for example,
  // you can change to the line_tracking program by calling changeProg("line") instead of the whole
  // string changeProg("line_tracking")
  Serial.println(pixy.changeProg("line")); //Tells the Pixy camera to switch to line tracking mode.
}

void loop()
{
//  pixy.changeProg("line");
  pixy.line.getMainFeatures(); //Requests all the line vectors Pixy sees
  int x_mid = (pixy.line.vectors->m_x1 + pixy.line.vectors->m_x0)/2
  if (pixy.line.numVectors){ 
    Serial.println(pixy.line.numVectors);
    
    if (x_mid < threshold) {
      //turn left
      if(Direction == 1)
        Counter++;
      else
      {
        Direction = 1;
        Counter = 0;
      }
      Serial.println("Turn Left"); 
    } else if (x_mid > PIXY_MAX_X - threshold) {
      //turn right
      if(Direction == 2)
        Counter++;
      else
      {
        Direction = 2;
        Counter = 0;
      }
      Serial.println("Turn Right");
    } else {
      //go straight
      Direction = 0;
      Counter = 0;
      analogWrite(TURN_LEFT_PIN, 0);
      analogWrite(TURN_RIGHT_PIN, 0);
      Serial.println("Go Straight");
    }
    
  }
  //nothing detected
  else
  {
    Direction = 0;
    Counter = 0;
    analogWrite(TURN_LEFT_PIN, 0);
    analogWrite(TURN_RIGHT_PIN, 0);
    Serial.println("No vector detected");
  }
    
  if(Counter >= consec_frames)
  {
    Counter = consec_frames; // so Counter doesn't overflow
    if(Direction == 2)
    {
      analogWrite(TURN_LEFT_PIN, 255);
      analogWrite(TURN_RIGHT_PIN, 0);
    }
    else if(Direction == 1)
    {
      analogWrite(TURN_LEFT_PIN, 0);
      analogWrite(TURN_RIGHT_PIN, 255);
    }
  }
  
}
