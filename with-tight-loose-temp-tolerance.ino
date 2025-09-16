#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// --- Pins ---editable
#define ONE_WIRE_BUS 3  //define digital pin to communicate with DS18B20
#define PIN_D4 4  //for ditital output
#define PIN_D5 5  //for ditital output
#define PIN_D6 6  //for ditital output
#define PIN_D7 7  //for ditital output

// --- OneWire and DS18B20 setup ---
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// --- LCD setup ---editable
LiquidCrystal_I2C lcd(0x27, 16, 2);

// --- Temperature settings ---editable
const float targetTemp = 25.0;
const float toleranceTight = 0.5;  // ±0.5 °C
const float toleranceLoose = 1.0;  // ±1.0 °C

// --- Timing variables ---
unsigned long actionStartTime = 0;
unsigned long d7HoldStart = 0;
unsigned long d5CycleStart = 0;

bool d7HoldActive = false;
bool d5HighPhase = true;

// --- Status tracking ---
String currentStatus = "";
String previousStatus = "";

// --- Functions ---
void createDegreeSymbol() {
  byte myChar[8] = {0x02,0x05,0x02,0,0,0,0,0};  //create the degree symbol
  lcd.createChar(0, myChar);
}

float getTemperature() {
  sensors.requestTemperatures();
  delay(750); // DS18B20 conversion!!!! important!!!!
  return sensors.getTempCByIndex(0);
}

String getStatus(float temp) {
  float lowTight = targetTemp - toleranceTight;  // 24.5
  float highTight = targetTemp + toleranceTight; // 25.5
  float lowLoose = targetTemp - toleranceLoose;  // 24.0
  float highLoose = targetTemp + toleranceLoose; // 26.0

  if (previousStatus != "Good") {
    // Enter Good only inside tight range
    if (temp >= lowTight && temp <= highTight) return "Good";
    else if (temp < lowTight) return "Heating";
    else return "Cooling";
  } else {
    // Already in Good → use loose hysteresis
    if (temp < lowLoose) return "Heating";
    else if (temp > highLoose) return "Cooling";
    else return "Good";
  }
}

void displayInfo(float temp, String status) {
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(status);
  lcd.setCursor(0,1);
  lcd.print(temp,1);
  lcd.write(byte(0));
  lcd.print("C");
}

// --- Non-blocking output control ---
void updateOutputs(String status) {
  unsigned long now = millis();

  // --- Handle D7 5-minute hold after leaving Cooling ---
  if (previousStatus == "Cooling" && status != "Cooling" && !d7HoldActive) {
    d7HoldActive = true;
    d7HoldStart = now;
  }

  if (d7HoldActive) {
    if (status == "Cooling") d7HoldActive = false; // cancel hold
    else if (now - d7HoldStart >= 300000UL) { // 5 min
      digitalWrite(PIN_D7, LOW);
      d7HoldActive = false;
    } else {
      digitalWrite(PIN_D7, HIGH);
    }
  }

  // --- Handle outputs based on status ---
  if (status != previousStatus) {
    // Reset timers when status changes
    actionStartTime = now;
    d5CycleStart = now;
    d5HighPhase = true;
  }

  if (status == "Good") {                                            //1.inner fan cycle blowing   2. prevent from over-heat  3. hysteresis control
    digitalWrite(PIN_D4, LOW);
    digitalWrite(PIN_D6, LOW);
    if(!d7HoldActive) digitalWrite(PIN_D7, LOW);

    // D5 blinking cycle: 10s high / 30s low
    unsigned long elapsed = now - d5CycleStart;
    if (d5HighPhase && elapsed >= 10000UL) { // 10s high done
      d5HighPhase = false;
      d5CycleStart = now;
    } else if (!d5HighPhase && elapsed >= 30000UL) { // 30s low done
      d5HighPhase = true;
      d5CycleStart = now;
    }
    digitalWrite(PIN_D5, d5HighPhase ? HIGH : LOW);
  }
  else if (status == "Heating") {
    digitalWrite(PIN_D4, HIGH);
    digitalWrite(PIN_D5, HIGH);
    digitalWrite(PIN_D6, LOW);
    if(!d7HoldActive) digitalWrite(PIN_D7, LOW);
  }
  else if (status == "Cooling") {
    digitalWrite(PIN_D4, LOW);
    digitalWrite(PIN_D5, HIGH);
    digitalWrite(PIN_D6, HIGH);
    digitalWrite(PIN_D7, HIGH);
  }
}

// --- Setup ---
void setup() {
  pinMode(PIN_D4, OUTPUT);
  pinMode(PIN_D5, OUTPUT);
  pinMode(PIN_D6, OUTPUT);
  pinMode(PIN_D7, OUTPUT);

  sensors.begin();
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0,0);
  lcd.print("Temp Control");
  delay(1000);
  createDegreeSymbol();
}

// --- Loop ---
void loop() {
  float temp = getTemperature();
  currentStatus = getStatus(temp);

  displayInfo(temp, currentStatus);
  updateOutputs(currentStatus);

  previousStatus = currentStatus;
}