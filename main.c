#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Configure LCD (I2C address, 16 columns, 2 rows)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Pin for LDR reading
const int ldrPin = A0;  

// Signal processing variables
int ldrValue = 0;            // Current raw LDR value
int filteredLdrValue = 0;    // Filtered value
int prevFilteredValue = 0;   // Previous filtered value
int minValue = 1023;         // Minimum recorded value
int maxValue = 0;            // Maximum recorded value

// Variables for moving average filter
const int numReadings = 20;  // Number of readings for smoothing
int readings[20];            // Array for moving average
int readIndex = 0;           // Index for readings array
long total = 0;              // Total for averaging

// Variables for calculating BPM
unsigned long lastBeatTime = 0;    // Time of last detected beat
unsigned long beatTimes[10];       // Last 10 beat times
int beatIndex = 0;                 // Index for circular beat array
int bpm = 60;                      // Beats per minute (initial)
unsigned long lastPrintTime = 0;   // Time for display update

// Pulse detection variables
int threshold = 0;                 // Threshold for pulse detection (dynamic)
bool pulseState = false;           // Current pulse state
unsigned long lastDebounceTime = 0;// Time of last beat for debounce
const int debounceDelay = 500;     // Debounce delay (ms)

// Advanced signal processing
int baseline = 0;                  // Signal baseline (DC)
int signalPeak = 0;                // Peak value during calibration
int signalValley = 1023;           // Valley value during calibration
int derivativeValue = 0;           // Rate of change of signal

// LED pins
const int ledPins[] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13}; 
const int numLeds = 12; 

// Buzzer pin
const int buzzerPin = A1;  

// Buzzer variables
bool buzzerState = false;
unsigned long buzzerTime = 0;
const int buzzerFreq = 1000;      // 1kHz buzzer frequency
const int buzzerOnTime = 50;      // Sound duration (ms)
const int buzzerOffTime = 700;    // Pause between sounds (ms)

// Calibration variables
bool isCalibrating = true;
unsigned long calibrationStartTime = 0;
const unsigned long calibrationDuration = 5000; // 5 seconds calibration

// Measurement cycle timing
const unsigned long measureDuration = 5000; // 5 seconds data capture
const unsigned long waitDuration = 3000;    // 3 seconds wait between cycles
unsigned long cycleStartTime = 0;
bool measuring = true;

// Constants for smoothing BPM display
const int baseBPM = 60;             // Base BPM for smoothing
const float smoothingFactor = 0.2;  // Weight for new BPM in smoothing

void setup() {
  Serial.begin(9600);  
  
  lcd.begin(16, 2);
  lcd.setBacklight(1);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Pulse Sensor");
  lcd.setCursor(0, 1);
  lcd.print("Calibrating...");

  for (int i = 0; i < numLeds; i++) {
    pinMode(ledPins[i], OUTPUT);
    digitalWrite(ledPins[i], HIGH);
  }
  
  pinMode(buzzerPin, OUTPUT);
  noTone(buzzerPin);
  
  for (int i = 0; i < numReadings; i++) readings[i] = 0;
  for (int i = 0; i < 10; i++) beatTimes[i] = 0;
  
  tone(buzzerPin, buzzerFreq);
  delay(200);
  noTone(buzzerPin);
  
  calibrationStartTime = millis();
  
  // Warm-up readings
  for (int i = 0; i < 100; i++) {
    analogRead(ldrPin);
    delay(5);
  }
  
  total = 0;
  for (int i = 0; i < numReadings; i++) {
    readings[i] = analogRead(ldrPin);
    total += readings[i];
    delay(10);
  }
  filteredLdrValue = total / numReadings;
  prevFilteredValue = filteredLdrValue;
  baseline = filteredLdrValue;

  buzzerTime = millis();
  cycleStartTime = millis();
  measuring = true;
}

