#include <Wire.h>
#include <RTClib.h>
#include <avr/interrupt.h>

// ---------- Settings ----------
constexpr uint8_t ALARM_HOUR = 7;       // 24-hour clock: 0–23
constexpr uint8_t ALARM_MINUTE = 0;     // 0–59
constexpr uint8_t ALARM_SECOND = 0;     // 0–59
constexpr uint16_t QUARTER_NOTE_MS = 200;
constexpr uint8_t BUZZER_PIN = 10;
constexpr uint8_t STOP_BUTTON_PIN = A0; // HIGH when pressed; external pull-down
// Set true, upload once, then set false and upload again.
constexpr bool SET_RTC_FROM_COMPILE_TIME = false;

static_assert(ALARM_HOUR < 24, "Alarm hour must be 0–23");
static_assert(ALARM_MINUTE < 60, "Alarm minute must be 0–59");
static_assert(ALARM_SECOND < 60, "Alarm second must be 0–59");
static_assert(QUARTER_NOTE_MS >= 2, "Quarter note must be at least 2 ms");
static_assert(STOP_BUTTON_PIN == A0, "This interrupt handler requires A0");

RTC_DS3231 rtc;
volatile bool stopRequested = false;
constexpr uint16_t EIGHTH_NOTE_MS = QUARTER_NOTE_MS / 2;

// Pitches in Hz. Flats follow the sheet's A-flat major key signature.
enum Pitch : uint16_t {
  REST = 0,
  C4 = 262, DB4 = 277, EB4 = 311, F4 = 349, G4 = 392,
  AB4 = 415, BB4 = 466, C5 = 523, DB5 = 554, EB5 = 622
};

enum Rhythm : uint8_t {
  EIGHTH = 1, QUARTER = 2, DOTTED_QUARTER = 3, HALF = 4
};

struct Note {
  uint16_t frequency;
  uint8_t eighthNotes;
};

const Note melody[] = {
  // Measures 1–7
  {C4, DOTTED_QUARTER}, {DB4, EIGHTH},
  {EB4, QUARTER}, {EB4, QUARTER},
  {EB4, QUARTER}, {AB4, QUARTER},
  {AB4, QUARTER}, {C5, QUARTER},
  {C5, DOTTED_QUARTER}, {BB4, EIGHTH},
  {AB4, QUARTER}, {REST, QUARTER}, {REST, HALF},
  // Measures 8–13
  {G4, QUARTER}, {AB4, QUARTER},
  {BB4, QUARTER}, {REST, QUARTER}, {REST, HALF},
  {C5, DOTTED_QUARTER}, {BB4, EIGHTH},
  {AB4, QUARTER}, {REST, QUARTER}, {REST, HALF},
  // Measures 14–20
  {C4, DOTTED_QUARTER}, {DB4, EIGHTH},
  {EB4, QUARTER}, {EB4, QUARTER},
  {EB4, QUARTER}, {AB4, QUARTER},
  {AB4, QUARTER}, {C5, QUARTER},
  {C5, DOTTED_QUARTER}, {BB4, EIGHTH},
  {AB4, QUARTER}, {REST, QUARTER}, {REST, HALF},
  // Measures 21–26
  {G4, QUARTER}, {AB4, QUARTER},
  {BB4, QUARTER}, {REST, QUARTER}, {REST, HALF},
  {C5, DOTTED_QUARTER}, {BB4, EIGHTH},
  {AB4, QUARTER}, {REST, QUARTER}, {REST, HALF},
  // Measures 27–32
  {EB4, DOTTED_QUARTER}, {AB4, EIGHTH},
  {G4, QUARTER}, {G4, QUARTER},
  {G4, QUARTER}, {G4, QUARTER},
  {F4, QUARTER}, {G4, QUARTER},
  {AB4, QUARTER}, {REST, QUARTER}, {REST, HALF},
  // Measures 33–38
  {G4, DOTTED_QUARTER}, {AB4, EIGHTH},
  {BB4, QUARTER}, {BB4, QUARTER},
  {BB4, QUARTER}, {BB4, QUARTER},
  {EB5, DOTTED_QUARTER}, {DB5, EIGHTH},
  {C5, QUARTER}, {REST, QUARTER}, {REST, HALF},
  // Measures 39–45
  {C4, DOTTED_QUARTER}, {DB4, EIGHTH},
  {EB4, QUARTER}, {EB4, QUARTER},
  {EB4, QUARTER}, {AB4, QUARTER},
  {AB4, QUARTER}, {C5, QUARTER},
  {C5, DOTTED_QUARTER}, {BB4, EIGHTH},
  {AB4, QUARTER}, {REST, QUARTER}, {REST, HALF},
  // Measures 46–51
  {G4, QUARTER}, {AB4, QUARTER},
  {BB4, QUARTER}, {REST, QUARTER}, {REST, HALF},
  {C5, DOTTED_QUARTER}, {BB4, EIGHTH},
  {AB4, QUARTER}, {REST, QUARTER}, {REST, HALF}
};

