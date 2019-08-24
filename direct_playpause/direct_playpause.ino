#include "TrinketHidCombo.h"

#define RAW_PIN  PINB
#define PIN_ENCODER_A 2
#define PIN_ENCODER_B 0
#define PIN_ENCODER_SWITCH 1
#define debounce 5 // ms debounce period to prevent flickering when pressing or releasing the button
#define hold_time 1000 // ms hold period: how long to wait for press+hold event

static uint8_t enc_prev_pos   = 0;
static uint8_t enc_flags      = 0;
static char    sw_already_pressed = 0;

long sw_down_time; // time the switch was pressed down
boolean ignore_up = false; // whether to ignore the switch release because click+hold was triggered

void setup() {
  // set pins as input with internal pull-up resistors enabled
  pinMode(PIN_ENCODER_A, INPUT);
  pinMode(PIN_ENCODER_B, INPUT);
  digitalWrite(PIN_ENCODER_A, HIGH);
  digitalWrite(PIN_ENCODER_B, HIGH);

  pinMode(PIN_ENCODER_SWITCH, INPUT);
  // the switch is active-high, not active-low
  // since it shares the pin with Spark's built-in LED
  // the LED acts as a pull-down resistor
  digitalWrite(PIN_ENCODER_SWITCH, LOW);

  TrinketHidCombo.begin(); // start the USB device engine and enumerate

  // get an initial reading on the encoder pins
  if (digitalRead(PIN_ENCODER_A) == LOW) {
    enc_prev_pos |= (1 << 0);
  }
  if (digitalRead(PIN_ENCODER_B) == LOW) {
    enc_prev_pos |= (1 << 1);
  }
}

void loop() {
  int8_t enc_action = 0; // 1 or -1 if moved, sign is direction

  // note: for better performance, the code will now use
  // direct port access techniques
  // http://www.arduino.cc/en/Reference/PortManipulation
  uint8_t enc_cur_pos = 0;
  // read in the encoder state first
  if (bit_is_clear(RAW_PIN, PIN_ENCODER_A)) {
    enc_cur_pos |= (1 << 0);
  }
  if (bit_is_clear(RAW_PIN, PIN_ENCODER_B)) {
    enc_cur_pos |= (1 << 1);
  }

  // if any rotation at all
  if (enc_cur_pos != enc_prev_pos) {
    if (enc_prev_pos == 0x00) {
      // this is the first edge
      if (enc_cur_pos == 0x01) {
        enc_flags |= (1 << 0);
      }
      else if (enc_cur_pos == 0x02) {
        enc_flags |= (1 << 1);
      }
    }

    if (enc_cur_pos == 0x03) {
      // this is when the encoder is in the middle of a "step"
      enc_flags |= (1 << 4);
    }
    else if (enc_cur_pos == 0x00) {
      // this is the final edge
      if (enc_prev_pos == 0x02) {
        enc_flags |= (1 << 2);
      }
      else if (enc_prev_pos == 0x01) {
        enc_flags |= (1 << 3);
      }

      // check the first and last edge
      // or maybe one edge is missing, if missing then require the middle state
      // this will reject bounces and false movements
      if (bit_is_set(enc_flags, 0) && (bit_is_set(enc_flags, 2) || bit_is_set(enc_flags, 4))) {
        enc_action = 1;
      }
      else if (bit_is_set(enc_flags, 2) && (bit_is_set(enc_flags, 0) || bit_is_set(enc_flags, 4))) {
        enc_action = 1;
      }
      else if (bit_is_set(enc_flags, 1) && (bit_is_set(enc_flags, 3) || bit_is_set(enc_flags, 4))) {
        enc_action = -1;
      }
      else if (bit_is_set(enc_flags, 3) && (bit_is_set(enc_flags, 1) || bit_is_set(enc_flags, 4))) {
        enc_action = -1;
      }

      enc_flags = 0; // reset for next time
    }
  }

  enc_prev_pos = enc_cur_pos;

  if (enc_action > 0) {
    TrinketHidCombo.pressMultimediaKey(MMKEY_VOL_UP);
  }
  else if (enc_action < 0) {
    TrinketHidCombo.pressMultimediaKey(MMKEY_VOL_DOWN);
  }

  // remember that the switch is active-high
  if (bit_is_set(RAW_PIN, PIN_ENCODER_SWITCH)) {
    // only on initial press, so the keystroke is not repeated while the button is held down
    if (sw_already_pressed == 0) {
      sw_down_time = millis();
      delay(debounce); // debounce delay
    }
    
    // check if we've held the button down for more than $hold_time and press play/pause instead of mute
    if ((millis() - sw_down_time) > long(hold_time) && ignore_up == false) {
      ignore_up = true;
      TrinketHidCombo.pressMultimediaKey(MMKEY_PLAYPAUSE);
    }
    
    sw_already_pressed = 1;
  }
  // else, when the switch isn't pressed
  else {
    if (sw_already_pressed == 1) {
      // if the switch was pressed and has now been released, plus we didn't hold it down, do:
      if (ignore_up == false) {
        TrinketHidCombo.pressMultimediaKey(MMKEY_MUTE);
      }
      else {
        ignore_up = false;
      }
      delay(debounce); // debounce delay
    }
    
    sw_already_pressed = 0;
  }
  
  TrinketHidCombo.poll(); // check if USB needs anything done
}
