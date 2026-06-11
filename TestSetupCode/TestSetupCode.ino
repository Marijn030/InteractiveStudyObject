// --------------------------------------------------
// Pin definitions
// --------------------------------------------------
#include <LiquidCrystal.h>

const int rs = 12, en = 8, d4 = 7, d5 = 4, d6 = A0, d7 = 2;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

// Buttons (active HIGH, external pull-down)
// Knop 1 = tandwiel, Knop 2 = -1 min, Knop 3 = +1 min, Knop 4 = start
const int button1Pin = A1;  // tandwiel
const int button2Pin = A2;  // -1 min
const int button3Pin = A3;  // +1 min
const int button4Pin = A4;  // start

// Tilt sensor (INPUT_PULLUP → LOW = upright, HIGH = flipped)
const int tiltPin = A5;

// Welke fysieke ledgroep staat boven?
// Dit wordt na iedere stabiele verandering van de tilt-switch omgewisseld.
bool led1Boven = true;

// RGB LED 1 (boven)
const int led1R = 9;
const int led1G = 10;
const int led1B = 11;

// RGB LED 2 (onder)
const int led2R = 3;
const int led2G = 5;
const int led2B = 6;

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
// Timer instellingen (instelbaar via menu)
// --------------------------------------------------
int studieminuten = 25;
int pauzeminuten  = 5;

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
// Knopstatus (voor stijgende flank detectie)
// --------------------------------------------------
bool prevBtn1 = LOW, prevBtn2 = LOW, prevBtn3 = LOW, prevBtn4 = LOW;

// --------------------------------------------------
// Knipperlogica (voor STUDIE_KLAAR / PAUZE_KLAAR)
// --------------------------------------------------
unsigned long lastBlinkTime = 0;
bool blinkOn = false;
const unsigned long BLINK_INTERVAL = 250;

// --------------------------------------------------
// Pauze kleur (1 van 4, random gekozen)
// --------------------------------------------------
// 0 = rood, 1 = groen, 2 = geel, 3 = paars
int pauzeKleur = 0;

// --------------------------------------------------
// Hulpfuncties LED
// --------------------------------------------------
void setLED1(int r, int g, int b) {
  analogWrite(led1R, r);
  analogWrite(led1G, g);
  analogWrite(led1B, b);
}

void setLED2(int r, int g, int b) {
  analogWrite(led2R, r);
  analogWrite(led2G, g);
  analogWrite(led2B, b);
}

void ledsUit() {
  setLED1(0, 0, 0);
  setLED2(0, 0, 0);
}

// Zandloper-effect: boven dimt, onder licht op
void updateStudieLEDs() {
  unsigned long verstreken = millis() - timerStart;
  if (verstreken >= huidigeDuur) verstreken = huidigeDuur;

  int boven = map(verstreken, 0, huidigeDuur, 255, 0);
  int onder = map(verstreken, 0, huidigeDuur, 0, 255);

  int waardeLED1 = led1Boven ? boven : onder;
  int waardeLED2 = led1Boven ? onder : boven;

  setLED1(0, 0, waardeLED1);
  setLED2(0, 0, waardeLED2);
}

void updatePauzeLEDs() {
  unsigned long verstreken = millis() - timerStart;
  if (verstreken >= huidigeDuur) verstreken = huidigeDuur;

  int boven = map(verstreken, 0, huidigeDuur, 255, 0);
  int onder = map(verstreken, 0, huidigeDuur, 0, 255);

  int waardeLED1 = led1Boven ? boven : onder;
  int waardeLED2 = led1Boven ? onder : boven;

  switch (pauzeKleur) {
    case 0: setLED1(waardeLED1, 0, 0);                setLED2(waardeLED2, 0, 0);                break; // rood
    case 1: setLED1(0, waardeLED1, 0);                setLED2(0, waardeLED2, 0);                break; // groen
    case 2: setLED1(waardeLED1, waardeLED1, 0);       setLED2(waardeLED2, waardeLED2, 0);       break; // geel
    case 3: setLED1(waardeLED1/2, 0, waardeLED1);     setLED2(waardeLED2/2, 0, waardeLED2);     break; // paars
  }
}

