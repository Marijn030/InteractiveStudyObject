// --------------------------------------------------
// Pin definitions
// --------------------------------------------------
#include <LiquidCrystal.h>

const int rs = 12, en = 8, d4 = 7, d5 = 4, d6 = A0, d7 = 2;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

// Buttons (active HIGH, external pull-down)
// Knop 1 = menu / tandwiel
// Knop 2 = uitgeschakeld
// Knop 3 = uitgeschakeld
// Knop 4 = tijd kiezen / start
const int button1Pin = A1;  // menu / tandwiel
const int button2Pin = A2;  // uitgeschakeld
const int button3Pin = A3;  // uitgeschakeld
const int button4Pin = A4;  // tijd kiezen / start

// Tilt sensor (INPUT_PULLUP dan LOW = upright, HIGH = flipped)
const int tiltPin = A5;

// RGB-ledtype: false = common cathode, true = common anode
// Zet dit op true wanneer de leds omgekeerd reageren.
const bool RGB_COMMON_ANODE = false;

// Oriëntatie van de fysieke zandklok.
// true: bij LOW van de tilt-switch zit ledgroep 1 fysiek boven.
// false: bij LOW van de tilt-switch zit ledgroep 2 fysiek boven.
// Verander dit wanneer het zandloper-effect in de verkeerde richting loopt.
const bool TILT_LOW_MEANS_LED1_IS_TOP = true;

// RGB-ledgroep 1: de twee leds in fysieke helft A
const int led1R = 11;
const int led1G = 10;
const int led1B = 9;

// RGB-ledgroep 2: de twee leds in fysieke helft B
const int led2R = 6;
const int led2G = 5;
const int led2B = 3;

// Timeout wanneer er gewacht wordt op flippen
unsigned long wachtStart = 0;
const unsigned long WACHT_TIMEOUT = 120000; // 2 minuten

// --------------------------------------------------
// State machine
// --------------------------------------------------
enum State {
  STARTSCHERM,
  MENU_STUDIE,
  MENU_PAUZE,
  MENU_KLAAR,
  STUDIE_TIMER,
  STUDIE_KLAAR,
  PAUZE_TIMER,
  PAUZE_KLAAR
};
State currentState = STARTSCHERM;

// --------------------------------------------------
// Timer instellingen
// --------------------------------------------------
int studieminuten = 25;
int pauzeminuten  = 5;

// Vaste keuzes voor knop 4 in het menu
const int studieOpties[] = {1, 5, 10, 15, 20, 25, 30};
const int aantalStudieOpties = 7;
int studieIndex = 5; // start op 25 minuten

const int pauzeOpties[] = {1, 5, 10};
const int aantalPauzeOpties = 3;
int pauzeIndex = 1; // start op 5 minuten

unsigned long timerStart  = 0;
unsigned long huidigeDuur = 0;

// --------------------------------------------------
// Startscherm wissellogica
// --------------------------------------------------
unsigned long lastScreenSwitch = 0;
bool startschermpagina = false;
const unsigned long SCREEN_INTERVAL = 2000;

// --------------------------------------------------
// Tilt sensor debounce
// --------------------------------------------------
int  lastTiltState   = -1;
int  stableTiltState = -1;
unsigned long tiltChangeTime = 0;
const unsigned long DEBOUNCE_MS = 300;

// --------------------------------------------------
// Knopstatus met debounce (voor stijgende flank detectie)
// --------------------------------------------------
struct ButtonState {
  int pin;
  bool lastReading;
  bool stableState;
  unsigned long lastChangeTime;
};

const unsigned long BUTTON_DEBOUNCE_MS = 20;

ButtonState button1 = { button1Pin, LOW, LOW, 0 };
ButtonState button4 = { button4Pin, LOW, LOW, 0 };

// --------------------------------------------------
// Knipperlogica (voor STUDIE_KLAAR / PAUZE_KLAAR)
// --------------------------------------------------
unsigned long lastBlinkTime = 0;
bool blinkOn = false;
const unsigned long BLINK_INTERVAL = 250;

// --------------------------------------------------
// LCD-refresh: alleen herschrijven wanneer de minuut verandert
// --------------------------------------------------
int laatstGetoondeMinuut = -1;

// --------------------------------------------------
// Pauze kleur (1 van 4, random gekozen)
// --------------------------------------------------
// 0 = rood, 1 = groen, 2 = geel, 3 = paars
int pauzeKleur = -1;

