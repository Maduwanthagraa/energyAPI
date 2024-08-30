#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <PZEM004Tv30.h>
#include <RTClib.h>
#include <SoftwareSerial.h>

#define LCD_COLUMNS 16
#define LCD_ROWS 4
#define RELAY_PIN 2
#define LOW_VOLTAGE_THRESHOLD 210
#define HIGH_VOLTAGE_THRESHOLD 240
#define USER_PHONE_NUMBER "+94757358093"

RTC_DS3231 rtc;
char daysOfTheWeek[7][9] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
int date;
int count = 0;

SoftwareSerial gsmSerial(12, 13);
PZEM004Tv30 pzem(10, 11);
LiquidCrystal_I2C lcd(0x27, LCD_COLUMNS, LCD_ROWS);

float energy;
DateTime nowTime;
float monEnergy;
float tempEnergy;
int eCount = 0;
String textMessage;
String lampState = "HIGH";

void setup() {
  Serial.begin(9600);
  lcdSetup();
  rtcSetup();
  gsmtextsetup();
  pinMode(RELAY_PIN, OUTPUT);
}

void loop() {
  safety();
  if (gsmSerial.available() > 0) {
    textMessage = gsmSerial.readString();
    gsmSerial.print(textMessage);    
    delay(10);
  } 
  disptime();
  getreadings();
  sendvalues();
  alertsms();
  checkSMSCommands();
}

void lcdSetup() {
  lcd.begin();
  lcd.backlight();
  lcd.clear();
  lcd.print("ENERGY MONITOR");
  delay(3000);
}

void rtcSetup() {
  Wire.begin();
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }
  if (rtc.lostPower()) {
    Serial.println("RTC lost power, setting the time...");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
}

void gsmtextsetup() {
  gsmSerial.begin(9600);
  delay(2000);
  gsmSerial.println("AT");
  delay(1000);
  if (gsmSerial.available()) {
    Serial.println(gsmSerial.readString());
  }
  gsmSerial.println("AT+CMGF=1");
  delay(1000);
  gsmSerial.println("AT+CNMI=2,2,0,0,0");
  delay(1000);
  delay(2000);
  Serial.print("SIM900 ready...");
}

void safety() {
  float voltageValue = pzem.voltage();
  if (voltageValue >= LOW_VOLTAGE_THRESHOLD && voltageValue <= HIGH_VOLTAGE_THRESHOLD) {
    digitalWrite(RELAY_PIN, HIGH);
  } else if (voltageValue > HIGH_VOLTAGE_THRESHOLD) {
    digitalWrite(RELAY_PIN, LOW);
    lcd.clear();
    lcd.print("High Voltage Detected.");
    delay(4000);
    lcd.clear();
    sendSMS(USER_PHONE_NUMBER, "High Voltage! Disconnected.");
  } else if (voltageValue < LOW_VOLTAGE_THRESHOLD) {
    digitalWrite(RELAY_PIN, LOW);
    lcd.clear();
    lcd.print("Low Voltage Detected.");
    delay(4000);
    lcd.clear();
    sendSMS(USER_PHONE_NUMBER, "Low Voltage! Disconnected.");
  }
}

void disptime() {
  nowTime = rtc.now();
  lcd.clear();
  int x = nowTime.dayOfTheWeek();
  lcd.setCursor(0, 0);
  lcd.print(daysOfTheWeek[x]);
  lcd.print("... ");
  lcd.setCursor(0, 1);
  lcd.print(nowTime.day());
  lcd.print("/");
  lcd.print(nowTime.month());
  lcd.print("/");
  lcd.print(nowTime.year());
  date = nowTime.day();
  lcd.setCursor(-4, 2);
  lcd.print("Time: ");
  lcd.print(nowTime.hour());
  lcd.print(":");
  if (nowTime.minute() < 10) lcd.print("0");
  lcd.print(nowTime.minute());
  lcd.print(":");
  if (nowTime.second() < 10) lcd.print("0");
  lcd.print(nowTime.second());
  delay(5000);
}

