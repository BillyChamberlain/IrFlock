// sound_alarm.ino: bench.ino plus a sound that loops while someone is there.
//
// Something closer than ON_CM starts the clip in sound.h, looping. The closer
// it gets, the louder it plays. When nothing has been in range for OFF_MS the
// sound stops, and the next detection starts it from the top.
//
// Same serial output as bench.ino ("cm 42.5" lines at 115200, plus "vol 0-255"),
// so sense-serial.js works unchanged.
//
// sound.h comes from an MP3: python3 mp3_to_sound.py yourfile.mp3
//
// Wiring, Arduino Nano:
//   HC-SR04  VCC -> 5V    GND -> GND    TRIG -> D9    ECHO -> D10
//   Speaker  D3 -> 220 ohm resistor -> speaker +    speaker - -> GND
// (Louder with an NPN transistor; see speaker-wiring.svg.)
//
// How it plays without an MP3 chip: Timer 2 runs a 62.5 kHz PWM on D3, too
// fast to hear, and Timer 1 interrupts SOUND_RATE times a second to set its
// duty to the next sample. The speaker averages the pulses into the waveform.

#include "sound.h"

const int TRIG = 9;
const int ECHO = 10;
const int SPEAKER = 3;           // Timer 2's PWM pin (OC2B)

const float ON_CM = 150.0;       // closer than this counts as someone there
const float LOUD_CM = 15.0;      // full volume at this distance or closer
const uint8_t QUIET = 40;        // the volume at the edge of range, 0-255
const unsigned long OFF_MS = 1000; // nothing in range this long stops the sound

volatile uint16_t pos = 0;       // the next sample to play
volatile uint8_t vol = 0;        // 0-255, set by loop(), used by the interrupt
bool playing = false;
float level = 0;                 // smoothed volume, so it swells instead of jumping
unsigned long lastSeen = 0;

// Runs SOUND_RATE times a second: the next sample, scaled by the volume.
ISR(TIMER1_COMPA_vect) {
  uint8_t s = pgm_read_byte(&SOUND[pos]);
  OCR2B = ((uint16_t)s * vol) >> 8;
  if (++pos >= SOUND_LEN) pos = 0;
}

void startSound() {
  pos = 0;
  TCNT1 = 0;
  TIMSK1 |= _BV(OCIE1A);         // start the sample interrupt
  TCCR2A |= _BV(COM2B1);         // connect the PWM to D3
  playing = true;
}

void stopSound() {
  TIMSK1 &= ~_BV(OCIE1A);
  TCCR2A &= ~_BV(COM2B1);        // D3 back to a plain pin, held low, so no
  digitalWrite(SPEAKER, LOW);    // current flows through the speaker at rest
  playing = false;
  level = 0;
  vol = 0;
}

float readCm() {
  digitalWrite(TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG, LOW);
  // pulseInLong times with micros(), so the sound's interrupt doesn't throw
  // the reading off the way it would with pulseIn.
  unsigned long us = pulseInLong(ECHO, HIGH, 30000);
  return us ? us / 58.0 : 400.0;
}

void setup() {
  Serial.begin(115200);          // sense-serial.js opens the port at this speed
  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);
  pinMode(SPEAKER, OUTPUT);
  digitalWrite(SPEAKER, LOW);

  // Timer 2: fast PWM, no prescaler, 16 MHz / 256 = 62.5 kHz on D3.
  TCCR2A = _BV(WGM21) | _BV(WGM20);
  TCCR2B = _BV(CS20);
  OCR2B = 0;

  // Timer 1: counts to the sample period and interrupts (enabled in startSound).
  TCCR1A = 0;
  TCCR1B = _BV(WGM12) | _BV(CS10);
  OCR1A = F_CPU / SOUND_RATE - 1;
}

void loop() {
  float cm = readCm();
  unsigned long now = millis();

  if (cm < ON_CM) {
    lastSeen = now;
    if (!playing) startSound();
    // 0 at the edge of range, 1 at LOUD_CM or closer
    float nearness = constrain((ON_CM - cm) / (ON_CM - LOUD_CM), 0.0, 1.0);
    float target = QUIET + nearness * (255 - QUIET);
    level += (target - level) * 0.3;
    vol = (uint8_t)level;
  } else if (playing && now - lastSeen > OFF_MS) {
    stopSound();
  }

  Serial.print("cm ");
  Serial.print(cm, 1);
  Serial.print(" vol ");
  Serial.print(vol);
  Serial.println();

  delay(50);                     // about 20 readings a second
}
