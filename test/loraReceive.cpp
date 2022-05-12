  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    const int capacity = JSON_OBJECT_SIZE(1);
    StaticJsonDocument<capacity> doc;
    char buffer [packetSize + 1];

    for(int i = 0; i<packetSize; i++) {
      buffer[i] = LoRa.read();
    }
    buffer[packetSize] = '\0';

    deserializeJson(doc, buffer);

    int servo_angle = doc["servoAngle"];
    myservo.write(servo_angle);
    DEBUGLN(servo_angle);
  }
