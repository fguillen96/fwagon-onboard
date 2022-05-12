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
#include <EEPROM.h>

// ---------- PIN DEFINITION  ---------
#define AI_MOTOR_0          A0
#define AI_MOTOR_1          A1
#define AI_BATTERY          A2
#define DO_SERVO_CONTROL    4
#define DI_HALL_SENSOR      3
#define DI_REED_SENSOR      2
#define DO_LED              LED_BUILTIN

// -------- LORA MODULE CONFIG ---------
#define SS      10
#define RST     9
#define DIO0    8
#define BAND    868E6

// -------- CONSTATNS CONFIG ---------
const float voltage_divider_adjust = 0.0304 ;    // 0.0282;     // Voltage divider adjustement
const int adc_rotation_tolerance = 500;          // Far away from 0 to avoid noise problems
const float v_batt_charging = 26.5;
const int max_lap_eeaddress = 0;

// --------- GLOBAL VAR ----------
const unsigned int lora_data_rate = 1000;
Servo myservo; 
struct {
  volatile int current = 0;
  volatile int max_trip = 0;
  volatile int max = 0;
} laps_from_home;




// ---------- FUNCTION HEADERS ----------
char motorRotation();


/**********************************
              ISR
**********************************/

void ISR_pulseEvent() {
  char motor_rotation = motorRotation();
  
  if(motor_rotation == 'D') {
    laps_from_home.current ++;
    laps_from_home.max ++;
  }
  else if(motor_rotation == 'R') {
    laps_from_home.current --;
  } 
}

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

  pinMode(DI_HALL_SENSOR, INPUT);
  pinMode(DI_REED_SENSOR, INPUT);

  // --------- HALL SENSOR INTERRUPT --------
  attachInterrupt(digitalPinToInterrupt(DI_HALL_SENSOR), ISR_pulseEvent, FALLING);

  // ---------- SERVO ---------
  myservo.attach(DO_SERVO_CONTROL);  // attaches the servo on pin 9 to the servo object

   // ---------- GET EEPROM PARAMETERS ---------
  EEPROM.get(max_lap_eeaddress, laps_from_home.max);
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

    // Checking if we are at home and setting values
    if (at_home || (v_battery >= v_batt_charging)) {
      if (laps_from_home.max != laps_from_home.max_trip) {
        laps_from_home.max = laps_from_home.max_trip;
        EEPROM.put(max_lap_eeaddress, laps_from_home.max);
      }
      laps_from_home.current = 0;
      laps_from_home.max_trip = 0;
    }

    // Prepairing Json document
    StaticJsonDocument<50> doc; //32 recommended
    char buffer[100];

    doc["vBat"] = v_battery;
    doc["vMot"] = v_motor;
    doc["home"] = at_home;
    doc["laps"]["cur"] = laps_from_home.current;
    doc["laps"]["max"] = laps_from_home.max;

    // Serialize Json
    size_t length = serializeJson(doc, buffer);
    DEBUGLN(buffer);

    // Send LoRa packet
    LoRa.beginPacket();
    LoRa.write(buffer, length);
    LoRa.endPacket();
  }
}



char motorRotation () {
  int adc = analogRead(AI_MOTOR_0) - analogRead(AI_MOTOR_1);
  char rotation;
  if(adc > adc_rotation_tolerance)  {
    rotation = 'F';
  }
  else if(adc < - adc_rotation_tolerance) {
    rotation = 'R';
  }
  else {
    rotation = 'S';
  }

  return rotation;
}

