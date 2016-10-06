#pragma once

static const long presses[5] = { 40, 150, 300, 800, 1400 };

volatile uint16_t encValuePrev;
volatile bool changed;

enum class duration_type : uint8_t { reallyQuickPress, shortPress, normalPress, longPress, veryLongPress };

// https://wiki.nottinghack.org.uk/wiki/Project:Arduino_Rotary_Encoder_Menu_System
template<int PIN_ENC1, int PIN_ENC2, int PIN_BTN, typename value_t = uint16_t>
class RotaryEncoderDecoder {
  public:
    using on_click_callback_t = void(*)(duration_type, uint16_t click_length);
    using on_move_callback_t = void(*)(bool dir);

  private:
    // volatile boolean moving = false;
    volatile value_t encValue = 0;
    volatile boolean enc1 = false;
    volatile boolean enc2 = false;

    on_move_callback_t on_move_callback;
    on_click_callback_t on_click_callback;
  public:
    RotaryEncoderDecoder(on_move_callback_t on_move_callback, on_click_callback_t on_click_callback) :
      on_move_callback(on_move_callback), on_click_callback(on_click_callback) {
    }

    void setup() {
      pinMode(PIN_ENC1, INPUT_PULLUP);
      pinMode(PIN_ENC2, INPUT_PULLUP);
      pinMode(PIN_BTN, INPUT_PULLUP);
    }

    inline
    value_t currentValue() {
      return encValue;
    }

    void loop() {
      // static unsigned long btnHeld = 0;

      static auto next = millis();
      auto now = millis();
      if (now > next) {
        next += 1000;
        Serial.print("encValuePrev: ");
        Serial.print(encValuePrev, DEC);
        Serial.print(", encValue: ");
        Serial.print(encValue, DEC);
        // Serial.print(", dir=");
        // Serial.print(dir ? ">>>" : "<<<");
        Serial.println();
      }

      // moving = true;
      // if (encValuePrev != encValue) {
      
      if (changed) {
        changed = false;
        bool dir = encValuePrev > encValue;

        on_move_callback(dir);

        encValuePrev = encValue;
      }
      
      /*
      // Upon button press...
      if ((digitalRead(PIN_BTN) == LOW) && !btnHeld) {
        btnHeld = millis();
        // digitalWrite(PIN_LED, HIGH);
        Serial.print("pressed selecting: ");
        Serial.println(encValue, DEC);
      }
      // Upon button release...
      if ((digitalRead(PIN_BTN) == HIGH) && btnHeld) {
        long t = millis();
        t -= btnHeld;
        // digitalWrite(PIN_LED, LOW);
        auto dur = duration_type::veryLongPress;
        for (int i = 0; i <= static_cast<int>(duration_type::veryLongPress); i++) {
          if (t > presses[i])
            continue;
          dur = static_cast<duration_type>(i);
          break;
        }

        on_click_callback(dur, t);
        btnHeld = 0;
      }
      */
    }

    void intr1() {
      // if (moving)
      //  delay(1);

      /*
      Serial.print("2: 1=");
      Serial.print(enc1);
      Serial.print(", 2=");
      Serial.print(enc2);
      Serial.print(", encValue=");
      Serial.print(encValue);
      Serial.print(", encValuePrev=");
      Serial.println(encValuePrev);
      */

      if (digitalRead(PIN_ENC1) == enc1) {
        // Serial.println("11");
        return;
      }
      enc1 = !enc1;
      if (enc1 && !enc2) {
        encValue += 1;
        changed = true;
      }
      // moving = false;
      // Serial.println("12");
    }

    void intr2() {
      // if (moving)
      //  delay(1);

      /*
      Serial.print("2: 1=");
      Serial.print(enc1);
      Serial.print(", 2=");
      Serial.print(enc2);
      Serial.print(", encValue=");
      Serial.print(encValue);
      Serial.print(", encValuePrev=");
      Serial.println(encValuePrev);
      */

      if (digitalRead(PIN_ENC2) == enc2) {
        // Serial.println("21");
        return;
      }
      enc2 = !enc2;
      if (enc2 && !enc1) {
        encValue -= 1;
        changed = true;
      }
      // moving = false;
      // Serial.println("22");
    }
};

