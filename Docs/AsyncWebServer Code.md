```

AsyncWebServer Request, sendsendWOR, and sendLoRaPayload

  server.on("/relay", HTTP_GET, [](AsyncWebServerRequest *request) {
    getDateTime();
    timestamp = dtStamp; 
    String option = "1";
    String payload = "1," + timestamp;
    Serial.println("\n=== WEB REQUEST RECEIVED ===");
    Serial.printf("Sending WOR payload: %s\n", payload.c_str());
    sendWOR();  // Send WOR-style preamble
    delay(250);
    sendLoRaPayload(payload);  //Sending command and timestamp
    request->send(200, "text/plain", "Sent WOR payload: " + payload);
  });



void sendWOR() {
  String wakeonRadio = "3, WOR--1234567890qwerty";
  int state;
  unsigned long transmitTime = 0;
  Serial.println("=== STARTING TRANSMISSION ===");
  Serial.printf("Payload: %s\n", wakeonRadio.c_str());
  Serial.printf("Payload length: %d bytes\n", wakeonRadio.length());
  Serial.printf("Preamble length: %d symbols\n", LORA_PREAMBLE_LENGTH);
  Serial.printf("Spreading Factor: %d\n", LORA_SPREADING_FACTOR);

  // Send payload transmission
  unsigned long startTime = millis();
  state = radio.transmit(wakeonRadio);
  transmitTime = millis() - startTime;

  delay(100);
  
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("Packet transmitted successfully!");
    Serial.printf("Transmission time: %lu ms\n", transmitTime);
    Serial.printf("Data rate: %.2f bps\n", (wakeonRadio.length() * 8.0) / (transmitTime / 1000.0));
    Serial.println("=== TRANSMISSION COMPLETE ===");
  } else {
    Serial.printf("Transmission failed! Error code: %d\n", state);
    
    // Decode common error codes
    switch(state) {
      case RADIOLIB_ERR_PACKET_TOO_LONG:
        Serial.println("Error: Packet too long");
        break;
      case RADIOLIB_ERR_TX_TIMEOUT:
        Serial.println("Error: Transmission timeout");
        break;
      case RADIOLIB_ERR_CHIP_NOT_FOUND:
        Serial.println("Error: Radio chip not responding");
        break;
      default:
        Serial.println("Check RadioLib documentation for error code details");
    }
  }
  
  Serial.println();
}



void sendLoRaPayload(String payload) {
  int state;
  unsigned long transmitTime = 0;
  Serial.println("=== STARTING TRANSMISSION ===");
  Serial.printf("Payload: %s\n", payload.c_str());
  Serial.printf("Payload length: %d bytes\n", payload.length());
  Serial.printf("Preamble length: %d symbols\n", LORA_PREAMBLE_LENGTH);
  Serial.printf("Spreading Factor: %d\n", LORA_SPREADING_FACTOR);

  // Send payload transmission
  unsigned long startTime = millis();
  state = radio.transmit(payload);
  transmitTime = millis() - startTime;
 
 delay(600);

  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("Packet transmitted successfully!");
    Serial.printf("Transmission time: %lu ms\n", transmitTime);
    Serial.printf("Data rate: %.2f bps\n", (payload.length() * 8.0) / (transmitTime / 1000.0));
    Serial.println("=== TRANSMISSION COMPLETE ===");
  } else {
    Serial.printf("Transmission failed! Error code: %d\n", state);
    
    // Decode common error codes
    switch(state) {
      case RADIOLIB_ERR_PACKET_TOO_LONG:
        Serial.println("Error: Packet too long");
        break;
      case RADIOLIB_ERR_TX_TIMEOUT:
        Serial.println("Error: Transmission timeout");
        break;
      case RADIOLIB_ERR_CHIP_NOT_FOUND:
        Serial.println("Error: Radio chip not responding");
        break;
      default:
        Serial.println("Check RadioLib documentation for error code details");
    }
  }
  
  Serial.println();
}

```






