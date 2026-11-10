#include <EEPROM.h>
#include <util/atomic.h>

const byte FOT_PIN = A0;
const byte BUTTON_PIN = 2;  
const byte BUZZER = 6;

#define RED_BIT    PB0       
#define GREEN_BIT  PB1      
#define BLUE_BIT   PB2      
#define LED_MASK   ((1 << RED_BIT) | (1 << GREEN_BIT) | (1 << BLUE_BIT))

const unsigned long DEFAULT_THRESHOLD = 512;
const int MID_ZONE = 40;
const unsigned long LONG_PRESS_MS = 1000;
const unsigned long DEBOUNCE_MS = 200;

struct Settings {
  uint16_t magic;
  byte version;
  unsigned long threshold;
  byte buzzerOn;
};

const uint16_t EEPROM_MAGIC = 0x4C31;
const byte EEPROM_VERSION = 1;

Settings settings;

volatile unsigned long timerTicks = 0;
volatile bool flagMeasure = false;
volatile bool flagPrint = false;
volatile bool buttonPressed = false;

enum LightState {
  DARK,
  MEDIUM,
  BRIGHT
};

LightState lightState = MEDIUM;

int lightValue = 0;
bool haveFirst = false;

bool btnHeld = false;
bool longDone = false;
unsigned long btnStart = 0;
unsigned long lastButtonTime = 0;

unsigned long lastBeep = 0;
unsigned long confirmBeepUntil = 0;
bool buzzerState = false;

unsigned long getTicks() {
  unsigned long result;

  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    result = timerTicks;
  }

  return result;
}

ISR(TIMER1_COMPA_vect) {
  static byte measureCounter = 0;
  static unsigned int printCounter = 0;

  timerTicks++;

  measureCounter++;
  if (measureCounter >= 50) {
    measureCounter = 0;
    flagMeasure = true;
  }

  printCounter++;
  if (printCounter >= 500) {
    printCounter = 0;
    flagPrint = true;
  }
}

void buttonISR() {
  buttonPressed = true;
}

void saveSettings() {
  EEPROM.put(0, settings);   
}

void loadSettings() {
  EEPROM.get(0, settings);

  bool valid =
    settings.magic == EEPROM_MAGIC &&
    settings.version == EEPROM_VERSION &&
    settings.threshold <= 1023 &&
    settings.threshold >=10 &&
    settings.buzzerOn <= 1;

  if (!valid) {
    settings.magic = EEPROM_MAGIC;
    settings.version = EEPROM_VERSION;
    settings.threshold = DEFAULT_THRESHOLD;
    settings.buzzerOn = 1;
    saveSettings();
    Serial.println(F("EEPROM neteisingas, naudojamos default reiksmes"));
  } else {
    Serial.println(F("EEPROM OK"));
  }
}

void setupTimer1() {
  cli();

  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1 = 0;

  OCR1A = 249;

  TCCR1B |= (1 << WGM12);
  TCCR1B |= (1 << CS11) | (1 << CS10); // prescaler 64 
  TIMSK1 |= (1 << OCIE1A);

  sei();
}

void setupLeds() {
  DDRB |= LED_MASK;        
  PORTB &= ~LED_MASK;      
}

void setLeds(LightState s) {
  uint8_t bits;

  if (s == DARK)        bits = (1 << RED_BIT);
  else if (s == MEDIUM) bits = (1 << BLUE_BIT);
  else                  bits = (1 << GREEN_BIT);

  PORTB = (PORTB & ~LED_MASK) | bits; 
}

void setBuzzer(bool on) {
  digitalWrite(BUZZER, on ? HIGH : LOW);
  buzzerState = on;
}

void confirmBeep(unsigned long ms) {
  confirmBeepUntil = getTicks() + ms;
}

void setup() {
  Serial.begin(9600);

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUZZER, OUTPUT);
  digitalWrite(BUZZER, LOW);

  setupLeds();

  loadSettings();
  setupTimer1();

  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonISR, FALLING);

  lastBeep = getTicks();
}

void loop() {
  unsigned long now = getTicks();

  bool pressed = false;
  bool doMeasure = false;
  bool doPrint = false;

  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    if (buttonPressed) {
      buttonPressed = false;
      pressed = true;
    }

    if (flagMeasure) {
      flagMeasure = false;
      doMeasure = true;
    }

    if (flagPrint) {
      flagPrint = false;
      doPrint = true;
    }
  }

  if (pressed && !btnHeld && now - lastButtonTime >= DEBOUNCE_MS) {
    btnHeld = true;
    longDone = false;
    btnStart = now;
  }

  if (btnHeld) {
    if (digitalRead(BUTTON_PIN) == LOW) {      
      if (!longDone && now - btnStart >= LONG_PRESS_MS) {
        settings.buzzerOn ^= 1;
        saveSettings();
        longDone = true;
        if (settings.buzzerOn) confirmBeep(300);

        Serial.print(F("Buzzer: "));
        Serial.println(settings.buzzerOn ? F("ON") : F("OFF"));
      }
    } else {                                    
      if (!longDone) {
        settings.threshold = lightValue;
        saveSettings();
        confirmBeep(150);

        Serial.print(F("Threshold saved: "));
        Serial.println(settings.threshold);
      }
      btnHeld = false;
      lastButtonTime = now;
    }
  }

  if (doMeasure) {
    int raw = analogRead(FOT_PIN);

    if (!haveFirst) {
      lightValue = raw;
      haveFirst = true;
    } else {
      lightValue = (lightValue * 3 + raw) / 4;
    }

    if (lightValue < settings.threshold - MID_ZONE)      lightState = DARK;
    else if (lightValue > settings.threshold + MID_ZONE) lightState = BRIGHT;
    else                                                  lightState = MEDIUM;

    setLeds(lightState);
  }

  if (doPrint) {
    Serial.print(F("Brightness: "));
    Serial.print(lightValue);
    Serial.print(F("  Threshold: "));
    Serial.print(settings.threshold);
    Serial.print(F("  State: "));
    if (lightState == DARK)        Serial.print(F("Dark"));
    else if (lightState == MEDIUM) Serial.print(F("Normal"));
    else                           Serial.print(F("Bright"));
    Serial.print(F("  Buzzer: "));
    Serial.println(settings.buzzerOn ? F("ON") : F("OFF"));
  }

  if ((long)(confirmBeepUntil - now) > 0) {
    setBuzzer(true);                      
  } else if (settings.buzzerOn && lightState == DARK) {
    unsigned long interval = (lightValue < settings.threshold / 2) ? 150 : 600;

    if (!buzzerState && now - lastBeep >= interval) {
      setBuzzer(true);
      lastBeep = now;
    } else if (buzzerState && now - lastBeep >= 40) {
      setBuzzer(false);
    }
  } else {
    setBuzzer(false);
  }
}
