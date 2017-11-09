#pragma once

class SerialReader {
    String buf;
  public:
    SerialReader() {
      buf.reserve(100);
    }

    bool read(String &line) {
      while (Serial.available()) {
        char chr = (char)Serial.read();
        if (chr == '\n' || chr == '\r') {
          if (buf.length() > 0) {
            line = buf;
            buf.remove(0);
            return true;
          }
        } else {
          buf += chr;
        }
      }

      return false;
    }
};

template<int interval_ms>
class fixed_interval_timer {
    long next;

  public:
    fixed_interval_timer() : next(millis() + interval_ms) {}

    ~fixed_interval_timer() {}

    void reset() {
      next = millis() + interval_ms;
    }

    bool expired() {
      auto now = millis();
      bool e = now > next;

      if (!e) {
        return false;
      }

      do {
        next += interval_ms;
      } while (now > next);

      return true;
    }
};