void getreadings() {
  float voltage = pzem.voltage();
  float current = pzem.current();
  float power = pzem.power();
  energy = pzem.energy();
  float frequency = pzem.frequency();
  float pf = pzem.pf();

  if (isnan(voltage) || isnan(current) || isnan(power) || isnan(energy) || isnan(frequency) || isnan(pf)) {
    Serial.println("Error reading sensor data");
    return;
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Voltage: ");
  lcd.print(voltage, 2);
  lcd.print("V");
  delay(3000);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Current: ");
  lcd.print(current, 2);
  lcd.print("A");
  delay(3000);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Power: ");
  lcd.print(power, 2);
  lcd.print("W");
  delay(3000);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("PF: ");
  lcd.print(pf, 2);
  lcd.setCursor(0, 1);
  lcd.print("Freq: ");
  lcd.print(frequency, 2);
  lcd.print("Hz");
  delay(3000);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Energy: ");
  lcd.print(energy);
  lcd.print("kWh");
  delay(3000);

  if (date == 1 && eCount < 1) {
    tempEnergy = energy;
    eCount = 1;
  } else {
    eCount = 0;
  }

  monEnergy = energy - tempEnergy;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Monthly Energy: ");
  lcd.setCursor(0, 1);
  lcd.print(monEnergy);
  lcd.print("kWh");
  delay(3000);
  lcd.clear();
}

void sendvalues() {
  lcd.print("Data sending...");
  String device_id = "A001";
  String billdate = String(nowTime.year()) + "-" + String(nowTime.month()) + "-" + String(nowTime.day()) + " " + String(nowTime.hour()) + ":" + String(nowTime.minute()) + ":" + String(nowTime.second());
  float energyvalue = energy;
  String payload = "{ \"device_id\":\"" + device_id + "\", \"billdate\":\"" + billdate + "\", \"energyvalue\":\"" + String(energyvalue) + "\" }";

  gsmSerial.begin(9600);
  delay(2000);

  sendATCommand("AT");
  sendATCommand("AT+SAPBR=3,1,\"Contype\",\"GPRS\"");
  sendATCommand("AT+SAPBR=3,1,\"APN\",\"dialogbb\"");
  sendATCommand("AT+SAPBR=1,1");
  delay(3000);

  sendATCommand("AT+HTTPINIT");
  delay(2000);
  sendATCommand("AT+HTTPPARA=\"CID\",1");
  delay(2000);
  sendATCommand("AT+HTTPPARA=\"URL\",\"3.0.19.217/data\"");
  delay(2000);
  sendATCommand("AT+HTTPPARA=\"CONTENT\",\"application/json\"");
  delay(2000);
  sendATCommand("AT+HTTPDATA=" + String(payload.length()) + ",10000", false);
  delay(2000);
  sendATCommand(payload, false);
  delay(2000);
  sendATCommand("AT+HTTPACTION=1");

  delay(10000);
  sendATCommand("AT+HTTPREAD");
  delay(2000);

  sendATCommand("AT+HTTPTERM");
  delay(8000);
  lcd.clear();
  lcd.print("Data sent to database.");
  delay(3000);
  lcd.clear();
}

void alertsms() {
  if (monEnergy > 1.0 && count < 1) {
    sendSMS(USER_PHONE_NUMBER, "Your energy usage exceeding 50 units.");
    count = count + 1;
    delay(5000);
  }
}

void checkSMSCommands() {
  if (textMessage.indexOf("ON") >= 0) {
    digitalWrite(RELAY_PIN, HIGH);
    lampState = "on";
    Serial.println("Relay set to ON");
    sendSMS(USER_PHONE_NUMBER, "SUPPLY CONNECTED.");
    delay(3000);
  }
  if (textMessage.indexOf("OFF") >= 0) {
    digitalWrite(RELAY_PIN, LOW);
    lampState = "off";
    Serial.println("Relay set to OFF");
    sendSMS(USER_PHONE_NUMBER, "SUPPLY DISCONNECTED.");
    delay(30000);
  }
}

void sendSMS(String mobileNumber, String content) {
  gsmSerial.println("AT+CMGS=\"" + mobileNumber + "\"");
  delay(1000);
  gsmSerial.println(content);
  delay(1000);
  gsmSerial.println((char)26);
  delay(1000);
}

void sendATCommand(String command, bool isPrintResponse = true) {
  gsmSerial.println(command);
  delay(500);
  if (isPrintResponse) {
    while (gsmSerial.available()) {
      Serial.write(gsmSerial.read());
    }
  }
}
