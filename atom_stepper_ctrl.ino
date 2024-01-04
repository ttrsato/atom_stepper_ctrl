// atom_stepper_ctrl.ino
//
// MIT License
// 
// Copyright (c) 2023 Tatsuro Sato
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//



#include "M5Atom.h"
#include "Unit_Encoder.h"
#include <Ticker.h>

const int EN_PIN          = 22;
const int DIR_PIN         = 23;
const int STEP_PIN        = 19;
const int FAULT_PIN       = 25;
const int VIN_PIN         = 33;
const int PULSE_WH        = 500;
const int PULSE_WL        = 500;
int pulse_wh = PULSE_WH;
int pulse_wl = PULSE_WL;

const int MOTOR_INTERVAL_MS = 1;

static int motor_cnt = 0;
static int last_motor_cnt = 0;

enum {
  ST_M_IDLE,
  ST_M_H,
  ST_M_L
};

static int motor_state = ST_M_IDLE;

int delta_step = 1;
bool step_en = false;

int pos = 0;

enum {
  LED_DISABLE = 0x000050,
  LED_ENABLE  = 0x003000,
  LED_FAULT   = 0x300000,
  LED_OFF     = 0x000000,
};

Ticker motor_tick;

void debugPrint(char *s)
{
#ifdef DEBUG_PRINT
  Serial.println(s);
#endif
}

void debugPrint(int d)
{
#ifdef DEBUG_PRINT
  Serial.println(d);
#endif
}

int isFault()
{
  static int last_fault = 0;
  int fault = digitalRead(FAULT_PIN) ? 0 : 1;
  int vin = digitalRead(VIN_PIN) ? 0 : 1;
  int cur_fault = fault | vin;
  if (cur_fault != last_fault) {
    if (cur_fault) {
      M5.dis.drawpix(0, LED_FAULT);
    }
    else {
      if (step_en) M5.dis.drawpix(0, LED_ENABLE);
      else M5.dis.drawpix(0, LED_DISABLE);
    }
  }
  last_fault = cur_fault;
  return cur_fault;
}

void setEnable(bool en)
{
  if (en) {
    digitalWrite(EN_PIN, LOW);
    debugPrint("Enabled");
    M5.dis.drawpix(0, LED_ENABLE);
    ets_delay_us(2);
    step_en = true;
    Serial.println("E");
    motor_tick.attach_ms(MOTOR_INTERVAL_MS, updateMotor);
  } else {
    digitalWrite(EN_PIN, HIGH);
    ets_delay_us(2);
    debugPrint("Disabled");
    M5.dis.drawpix(0, LED_DISABLE);
    step_en = false;
    Serial.println("D");
    motor_tick.detach();
  }
  isFault();
}

void setDirection(int dir)
{
  if (dir > 0)
    digitalWrite(DIR_PIN, LOW);
  else
    digitalWrite(DIR_PIN, HIGH);
}

void updateMotor()
{
  if (motor_cnt != 0) {
    switch (motor_state) {
      case ST_M_IDLE:
        setDirection(motor_cnt);
        digitalWrite(STEP_PIN, HIGH);
        motor_state = ST_M_H;
        break;
      case ST_M_H:
        digitalWrite(STEP_PIN, LOW);
        motor_state = ST_M_L;
        break;
      case ST_M_L:
        motor_state = ST_M_IDLE;
        if (motor_cnt > 0) motor_cnt--;
        else if (motor_cnt < 0) motor_cnt++;
        if (motor_cnt == 0) Serial.println(last_motor_cnt);
        break;
    }
  }
}

void setup()
{
  M5.begin(true, false, true);
  Serial.begin(115200);
  pinMode(EN_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  pinMode(STEP_PIN, OUTPUT);
  pinMode(FAULT_PIN, INPUT);
  pinMode(VIN_PIN, INPUT);
  digitalWrite(EN_PIN, LOW);
  digitalWrite(DIR_PIN, LOW);
  digitalWrite(STEP_PIN, LOW);
  setEnable(true);
  delay(100);
  motor_tick.attach_ms(MOTOR_INTERVAL_MS, updateMotor);
}

void loop()
{
  String aStr;
  if (Serial.available() > 0) {
    aStr = Serial.readStringUntil('\n');
    if (aStr.equals("s")) {
      switch (isFault()) {
        case 1: Serial.println("U");   break; // UVLO (VIN)
        case 2: Serial.println("O");   break; // Overtemp / Overcurrent
        case 3: Serial.println("OU"); break;  // UVLO and OV
        default:
          if (step_en) Serial.println("E");
          else Serial.println("D");
      }
    } else if (aStr.equals("E")) {
      setEnable(true);
    } else if (aStr.equals("D")) {
      setEnable(false);
    } else if (aStr.equals("p")) {
      Serial.print("p");
      Serial.println(pos);
    } else {
      if (motor_cnt == 0) {
        last_motor_cnt = aStr.toInt();
        motor_cnt = last_motor_cnt;
      }
    }
  }
  isFault();
  M5.update();
}
