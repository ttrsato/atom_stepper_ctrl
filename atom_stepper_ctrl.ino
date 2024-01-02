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

//const int ATOM_CMD = 0x01;
Unit_Encoder sensor;
unsigned long last_time = 0;

int last_encoder_value;
int last_btn_status;

int delta_step = 1;
bool step_en = false;

int pos = 0;

enum {
  LED_ENABLE  = 0x300000,
  LED_DISABLE = 0x003000,
  LED_FAULT   = 0x707070,
  LED_OFF     = 0x000000,
};

enum {BLK_STOP, BLK_H, BLK_L};
Ticker blink_tick;
volatile int blink_stat = BLK_STOP;
int blink_color = LED_FAULT;

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
  int fault = digitalRead(FAULT_PIN) ? 0 : 1;
  int vin = digitalRead(VIN_PIN) ? 0 : 1;
  if (fault) {
    blink_color = LED_FAULT;
  } else {
    blink_color = LED_DISABLE;
  }
  return fault || vin;
}

void setEnable(bool en)
{
  if (en) {
    digitalWrite(EN_PIN, LOW);
    M5.dis.drawpix(0, LED_ENABLE);
    debugPrint("Enabled");
    ets_delay_us(2);
    sensor.setLEDColor(2, 0x060600);
    step_en = true;
    Serial.println("E");
    motor_tick.attach_ms(MOTOR_INTERVAL_MS, updateMotor);
  } else {
    digitalWrite(EN_PIN, HIGH);
    ets_delay_us(2);
    M5.dis.drawpix(0, LED_DISABLE);
    debugPrint("Disabled");
    sensor.setLEDColor(2, 0x000000);
    step_en = false;
    Serial.println("D");
    motor_tick.detach();
  }
}

void stepPulse()
{
  digitalWrite(STEP_PIN, HIGH);
  ets_delay_us(pulse_wh);
  digitalWrite(STEP_PIN, LOW);
  ets_delay_us(pulse_wl);
}

void rotateStepper(int move_step)
{
  int mstep = move_step;
  while (move_step != 0) {
    if (move_step > 0) {
      move_step--;
      digitalWrite(DIR_PIN, LOW);
      stepPulse();
      pos += 1;
    } else if (move_step < 0) {
      move_step++;
      digitalWrite(DIR_PIN, HIGH);
      stepPulse();
      pos -= 1;
    }
  }
  Serial.println(mstep);
}

enum {ST_ENABLE, ST_DISABLE, ST_FAULT};

int state = ST_FAULT;

void blink_toggle()
{
  if (blink_stat == BLK_H) {
    M5.dis.drawpix(0, LED_OFF);  
    blink_stat = BLK_L;
  } else {
    M5.dis.drawpix(0, blink_color);  
    blink_stat = BLK_H;
  }
}

void updateStatus()
{
  int is_fault = isFault();
  int btn_was_pressed = M5.Btn.wasPressed();
  if (is_fault) {
    if (blink_stat == BLK_STOP) {
      debugPrint("FAULT");
      state = ST_FAULT;
      blink_stat = BLK_H;
      M5.dis.drawpix(0, blink_color);
      blink_tick.attach(0.5, blink_toggle);
      return;
    }
  } else {
    switch (state) {
      case ST_ENABLE:
        if (btn_was_pressed) {
          state = ST_DISABLE;
          setEnable(false);
        }
        break;
      case ST_DISABLE:
        if (btn_was_pressed) {
          state = ST_ENABLE;
          setEnable(true);
        }
        break;
      case ST_FAULT:
        debugPrint("Exit fault");
        state = ST_ENABLE;
        blink_stat = BLK_STOP;
        blink_tick.detach();
        setEnable(true);
        break;
      default:
        state = ST_ENABLE;
        setEnable(true);
        break;
    }
  }
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
  sensor.begin();
  // SerialBT.begin("ATOM-STEP");
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
  last_encoder_value = sensor.getEncoderValue();
  last_btn_status    = sensor.getButtonStatus();
  sensor.setLEDColor(1, 0x00000f);
  motor_tick.attach_ms(MOTOR_INTERVAL_MS, updateMotor);
}

void updateDeltaStep()
{
  switch (delta_step) {
    case  1: delta_step = 5;  sensor.setLEDColor(1, 0x000900); break;
    case  5: delta_step = 10; sensor.setLEDColor(1, 0x090000); break;
    case 10: delta_step = 1;  sensor.setLEDColor(1, 0x00000f); break;
  }
  Serial.print("s");
  Serial.println(delta_step);
}

void loop()
{

  String aStr;
  if (Serial.available() > 0) {
    aStr = Serial.readStringUntil('\n');
    if (aStr.equals("s")) {
      updateDeltaStep();
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
  M5.update();
}