constexpr size_t NOTE_COUNT = sizeof(melody) / sizeof(melody[0]);

// A0 belongs to pin-change interrupt group 1 on the ATmega328P.
// Keep the ISR short: playback handles the stop request outside the ISR.
ISR(PCINT1_vect) {
  if (PINC & _BV(PC0)) {
    stopRequested = true;
  }
}

void haltWithError(const __FlashStringHelper *message) {
  Serial.println(message);
  noTone(BUZZER_PIN);
  while (true) {
    delay(100);
  }
}

void configureStopButton() {
  pinMode(STOP_BUTTON_PIN, INPUT);
  PCMSK1 |= _BV(PCINT8);
  PCIFR = _BV(PCIF1); // Clear pending pin-change interrupt.
  PCICR |= _BV(PCIE1);
}

void configureClock() {
  if (!rtc.begin()) {
    haltWithError(F("DS3231 not detected. Check wiring."));
  }

  if (SET_RTC_FROM_COMPILE_TIME || rtc.lostPower()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    Serial.println(F("RTC set to sketch compilation time."));
  }

  char timestamp[] = "YYYY-MM-DD hh:mm:ss";
  DateTime now = rtc.now();
  Serial.print(F("Current RTC time: "));
  Serial.println(now.toString(timestamp));
}

void configureDailyAlarm() {
  rtc.disable32K();
  rtc.writeSqwPinMode(DS3231_OFF);
  rtc.disableAlarm(1);
  rtc.disableAlarm(2);
  rtc.clearAlarm(1);
  rtc.clearAlarm(2);

  // Match hour, minute and second every day; the date is ignored.
  DateTime alarmTime(2000, 1, 1, ALARM_HOUR, ALARM_MINUTE, ALARM_SECOND);
  if (!rtc.setAlarm1(alarmTime, DS3231_A1_Hour)) {
    haltWithError(F("Could not configure daily alarm."));
  }

  char timestamp[] = "hh:mm:ss";
  Serial.print(F("Daily alarm: "));
  Serial.println(alarmTime.toString(timestamp));
}

bool waitUntilElapsedOrStopped(unsigned long durationMs) {
  const unsigned long startedAt = millis();
  while (millis() - startedAt < durationMs) {
    if (stopRequested) {
      noTone(BUZZER_PIN);
      return false;
    }
    delay(1);
  }
  return true;
}

void playMelody() {
  stopRequested = false;
  if (digitalRead(STOP_BUTTON_PIN) == HIGH) {
    Serial.println(F("Alarm skipped: stop button held."));
    return;
  }

  Serial.println(F("Playing alarm."));
  for (size_t i = 0; i < NOTE_COUNT && !stopRequested; ++i) {
    const Note &note = melody[i];
    const unsigned long durationMs =
        static_cast<unsigned long>(note.eighthNotes) * EIGHTH_NOTE_MS;

    if (note.frequency == REST) {
      noTone(BUZZER_PIN);
      if (!waitUntilElapsedOrStopped(durationMs)) break;
      continue;
    }

    const bool repeatsNext =
        i + 1 < NOTE_COUNT && note.frequency == melody[i + 1].frequency;
    const uint16_t gapMs = repeatsNext ? EIGHTH_NOTE_MS : 0;

    tone(BUZZER_PIN, note.frequency);
    if (!waitUntilElapsedOrStopped(durationMs - gapMs)) break;
    noTone(BUZZER_PIN);
    if (gapMs && !waitUntilElapsedOrStopped(gapMs)) break;
  }

  noTone(BUZZER_PIN);
  Serial.println(stopRequested ? F("Alarm stopped by button.")
                               : F("Alarm finished."));
}

void setup() {
  Serial.begin(9600);
  pinMode(BUZZER_PIN, OUTPUT);
  configureStopButton();
  configureClock();
  configureDailyAlarm();
}

void loop() {
  if (rtc.alarmFired(1)) {
    rtc.clearAlarm(1);
    playMelody(); // Play once; stopping does not disable tomorrow's alarm.
  }
  delay(50);
}