void loop() {
  unsigned long currentTime = millis();

  updateBuzzer();

  if (isCalibrating) {
    if (currentTime - calibrationStartTime > calibrationDuration) {
      isCalibrating = false;
      baseline = (signalPeak + signalValley) / 2;
      threshold = (signalPeak - signalValley) / 4;
      minValue = signalValley;
      maxValue = signalPeak;

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Calibration done");
      delay(1000);
      lcd.clear();
      cycleStartTime = currentTime;
      measuring = true;
    }
    return;
  }

  if (measuring) {
    // Measure for 5 seconds with timer on display
    ldrValue = analogRead(ldrPin);

    total = total - readings[readIndex];
    readings[readIndex] = ldrValue;
    total = total + readings[readIndex];
    readIndex = (readIndex + 1) % numReadings;

    filteredLdrValue = (filteredLdrValue * 7 + (total / numReadings) * 3) / 10;
    derivativeValue = filteredLdrValue - prevFilteredValue;

    minValue = min(minValue, filteredLdrValue);
    maxValue = max(maxValue, filteredLdrValue);

    minValue = minValue + (filteredLdrValue - minValue) * 0.001;
    maxValue = maxValue - (maxValue - filteredLdrValue) * 0.001;

    threshold = minValue + (maxValue - minValue) * 0.3;

    if (!pulseState && filteredLdrValue > threshold && derivativeValue > 0 &&
        currentTime - lastDebounceTime > debounceDelay) {
      pulseState = true;

      unsigned long beatInterval = currentTime - lastBeatTime;

      if (beatInterval > 333 && beatInterval < 2000) {
        beatTimes[beatIndex] = currentTime;
        beatIndex = (beatIndex + 1) % 10;

        lastBeatTime = currentTime;
        lastDebounceTime = currentTime;

        calculateBPM();
      }
    } else if (pulseState && filteredLdrValue < threshold && derivativeValue < 0) {
      pulseState = false;
    }

    // Show timer counting down measurement seconds
    unsigned long elapsed = currentTime - cycleStartTime;
    if (elapsed < measureDuration) {
      lcd.setCursor(0, 0);
      lcd.print("Masurare: ");
      lcd.print((measureDuration - elapsed) / 1000 + 1);
      lcd.print("s   ");
      lcd.setCursor(0, 1);
      lcd.print("BPM: ");
      lcd.print(bpm);
      lcd.print("       ");
    } else {
      // Show final BPM after measurement
      lcd.setCursor(0, 0);
      lcd.print("Puls: ");
      lcd.print(bpm);
      lcd.print(" bpm   ");
      lcd.setCursor(0, 1);
      lcd.print("Urmatoarea masurare");
      lcd.setCursor(0, 1);
      lcd.print("in 3 secunde   ");
      measuring = false;
      cycleStartTime = currentTime; // reset timer for waiting
    }
  } else {
    // Waiting 3 seconds before next measurement cycle
    unsigned long waitElapsed = currentTime - cycleStartTime;
    lcd.setCursor(0, 0);
    lcd.print("Urmatoarea masurare");
    lcd.setCursor(0, 1);
    lcd.print("in ");
    lcd.print((waitDuration - waitElapsed) / 1000 + 1);
    lcd.print("s     ");

    if (waitElapsed >= waitDuration) {
      cycleStartTime = currentTime;
      measuring = true;

      // Reset arrays and values for next cycle
      for (int i = 0; i < 10; i++) beatTimes[i] = 0;
      beatIndex = 0;
      bpm = baseBPM;
      minValue = 1023;
      maxValue = 0;
      total = 0;
      for (int i = 0; i < numReadings; i++) readings[i] = 0;
      readIndex = 0;
    }
  }

  prevFilteredValue = filteredLdrValue;

  // Debug output
  Serial.print("Raw: ");
  Serial.print(ldrValue);
  Serial.print("\tFiltered: ");
  Serial.print(filteredLdrValue);
  Serial.print("\tThreshold: ");
  Serial.print(threshold);
  Serial.print("\tBPM: ");
  Serial.println(bpm);

  delay(10);
}

void calculateBPM() {
  unsigned long totalInterval = 0;
  int validIntervals = 0;

  for (int i = 0; i < 9; i++) {
    int idx1 = (beatIndex - i - 1 + 10) % 10;
    int idx2 = (beatIndex - i - 2 + 10) % 10;

    if (beatTimes[idx1] > 0 && beatTimes[idx2] > 0) {
      unsigned long interval = beatTimes[idx1] - beatTimes[idx2];

      if (interval > 333 && interval < 2000) {
        totalInterval += interval;
        validIntervals++;
      }
    }
  }

  if (validIntervals > 0) {
    unsigned long avgInterval = totalInterval / validIntervals;
    int newBPM = 60000 / avgInterval;

    // Smooth bpm by combining base BPM and new value
    bpm = (int)(baseBPM * (1 - smoothingFactor) + newBPM * smoothingFactor);

    if (bpm < 40) bpm = 40;
    if (bpm > 200) bpm = 200;
  } else {
    bpm = baseBPM;
  }
}

void updateBuzzer() {
  unsigned long currentTime = millis();

  if (millis() - lastBeatTime < 3000) {
    unsigned long beatInterval = 60000 / bpm;

    if (currentTime - buzzerTime > (buzzerState ? buzzerOnTime : (beatInterval - buzzerOnTime))) {
      buzzerTime = currentTime;
      buzzerState = !buzzerState;

      if (buzzerState) {
        tone(buzzerPin, buzzerFreq);
      } else {
        noTone(buzzerPin);
      }
    }
  } else {
    noTone(buzzerPin);
    buzzerState = false;
  }
}