void updateBlinkLEDs() {
  if (millis() - lastBlinkTime >= BLINK_INTERVAL) {
    lastBlinkTime = millis();
    blinkOn = !blinkOn;
    if (blinkOn) { setLED1(255, 255, 255); setLED2(255, 255, 255); }
    else           ledsUit();
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
  char buf[17];
  snprintf(buf, sizeof(buf), "%d min", minuten);
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
  char buf[17];
  snprintf(buf, sizeof(buf), "%d min", studieminuten);
  lcdRegel(1, buf);
}

void naarMenuPauze() {
  currentState = MENU_PAUZE;
  lcdRegel(0, "Break time:");
  char buf[17];
  snprintf(buf, sizeof(buf), "%d min", pauzeminuten);
  lcdRegel(1, buf);
}

void naarMenuKlaar() {
  currentState = MENU_KLAAR;
  lcdRegel(0, "Ready to start?");
  lcdRegel(1, "Flip hourglass!");
}

void startStudieTimer() {
  currentState = STUDIE_TIMER;
  huidigeDuur  = (unsigned long)studieminuten * 60UL * 1000UL;
  timerStart   = millis();
  lcdRegel(0, "Studying...");
}

void startPauzeTimer() {
  currentState = PAUZE_TIMER;
  huidigeDuur  = (unsigned long)pauzeminuten * 60UL * 1000UL;
  timerStart   = millis();
  pauzeKleur   = random(0, 4);

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

  if (btn1) naarMenuStudie();   // tandwiel → instellingen
  if (btn4) naarMenuKlaar();    // start → direct naar klaar-scherm
}

void handleMenuStudie(bool btn1, bool btn2, bool btn3, bool btn4) {
  bool gewijzigd = false;

  if (btn2 && studieminuten > 1)  { studieminuten--; gewijzigd = true; }
  if (btn3 && studieminuten < 60) { studieminuten++; gewijzigd = true; }

  if (gewijzigd) {
    char buf[17];
    snprintf(buf, sizeof(buf), "%d min", studieminuten);
    lcdRegel(1, buf);
  }

  if (btn1 || btn4) naarMenuPauze(); // tandwiel of start → door naar pauze menu
}

void handleMenuPauze(bool btn1, bool btn2, bool btn3, bool btn4) {
  bool gewijzigd = false;

  if (btn2 && pauzeminuten > 1)  { pauzeminuten--; gewijzigd = true; }
  if (btn3 && pauzeminuten < 30) { pauzeminuten++; gewijzigd = true; }

  if (gewijzigd) {
    char buf[17];
    snprintf(buf, sizeof(buf), "%d min", pauzeminuten);
    lcdRegel(1, buf);
  }

  if (btn1 || btn4) naarMenuKlaar(); // tandwiel of start → klaar scherm
}

void handleMenuKlaar(bool btn1, bool omgedraaid) {
  if (btn1)        naarMenuStudie();  // tandwiel → terug naar instellingen
  if (omgedraaid)  startStudieTimer(); // omdraaien → start!
}

void handleStudieTimer() {
  unsigned long verstreken = millis() - timerStart;
  updateStudieLEDs();
  toonMinuten(verstreken, huidigeDuur);

  if (verstreken >= huidigeDuur) {
    currentState = STUDIE_KLAAR;
    lcdRegel(0, "Time for a break");
    lcdRegel(1, "Flip hourglass!");
    ledsUit();
    lastBlinkTime = millis();
  }
}

void handleStudieKlaar(bool omgedraaid) {
  updateBlinkLEDs();
  if (omgedraaid) startPauzeTimer();
}

void handlePauzeTimer() {
  unsigned long verstreken = millis() - timerStart;
  updatePauzeLEDs();
  toonMinuten(verstreken, huidigeDuur);

  if (verstreken >= huidigeDuur) {
    currentState = PAUZE_KLAAR;
    lcdRegel(0, "Break over!");
    lcdRegel(1, "Flip hourglass!");
    ledsUit();
    lastBlinkTime = millis();
  }
}

void handlePauzeKlaar(bool omgedraaid) {
  updateBlinkLEDs();
  if (omgedraaid) startStudieTimer();
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
    led1Boven = !led1Boven;
    return true;
  }
  return false;
}

// --------------------------------------------------
// Setup
// --------------------------------------------------
void setup() {
  lcd.begin(16, 2);

  pinMode(button1Pin, INPUT);
  pinMode(button2Pin, INPUT);
  pinMode(button3Pin, INPUT);
  pinMode(button4Pin, INPUT);
  pinMode(tiltPin, INPUT_PULLUP);

  pinMode(led1R, OUTPUT); pinMode(led1G, OUTPUT); pinMode(led1B, OUTPUT);
  pinMode(led2R, OUTPUT); pinMode(led2G, OUTPUT); pinMode(led2B, OUTPUT);

  randomSeed(analogRead(A3));

  stableTiltState = digitalRead(tiltPin);
  lastTiltState   = stableTiltState;

  naarStartscherm();
}

// --------------------------------------------------
// Loop
// --------------------------------------------------
void loop() {
  bool omgedraaid = tiltVeranderd();

  // Lees knoppen (stijgende flank)
  bool b1 = digitalRead(button1Pin);
  bool b2 = digitalRead(button2Pin);
  bool b3 = digitalRead(button3Pin);
  bool b4 = digitalRead(button4Pin);

  bool btn1 = (b1 == HIGH && prevBtn1 == LOW);
  bool btn2 = (b2 == HIGH && prevBtn2 == LOW);
  bool btn3 = (b3 == HIGH && prevBtn3 == LOW);
  bool btn4 = (b4 == HIGH && prevBtn4 == LOW);

  prevBtn1 = b1; prevBtn2 = b2; prevBtn3 = b3; prevBtn4 = b4;

  switch (currentState) {
    case STARTSCHERM:   handleStartscherm(btn1, btn4);                break;
    case MENU_STUDIE:   handleMenuStudie(btn1, btn2, btn3, btn4);     break;
    case MENU_PAUZE:    handleMenuPauze(btn1, btn2, btn3, btn4);      break;
    case MENU_KLAAR:    handleMenuKlaar(btn1, omgedraaid);            break;
    case STUDIE_TIMER:  handleStudieTimer();                          break;
    case STUDIE_KLAAR:  handleStudieKlaar(omgedraaid);                break;
    case PAUZE_TIMER:   handlePauzeTimer();                           break;
    case PAUZE_KLAAR:   handlePauzeKlaar(omgedraaid);                 break;
  }

  delay(50);
}