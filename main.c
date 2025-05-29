#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Inițializarea display-ului LCD I2C la adresa 0x27, 16 caractere pe 2 linii
LiquidCrystal_I2C lcd(0x27, 16, 2);

const int ldrPin = A0;         // Pinul analogic pentru senzorul LDR (fotorezistor)
int ldrValue = 0;              // Valoarea citită de la LDR

// Pinurile pentru LED-uri (12 LED-uri conectate pe pini digitali 2-13)
const int ledPins[] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13};
const int numLeds = 12;        

const int buzzerPin = A1;      // Pinul buzzer-ului

int prevValue = 0;             // Valoarea anterioară a senzorului (pentru a calcula derivata)
unsigned long lastBeatTime = 0; // Timpul ultimei "bătăi" detectate
int bpm = 0;                   // Calculul bătăilor pe minut (BPM)

const int bpmHistorySize = 5;  // Dimensiunea bufferului pentru istoricul BPM
int bpmHistory[bpmHistorySize] = {0}; // Buffer circular pentru valorile BPM
int bpmIndex = 0;              // Indexul curent în istoricul BPM

float smoothBPM = 0;           // Valoare BPM filtrată (smooth)
const float smoothingFactor = 0.2; // Factorul pentru filtrarea valorii BPM

// Variabile pentru control buzzer ritmic
unsigned long lastBuzzerToggle = 0;
bool buzzerOn = false;
const unsigned long buzzerOnDuration = 50;  // ms sunet buzzer aprins

void setup() {
  Serial.begin(9600);

  // Inițializare LCD (backlight pornit și mesaj de start)
  lcd.begin(16, 2, 0x00);
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Citire LDR");

  // Setare pini LED ca output și aprindere toate LED-urile
  for (int i = 0; i < numLeds; i++) {
    pinMode(ledPins[i], OUTPUT);
    digitalWrite(ledPins[i], HIGH);
  }

  // Setare pin buzzer ca output și oprire ton buzzer
  pinMode(buzzerPin, OUTPUT);
  noTone(buzzerPin);
}

void controlLeds(int bpmValue) {
  // Mapare bpm de la 40-140 la numarul de LED-uri aprinse
  int ledsToLight = map(bpmValue, 40, 140, 0, numLeds);
  ledsToLight = constrain(ledsToLight, 0, numLeds);

  // Aprind LED-uri până la ledsToLight, restul stinse
  for (int i = 0; i < numLeds; i++) {
    if (i < ledsToLight) {
      digitalWrite(ledPins[i], HIGH);
    } else {
      digitalWrite(ledPins[i], LOW);
    }
  }
}

void controlBuzzer(int bpmValue) {
  if (bpmValue > 40) { // pornește doar dacă bpm > 40
    unsigned long now = millis();
    unsigned long interval = 60000UL / bpmValue; // intervalul total între bipuri în ms

    if (buzzerOn) {
      // Dacă sunetul e pornit și a trecut durata sunetului, oprește buzzerul
      if (now - lastBuzzerToggle >= buzzerOnDuration) {
        noTone(buzzerPin);
        buzzerOn = false;
        lastBuzzerToggle = now;
      }
    } else {
      // Dacă buzzerul e oprit și a trecut timpul de pauză, pornește buzzerul
      if (now - lastBuzzerToggle >= (interval - buzzerOnDuration)) {
        tone(buzzerPin, 1000);
        buzzerOn = true;
        lastBuzzerToggle = now;
      }
    }

  } else {
    // Sub 40 bpm buzzer oprit
    noTone(buzzerPin);
    buzzerOn = false;
    lastBuzzerToggle = 0;
  }
}

// Calculeaza media valorilor BPM din istoric
float getAverageBPM() {
  int sum = 0;
  int count = 0;
  for (int i = 0; i < bpmHistorySize; i++) {
    if (bpmHistory[i] > 0) {
      sum += bpmHistory[i];
      count++;
    }
  }
  return count > 0 ? (float)sum / count : 0;
}

void loop() {
  // Citire valoare LDR
  ldrValue = analogRead(ldrPin);

  // Filtrare medie mobila pe ultimele 10 citiri pentru stabilizare semnal
  static int readings[10] = {0};
  static int index = 0;
  static long total = 0;

  total -= readings[index];
  readings[index] = ldrValue;
  total += readings[index];
  index = (index + 1) % 10;

  int filteredValue = total / 10;  // Valoare filtrata

  // Calcul derivata pentru detectarea cresterii rapide a luminii (posibila bataie)
  int derivative = filteredValue - prevValue;
  unsigned long now = millis();

  // Detectare "bataie" pe baza cresterii rapide a luminii si timp minim intre batai
  if (derivative > 1) {
    unsigned long interval = now - lastBeatTime;
    if (interval > 400) {       // Interval minim intre batai 400 ms (150 bpm max)
      bpm = 60000 / interval;   // Calcul BPM pe baza intervalului
      lastBeatTime = now;

      // Stocare BPM in buffer circular
      bpmHistory[bpmIndex] = bpm;
      bpmIndex = (bpmIndex + 1) % bpmHistorySize;
    }
  }

  prevValue = filteredValue;

  // Calcul medie BPM din istoric pentru stabilizare
  float avgBPM = getAverageBPM();

  // Filtrare suplimentara (smooth) a BPM-ului pentru afisare
  if (avgBPM > 0) {
    smoothBPM = smoothingFactor * avgBPM + (1 - smoothingFactor) * smoothBPM;
  }

  int displayBPM = (int)(smoothBPM);

  // Afisare BPM pe LCD
  lcd.setCursor(0, 0);
  lcd.print("BPM: ");
  lcd.print(displayBPM);
  lcd.print("    "); // sterge caractere ramase daca valoarea scade

  // Afisare valoare filtrata LDR pe LCD (pentru debugging/monitorizare)
  lcd.setCursor(0, 1);
  lcd.print("Val: ");
  lcd.print(filteredValue);
  lcd.print("    ");

  // Control LED-uri si buzzer pe baza valorii BPM
  controlLeds(displayBPM);
  controlBuzzer(displayBPM);

  delay(20);  // Delay scurt pentru stabilizare si pentru a nu suprasolicita CPU
}