// --------------------------------------------------
// Hulpfuncties LED
// --------------------------------------------------
int ledWaarde(int waarde) {
  return RGB_COMMON_ANODE ? 255 - waarde : waarde;
}

void setLED1(int r, int g, int b) {
  analogWrite(led1R, ledWaarde(r));
  analogWrite(led1G, ledWaarde(g));
  analogWrite(led1B, ledWaarde(b));
}

void setLED2(int r, int g, int b) {
  analogWrite(led2R, ledWaarde(r));
  analogWrite(led2G, ledWaarde(g));
  analogWrite(led2B, ledWaarde(b));
}

void ledsUit() {
  setLED1(0, 0, 0);
  setLED2(0, 0, 0);
}

// Bepaal na iedere draai welke fysieke helft werkelijk boven zit.
bool led1IsBoven() {
  bool tiltIsLow = (stableTiltState == LOW);
  return TILT_LOW_MEANS_LED1_IS_TOP ? tiltIsLow : !tiltIsLow;
}

void setBovenLEDs(int r, int g, int b) {
  if (led1IsBoven()) {
    setLED1(r, g, b);
  } else {
    setLED2(r, g, b);
  }
}

void setOnderLEDs(int r, int g, int b) {
  if (led1IsBoven()) {
    setLED2(r, g, b);
  } else {
    setLED1(r, g, b);
  }
}

// Zandloper-effect: de fysiek bovenste helft dimt, de onderste licht op
void updateStudieLEDs() {
  unsigned long verstreken = millis() - timerStart;
  if (verstreken >= huidigeDuur) verstreken = huidigeDuur;

  int boven = map(verstreken, 0, huidigeDuur, 255, 0);
  int onder = map(verstreken, 0, huidigeDuur, 0, 255);

  setBovenLEDs(0, 0, boven);
  setOnderLEDs(0, 0, onder);
}

void updatePauzeLEDs() {
  unsigned long verstreken = millis() - timerStart;
  if (verstreken >= huidigeDuur) verstreken = huidigeDuur;

  int boven = map(verstreken, 0, huidigeDuur, 255, 0);
  int onder = map(verstreken, 0, huidigeDuur, 0, 255);

  switch (pauzeKleur) {
    case 0: setBovenLEDs(boven, 0, 0);          setOnderLEDs(onder, 0, 0);          break; // rood
    case 1: setBovenLEDs(0, boven, 0);          setOnderLEDs(0, onder, 0);          break; // groen
    case 2: setBovenLEDs(boven, boven, 0);      setOnderLEDs(onder, onder, 0);      break; // geel
    case 3: setBovenLEDs(boven/2, 0, boven);    setOnderLEDs(onder/2, 0, onder);    break; // paars
  }
}

void updateBlinkLEDs() {
  if (millis() - lastBlinkTime >= BLINK_INTERVAL) {
    lastBlinkTime = millis();
    blinkOn = !blinkOn;
    if (blinkOn) {
      setLED1(255, 255, 255);
      setLED2(255, 255, 255);
    } else {
      ledsUit();
    }
  }
}

// --------------------------------------------------
// Hulpfuncties LCD
// --------------------------------------------------
void lcdRegel(int rij, const char* tekst) {
  lcd.setCursor(0, rij);
  lcd.print("                ");
  lcd.setCursor(0, rij);
  lcd.print(tekst);
}

// Toon resterende tijd in minuten (afgerond naar boven)
void toonMinuten(unsigned long verstreken, unsigned long totaal) {
  unsigned long resterend = (verstreken >= totaal) ? 0 : totaal - verstreken;
  unsigned int minuten = (resterend + 59999) / 60000; // afgerond naar boven

  if ((int)minuten == laatstGetoondeMinuut) return;
  laatstGetoondeMinuut = minuten;

  char buf[17];
  snprintf(buf, sizeof(buf), "%u min", minuten);
  lcdRegel(1, buf);
}

void toonStudieMinuten() {
  char buf[17];
  snprintf(buf, sizeof(buf), "%d min", studieminuten);
  lcdRegel(1, buf);
}

void toonPauzeMinuten() {
  char buf[17];
  snprintf(buf, sizeof(buf), "%d min", pauzeminuten);
  lcdRegel(1, buf);
}

