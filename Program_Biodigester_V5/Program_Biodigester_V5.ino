#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <max6675.h>
#include <DS3231.h>
#include <SPI.h>
#include "MQ135.h"

/*
   ANALOG
   Methane              A8
   Pressure 1           A9
   Pressure 2           A10
   pH                   A11
   CO2                  A12
*/

/* DIGITAL
   Volume (Flow)       D22
   Temperature (SCK)   D24
   Temperature (CS)    D26
   Temperature (SO)    D28
   Level               D30
   Buzzer              D39
*/

/*
   SDA                D20
   SCL                D21
   GND                GND
*/

#define methaneMQ2          A8    // definisikan sensor metana
#define pressureSensor1     A9    // definisikan sensor tekanan 1
#define pressureSensor2     A10   // definisikan sensor tekanan 2
#define phSensor            A11   // definisikan sensor pH
#define carbonMQ135         A12   // definisikan sensor C02

const byte tempSCK          = 24; // pin data SCK termokepel pada pin 24 arduino mega
const byte tempCS           = 26; // pin data CS termokepel pada pin 26 arduino mega
const byte tempSO           = 28; // pin data SO termokepel pada pin 28 arduino mega
const byte levelSensor      = 22; // pin data level sensor pada pin 30 arduino mega
const byte buzzerPin        = 39; // pin buzzer pada pin 39 arduino mega

const byte strobeRelay      = 31; // pin IN4 relay pada pin 31 arduino mega
const byte valveRelay       = 33; // pin IN3 relay pada pin 33 arduino mega
const byte motorRelay       = 35; // pin IN2 relay pada pin 35 arduino mega

//const int pressureZero = 100.0; //pembacaan analog transduser tekanan pada 0psi
//const int pressureMax = 921.6; //pembacaan analog transduser tekanan pada 174psi
//const int pressuretransducermaxPSI = 100; // nilai psi dari transduser yang

LiquidCrystal_I2C lcd1 = LiquidCrystal_I2C (0x27, 20, 4); //LCD 1
LiquidCrystal_I2C lcd2 = LiquidCrystal_I2C (0x25, 20, 4); // LCD 2

int liquidLevel             = 0;        //LEVEL LIMBAH TANGKI 1
String kondisiTangki;                   //LEVEL LIMBAH TANGKI 1

int pressureAnalog1;
int pressureAnalog2;
float pressure1;                        //TEKANAN TANGKI 1
float pressure2;                        //TEKANAN TANGKI 2
float threshold = 10.15;                // Nilai ambang batas tekanan untuk mengaktifkan buzzer

int temperatureValue;                   //TEMPERATUR TANGKI 1
MAX6675 thermocouple(tempSCK, tempCS, tempSO);

float phAnalog;
float finalphValue          = 0.0;      //PH TANGKI 1

//#define RL 10
//#define m 4159.91953                    //hasil kalibrasi
//#define b -2.714712023                  //hasil kalibrasi
//#define Ro 0.80
int methaneGas              = 0;        //Metana
float CH4Voltage;
int methanePPM;                         //Metana
//float Rs;
//float Ratio;
//float ppm;

int carbonPPM;                          //CO2
MQ135 CO2Sensor (carbonMQ135);

// set waktu nyala dan mati pengaduk otomatis
const byte onHour   = 13;
const byte onMin    = 0;
const byte offHour  = 13;
const byte offMin   = 30;

DS3231 rtc (SDA, SCL);
Time t;

void setup() {
  Serial.begin (9600);

  pinMode (methaneMQ2, INPUT);        //MQ2 Sensor
  pinMode (pressureSensor1, INPUT);   //Pressure Sensor 1
  pinMode (pressureSensor2, INPUT);   //Pressure Sensor 2
  pinMode (phSensor, INPUT);          //pH Sensor
  pinMode (carbonMQ135, INPUT);       //MQ135 Sensor
  pinMode (levelSensor, INPUT);       //Level Sensor

  pinMode (strobeRelay, OUTPUT);      //Strobe Lamp Relay
  pinMode (valveRelay, OUTPUT);       //Valve Relay
  pinMode (motorRelay, OUTPUT);       //AC Motor Relay
  pinMode (buzzerPin, OUTPUT);        //Buzzer

  //  Inisialisasi LCD
  lcd1.init();
  lcd2.init();
  lcd1.backlight();
  lcd2.backlight();

  //  Inisialisasi RTC
  rtc.begin();

//    rtc.setDate(19, 8, 2023);   //mensetting tanggal 21 juli 2023
//    rtc.setDOW(6);              //menset hari "Minggu"
//    rtc.setTime(16, 45, 20);    //menset jam 13:02:40

  lcd1.setCursor (0, 0);
  lcd1.print ("Tangki   : "); // kosong atau penuh
  lcd1.setCursor (0, 1);
  lcd1.print ("Tekanan1 : ");
  lcd1.setCursor (17, 1);
  lcd1.print ("Bar");
  lcd1.setCursor (0, 2);
  lcd1.print ("Suhu     :   ");
  lcd1.setCursor (18, 2);
  lcd1.print ((char)223);
  lcd1.print ("C");
  lcd1.setCursor (0, 3);
  lcd1.print ("pH Limbah: ");

  lcd2.setCursor (0, 0);
  lcd2.print ("Metana  : ");
  lcd2.setCursor (17, 0);
  lcd2.print ("PPM");
  lcd2.setCursor (0, 1);
  lcd2.print ("CO2     : ");
  lcd2.setCursor (17, 1);
  lcd2.print ("PPM");
  lcd2.setCursor (0, 2);
  lcd2.print ("Tekanan2: ");
  lcd2.setCursor (17, 2);
  lcd2.print ("Bar");
  lcd2.setCursor (0, 3);
  lcd2.print ("Tanggal : ");

  Serial.println ("A0");

}

