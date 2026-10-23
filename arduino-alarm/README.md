# Arduino Musical Alarm

An Arduino Uno plays a melody once each morning using a DS3231 real-time clock. A button stops playback immediately without disabling the next day's alarm.

## Parts and wiring

- Arduino Uno R3 / ATmega328P
- DS3231 RTC module with compatible backup battery
- Passive piezo buzzer suitable for direct GPIO drive
- Normally open momentary button
- 10 kΩ resistor, breadboard and jumper wires

| Component connection | Uno connection |
| --- | --- |
| DS3231 VCC | 5V (use a 5V-compatible breakout) |
| DS3231 GND | GND |
| DS3231 SDA | A4 |
| DS3231 SCL | A5 |
| Passive piezo buzzer + | D10 |
| Passive piezo buzzer − | GND |
| Button, first terminal | 3.3V |
| Button, opposite terminal | A0 |
| 10 kΩ pull-down resistor | Between A0 and GND |

The button input is LOW at rest and HIGH when pressed. Use terminals on opposite sides of the switch contacts. All grounds must be connected. The RTC SQW pin is unused: the sketch polls its alarm flag through I²C.

Use a transistor driver for a buzzer that needs more current than a GPIO can provide. An active buzzer will not reproduce the melody's different pitches.

## Upload

1. Install **RTClib by Adafruit** through Arduino IDE Library Manager, including its dependencies when prompted.
2. Open `arduino_alarm/arduino_alarm.ino`. Keep the sketch in its matching `arduino_alarm` directory.
3. Select **Arduino Uno** and the correct serial port.
4. Edit the settings at the top of the sketch, then upload.
5. Open Serial Monitor at **9600 baud** to see the current RTC time and configured alarm time. Opening the monitor may reset the Uno and print startup information again.

## Set the alarm and tempo

```cpp
constexpr uint8_t ALARM_HOUR = 7;
constexpr uint8_t ALARM_MINUTE = 0;
constexpr uint8_t ALARM_SECOND = 0;
constexpr uint16_t QUARTER_NOTE_MS = 200;
```

The alarm uses a 24-hour clock. For 7:30 PM, use `19`, `30`, `0`. Upload again after changing the constants. A smaller quarter-note duration plays the melody faster.

For a quick test, use an alarm time a minute or two ahead of the startup RTC time. Return it to the desired morning time afterward.

## Set the clock

When the RTC reports lost power, the sketch initializes it from the sketch's compilation timestamp. This approximates your computer's local date and time; upload time introduces a small delay. Confirm the printed time.

To correct an RTC whose time is wrong:

1. Set `SET_RTC_FROM_COMPILE_TIME = true` and upload.
2. Set it back to `false` and upload again so future resets do not overwrite the clock.

The RTC does not automatically apply time zones or daylight saving changes. Adjust it when local time changes.

## Playback and stop button

- The melody plays once per daily alarm and then stops automatically.
- Pressing the button interrupts playback, including during rests.
- Holding the button when the alarm starts skips that playback.
- Repeated pitches have an eighth-note silent gap taken from the preceding note's duration, preserving the overall rhythm.
- A0 uses the ATmega328P pin-change interrupt (`PCINT1_vect`), not `attachInterrupt()`. The ISR only sets a flag; the playback function handles stopping. Switch bounce is harmless because requesting a stop multiple times has the same effect.
- The melody is stored as pitch/duration pairs. Durations count eighth notes; named values make the score easier to edit.

Keep the Uno powered for playback. The RTC backup battery preserves time but does not power the Uno or buzzer. Startup clears pending alarm flags, so alarms missed while the Uno was off are not replayed. Pins and interrupt registers target the Uno R3; other Arduino boards may require changes.

## Validation

Hardware testing is still required. Check startup time, trigger a near-future alarm, press the stop button during both notes and rests, and confirm the configured daily alarm remains enabled. The project has not been compiled with the Arduino AVR toolchain or tested on physical hardware in this environment.

## References

- [Adafruit RTClib](https://github.com/adafruit/RTClib)
- [DS3231 alarm example](https://github.com/adafruit/RTClib/blob/master/examples/DS3231_alarm/DS3231_alarm.ino)