// --------------------------------------------------
// State starters
// --------------------------------------------------
void naarStartscherm() {
  currentState = STARTSCHERM;
  lastScreenSwitch = millis();
  startschermpagina = false;
  ledsUit();
  lcdRegel(0, "Press start");
  lcdRegel(1, "to study");
}

void naarMenuStudie() {
  currentState = MENU_STUDIE;
  ledsUit();
  lcdRegel(0, "Study time:");
  toonStudieMinuten();
}

void naarMenuPauze() {
  currentState = MENU_PAUZE;
  ledsUit();
  lcdRegel(0, "Break time:");
  toonPauzeMinuten();
}

void naarMenuKlaar() {
  currentState = MENU_KLAAR;
  wachtStart = millis();
  ledsUit();
  lcdRegel(0, "Ready to start?");
  lcdRegel(1, "Flip hourglass!");
}

void startStudieTimer() {
  currentState = STUDIE_TIMER;
  huidigeDuur  = (unsigned long)studieminuten * 60UL * 1000UL;
  timerStart   = millis();
  laatstGetoondeMinuut = -1;
  lcdRegel(0, "Studying...");
}

void startPauzeTimer() {
  currentState = PAUZE_TIMER;
  huidigeDuur  = (unsigned long)pauzeminuten * 60UL * 1000UL;
  timerStart   = millis();
  laatstGetoondeMinuut = -1;
  
  int nieuweKleur;

  do {
  nieuweKleur = random(0, 4);
} while (nieuweKleur == pauzeKleur);

  pauzeKleur = nieuweKleur;

  switch (pauzeKleur) {
    case 0: lcdRegel(0, "Drawer: RED");    break;
    case 1: lcdRegel(0, "Drawer: GREEN");  break;
    case 2: lcdRegel(0, "Drawer: YELLOW"); break;
    case 3: lcdRegel(0, "Drawer: PURPLE"); break;
  }
}

// --------------------------------------------------
// State handlers
// --------------------------------------------------
void handleStartscherm(bool btn1, bool btn4) {
  // Wissel tekst elke 2 seconden
  if (millis() - lastScreenSwitch >= SCREEN_INTERVAL) {
    lastScreenSwitch = millis();
    startschermpagina = !startschermpagina;
    if (startschermpagina) {
      lcdRegel(0, "Press settings");
      lcdRegel(1, "to set time");
    } else {
      lcdRegel(0, "Press start");
      lcdRegel(1, "to study");
    }
  }

  if (btn1) {
    naarMenuStudie();   // tandwiel/menu naar instellingen
  } else if (btn4) {
    naarMenuKlaar();    // start direct naar klaar-scherm
  }
}

void handleMenuStudie(bool btn1, bool btn4) {
  // Button 4 kiest studietijd: 1, 5, 10, 15, 25, 30
  if (btn4) {
    studieIndex++;
    if (studieIndex >= aantalStudieOpties) {
      studieIndex = 0;
    }

    studieminuten = studieOpties[studieIndex];
    toonStudieMinuten();
  }

  // Button 1 gaat naar pauzetijd-menu
  if (btn1) {
    naarMenuPauze();
  }
}

void handleMenuPauze(bool btn1, bool btn4) {
  // Button 4 kiest pauzetijd: 1, 5, 10
  if (btn4) {
    pauzeIndex++;
    if (pauzeIndex >= aantalPauzeOpties) {
      pauzeIndex = 0;
    }

    pauzeminuten = pauzeOpties[pauzeIndex];
    toonPauzeMinuten();
  }

  // Button 1 gaat naar klaar-scherm
  if (btn1) {
    naarMenuKlaar();
  }
}

void handleMenuKlaar(bool btn1, bool tiltGewijzigd) {
  if (btn1) {
    naarMenuStudie();
  } else if (tiltGewijzigd) {
    startStudieTimer();
  } else if (millis() - wachtStart >= WACHT_TIMEOUT) {
    naarStartscherm();
  }
}

void handleStudieTimer() {
  unsigned long verstreken = millis() - timerStart;
  updateStudieLEDs();
  toonMinuten(verstreken, huidigeDuur);

  if (verstreken >= huidigeDuur) {
    currentState = STUDIE_KLAAR;
    wachtStart = millis();
    lcdRegel(0, "Time for a break");
    lcdRegel(1, "Flip hourglass!");
    ledsUit();
    lastBlinkTime = millis();
  }
}

