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
#include "BluetoothSerial.h"
#include <Ticker.h>

BluetoothSerial SerialBT;

// #define DEBUG_PRINT

const int EN_PIN          = 22;
const int DIR_PIN         = 23;
const int STEP_PIN        = 19;
const int FAULT_PIN       = 25;
const int VIN_PIN         = 33;
const int PULSE_WH        = 500;
const int PULSE_WL        = 500;
int pulse_wh = PULSE_WH;
int pulse_wl = PULSE_WL;

//const int ATOM_CMD = 0x01;
Unit_Encoder sensor;
unsigned long last_time = 0;

int last_encoder_value;
int last_btn_status;

int delta_step = 1;
bool step_en = false;

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
  } else {
    digitalWrite(EN_PIN, HIGH);
    ets_delay_us(2);
    M5.dis.drawpix(0, LED_DISABLE);
    debugPrint("Disabled");
    sensor.setLEDColor(2, 0x000000);
    step_en = false;
    Serial.println("D");
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
    } else if (move_step < 0) {
      move_step++;
      digitalWrite(DIR_PIN, HIGH);
      stepPulse();
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

void setup()
{
  M5.begin(true, false, true);
  Serial.begin(115200);
  sensor.begin();
  SerialBT.begin("ATOM-STEP");
  pinMode(EN_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  pinMode(STEP_PIN, OUTPUT);
  pinMode(FAULT_PIN, INPUT);
  pinMode(VIN_PIN, INPUT);
  setEnable(true);
  delay(100);
  last_encoder_value = sensor.getEncoderValue();
  last_btn_status    = sensor.getButtonStatus();
  sensor.setLEDColor(1, 0x00000f);
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
  signed short int encoder_value = sensor.getEncoderValue();
  bool btn_status                = sensor.getButtonStatus();

  if (btn_status != last_btn_status) {
    if (btn_status == 1) {
      unsigned long cur_time = millis();
      if (cur_time - last_time > 2000) {
        setEnable(!step_en);
      } else if (step_en) {
        updateDeltaStep();
      }
    } else {
      last_time = millis();
    }
  }

  if (encoder_value != last_encoder_value) {
    if (encoder_value > last_encoder_value) {
      rotateStepper(+delta_step);
    } else {
      rotateStepper(-delta_step);      
    }
  }

  last_encoder_value = encoder_value;
  last_btn_status    = btn_status;

  String aStr;
  if (SerialBT.available() > 0) {
    aStr = SerialBT.readStringUntil('\n');
    if (aStr.equals("s")) {
      updateDeltaStep();
    } else if (aStr.equals("E")) {
      setEnable(true);
    } else if (aStr.equals("D")) {
      setEnable(false);
    } else {
      int cnt = aStr.toInt();
      if (cnt != 0) {
        rotateStepper(cnt);      
      }
    }
  }
  
  if (Serial.available() > 0) {
    aStr = Serial.readStringUntil('\n');
    if (aStr.equals("s")) {
      updateDeltaStep();
    } else if (aStr.equals("E")) {
      setEnable(true);
    } else if (aStr.equals("D")) {
      setEnable(false);
    } else {
      int cnt = aStr.toInt();
      if (cnt != 0) {
        rotateStepper(cnt);      
      }
    }
  }
  
  M5.update();
}