void loop() {

  t = rtc.getTime();

  //  Serial.print("Jam : ");
  //  Serial.print(t.hour);
  //  Serial.print(" : ");
  //  Serial.print(t.min);
  //  Serial.print(" : ");
  //  Serial.print(t.sec);
  //  Serial.println(" ");

  LevelSensor();

  if (t.hour == onHour && t.min == onMin)
  {
    digitalWrite (motorRelay, LOW);
  }
  else if (t.hour == offHour && t.min == offMin)
  {
    digitalWrite (motorRelay, HIGH);
  }

  MethaneSensor();
  CarbonSensor();
  PressureSens1();
  PressureSens2();
  ThermoSensor();
  PhSensor();

  Serial.println ("A1");

  // Kontrol Valve
  if ((finalphValue >= 6.5 && finalphValue <= 7.5)  || (temperatureValue >= 20 && temperatureValue <= 30))
  {
    digitalWrite (valveRelay, LOW); // Nyala
  }
  else
  {
    digitalWrite (valveRelay, HIGH);
  }

  Serial.println ("A2");

  // Kontrol Strobe Light
  if (pressure1 >= 0.4)
  {
    digitalWrite (strobeRelay, HIGH);
  }
  else
  {
    digitalWrite (strobeRelay, LOW); // Nyala
  }

  Serial.println ("A3");

  // Kontrol buzzer
  if (pressure1 >= threshold) {
    digitalWrite(buzzerPin, HIGH); // Hidupkan buzzer
  } else {
    digitalWrite(buzzerPin, LOW); // Matikan buzzer
  }

  Serial.println ("A4");

  // Print Nilai Sensor Pada LCD 1
  lcd1.setCursor (11, 1);
  lcd1.print (pressure1, 1);
  Serial.print("Tekanan 1 : ");
  Serial.println (pressure1, 1);

  lcd1.setCursor (13, 2);
  lcd1.print (int (temperatureValue));
  Serial.print("Suhu : ");
  Serial.println (temperatureValue);

  lcd1.setCursor (11, 3);
  lcd1.print (finalphValue);
  Serial.print("Nilai pH : ");
  Serial.println (finalphValue);

  Serial.println ("A5");

  // Print Nilai Sensor Pada LCD 2
  lcd2.setCursor (9, 0);
  lcd2.print ("       ");
  lcd2.setCursor (10, 0);
  lcd2.print (methanePPM);
  Serial.print("Metana : ");
  Serial.println (methanePPM);

  lcd2.setCursor (9, 1);
  lcd2.print ("       ");
  lcd2.setCursor (10, 1);
  lcd2.print (carbonPPM);
  Serial.print("CO2 : ");
  Serial.println (carbonPPM);

  lcd2.setCursor (10, 2);
  lcd2.print (pressure2, 1);
  Serial.print("Tekanan 2 : ");
  Serial.println (pressure2, 1);

  lcd2.setCursor (10, 3);
  lcd2.print(rtc.getDateStr());

  Serial.println ("A6");

  delay(1250);

}

void LevelSensor() {
  // Kondisi Level Tangki
  liquidLevel = digitalRead (levelSensor);
  if (liquidLevel == 1)
  {
    digitalWrite (motorRelay, HIGH);
    lcd1.setCursor (11, 0);
    lcd1.print ("Penuh ");
    kondisiTangki = "Penuh";
    Serial.println (liquidLevel);
    Serial.println (kondisiTangki);
  }
  else
  {
    digitalWrite (motorRelay, LOW); // Nyala
    lcd1.setCursor (11, 0);
    lcd1.print ("Kosong");
    kondisiTangki = "Kosong";
    Serial.println (liquidLevel);
    Serial.println (kondisiTangki);
  }

  Serial.println ("A7");
}

void MethaneSensor() {
  // Sensor Metana
  methaneGas = analogRead (methaneMQ2);
  CH4Voltage = methaneGas * (5.0 / 1023.0);
  methanePPM = (15000 / 1024) * ((CH4Voltage / 5) * 1024);
  //  Rs = ((5.0 * RL) / methaneGas) - RL;
  //  Ratio = Rs / Ro;
  //  logRatio = log10(Ratio);
  //  ppm = pow(10, ((log10(Ratio) - b) / m));

  Serial.println ("A8");
}

void CarbonSensor() {
  // Sensor Karbon
  carbonPPM = CO2Sensor.getPPM();

  Serial.println ("A9");
}

void PressureSens1() {
  // Sensor Tekanan
  pressureAnalog1     = analogRead (pressureSensor1);
  pressure1      = pressure (pressureAnalog1);
}

void PressureSens2() {
  // Sensor Tekanan
  pressureAnalog2     = analogRead (pressureSensor2);
  pressure2      = pressure (pressureAnalog2);

  Serial.println ("A10");
}

void ThermoSensor() {
  // Sensor Termocouple
  temperatureValue = thermocouple.readCelsius();

  Serial.println ("A11");
}

void PhSensor() {
  // Sensor pH
  phAnalog = analogRead (phSensor);
  finalphValue = (-0.0693 * phAnalog) + 7.3855;

  Serial.println ("A12");
}

float pressure (int pressureValue)
{
  float voltage = (pressureValue * 5.0) / 1024.0;
  float pressurePascal = (3.0 * ((float)voltage - 0.47)) * 1000000.0;
  float pressureBar = pressurePascal / 10e5;
  return pressureBar;
}
