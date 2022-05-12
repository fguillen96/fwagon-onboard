/****************************************************************************************
 * DESCRIPTION: Feed Wagon parameters sender using LoRa
 * AUTOR: Fran Guillén Arenas
 * COMPANY: CabraControl
*****************************************************************************************/

#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>
#include <ArduinoJson.h>
#include <debug.h>
#include <Servo.h>

// ---------- PIN DEFINITION  ---------
#define AI_MOTOR_0          A0
#define AI_MOTOR_1          A1
#define AI_BATTERY          A2
#define DO_SERVO_CONTROL    4
#define DI_REED_SENSOR      2

// -------- LORA MODULE CONFIG ---------
#define SS      10
#define RST     9
#define DIO0    8
#define BAND    868E6

// -------- CONSTATNS CONFIG ---------
const float voltage_divider_adjust = 0.0304 ;    // 0.0282;     // Voltage divider adjustement
const int adc_rotation_tolerance = 500;          // Far away from 0 to avoid noise problems
const float v_batt_charging = 26.5;

// --------- GLOBAL VAR ----------
const unsigned int lora_data_rate = 1000;
Servo myservo; 


// ---------- FUNCTION HEADERS ----------
char motorRotation();



void setup() {
  #if DEBUGGING
  Serial.begin(9600);
  #endif

  // -------- LORA SETUP ---------
  SPI.begin();
  LoRa.setPins(SS, RST, DIO0);
  
  if (!LoRa.begin(BAND)) {
    DEBUGLN("LoRa failed!");
    while (1);
  }
  DEBUGLN("LoRa OK!");

  pinMode(DI_REED_SENSOR, INPUT);


  // ---------- SERVO ---------
  myservo.attach(DO_SERVO_CONTROL);  // attaches the servo on pin 9 to the servo object

}


void loop() {
  // --------- CHECK IF LORA DATA AVAILABLE ----------
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    const int capacity = JSON_OBJECT_SIZE(1);
    StaticJsonDocument<capacity> doc;

    deserializeJson(doc, LoRa);

    int servo_angle = doc["servoAngle"];
    DEBUGLN(servo_angle);
    myservo.write(servo_angle);
    delay(1000);
    myservo.write(0);
  }


  // -------- SEND DATA EVERY LORA DATA RATE ---------
  static unsigned long prev_millis = 0;
  if ((millis() - prev_millis) >= lora_data_rate) {
    prev_millis = millis();

    // Getting data
    float v_battery = int(analogRead(AI_BATTERY) * voltage_divider_adjust * 100.0) / 100.0;
    char v_motor = int((analogRead(AI_MOTOR_0) - analogRead(AI_MOTOR_1)) * voltage_divider_adjust * 100.0) / 100.0;
    bool at_home = !digitalRead(DI_REED_SENSOR);

    // Prepairing Json document
    StaticJsonDocument<50> doc; //32 recommended
    char buffer[100];

    doc["vBat"] = v_battery;
    doc["vMot"] = v_motor;
    doc["home"] = at_home;

    // Serialize Json
    size_t length = serializeJson(doc, buffer);
    DEBUGLN(buffer);

    // Send LoRa packet
    LoRa.beginPacket();
    LoRa.write(buffer, length);
    LoRa.endPacket();
  }
}