void handleStudieKlaar(bool omgedraaid) {
  updateBlinkLEDs();

  if (omgedraaid) {
    startPauzeTimer();
  } else if (millis() - wachtStart >= WACHT_TIMEOUT) {
    naarStartscherm();
  }
}

void handlePauzeTimer() {
  unsigned long verstreken = millis() - timerStart;
  updatePauzeLEDs();
  toonMinuten(verstreken, huidigeDuur);

  if (verstreken >= huidigeDuur) {
    currentState = PAUZE_KLAAR;
    wachtStart = millis();
    lcdRegel(0, "Break over!");
    lcdRegel(1, "Flip hourglass!");
    ledsUit();
    lastBlinkTime = millis();
  }
}

void handlePauzeKlaar(bool omgedraaid) {
  updateBlinkLEDs();

  if (omgedraaid) {
    startStudieTimer();
  } else if (millis() - wachtStart >= WACHT_TIMEOUT) {
    naarStartscherm();
  }
}

// --------------------------------------------------
// Tilt sensor debounce
// --------------------------------------------------
bool tiltVeranderd() {
  int huidig = digitalRead(tiltPin);

  if (huidig != lastTiltState) {
    tiltChangeTime = millis();
    lastTiltState  = huidig;
  }

  if ((millis() - tiltChangeTime >= DEBOUNCE_MS) && (huidig != stableTiltState)) {
    stableTiltState = huidig;
    return true;
  }
  return false;
}

// --------------------------------------------------
// Knop debounce: geeft true terug bij een stabiele LOW naar HIGH-overgang
// --------------------------------------------------
bool knopIngedrukt(ButtonState &button) {
  bool reading = digitalRead(button.pin);

  if (reading != button.lastReading) {
    button.lastChangeTime = millis();
    button.lastReading = reading;
  }

  if ((millis() - button.lastChangeTime >= BUTTON_DEBOUNCE_MS) &&
      (reading != button.stableState)) {
    button.stableState = reading;
    return button.stableState == HIGH;
  }

  return false;
}

// --------------------------------------------------
// Setup
// --------------------------------------------------
void setup() {
  lcd.begin(16, 2);

  pinMode(button1Pin, INPUT);
  // Button 2 en 3 worden niet meer gebruikt, dus ook niet gelezen.
  // pinMode(button2Pin, INPUT);
  // pinMode(button3Pin, INPUT);
  pinMode(button4Pin, INPUT);
  pinMode(tiltPin, INPUT_PULLUP);

  pinMode(led1R, OUTPUT); pinMode(led1G, OUTPUT); pinMode(led1B, OUTPUT);
  pinMode(led2R, OUTPUT); pinMode(led2G, OUTPUT); pinMode(led2B, OUTPUT);

  randomSeed(micros());

  stableTiltState = digitalRead(tiltPin);
  lastTiltState   = stableTiltState;

  naarStartscherm();
}

// --------------------------------------------------
// Loop
// --------------------------------------------------
void loop() {
  bool tiltGewijzigd = tiltVeranderd();

  // Alleen button 1 en button 4 worden gelezen.
  bool btn1 = knopIngedrukt(button1);
  bool btn4 = knopIngedrukt(button4);

  // Button 1 werkt als terug-naar-menu.
  // In MENU_STUDIE en MENU_PAUZE wordt button 1 gebruikt om door te gaan.
  if (btn1 &&
      currentState != MENU_STUDIE &&
      currentState != MENU_PAUZE &&
      currentState != STARTSCHERM) {
    ledsUit();
    naarMenuStudie();
    return;
  }

  switch (currentState) {
    case STARTSCHERM:   handleStartscherm(btn1, btn4);        break;
    case MENU_STUDIE:   handleMenuStudie(btn1, btn4);         break;
    case MENU_PAUZE:    handleMenuPauze(btn1, btn4);          break;
    case MENU_KLAAR:    handleMenuKlaar(btn1, tiltGewijzigd); break;
    case STUDIE_TIMER:  handleStudieTimer();                  break;
    case STUDIE_KLAAR:  handleStudieKlaar(tiltGewijzigd);     break;
    case PAUZE_TIMER:   handlePauzeTimer();                   break;
    case PAUZE_KLAAR:   handlePauzeKlaar(tiltGewijzigd);      break;
  }

  delay(5);
}
