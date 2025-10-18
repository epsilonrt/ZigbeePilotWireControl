// SPDX-Identifier: BSD-3-Clause
// SPDX-FileCopyrightText: 2025 Pascal JEAN aka epsilonrt

#ifndef ZIGBEE_MODE_ZCZR
#error "Zigbee coordinator mode is not selected in Tools->Zigbee mode"
#endif

#include <Arduino.h>
#include <Zigbee.h>
#include <ZigbeePilotWireControl.h>

#define ZIGBEE_PILOTWIRE_CONTROL_ENDPOINT 1

uint8_t button = BOOT_PIN;
ZigbeePilotWireControl zbPilotWireControl = ZigbeePilotWireControl(ZIGBEE_PILOTWIRE_CONTROL_ENDPOINT);

// Callback function to handle pilot wire mode changes
void setPilotWire(ZigbeePilotWireMode mode) {
  switch (mode) {
    case PILOTWIRE_MODE_OFF:
      Serial.println("Pilot Wire mode: OFF");
      break;
    case PILOTWIRE_MODE_COMFORT:
      Serial.println("Pilot Wire mode: COMFORT");
      break;
    case PILOTWIRE_MODE_ECO:
      Serial.println("Pilot Wire mode: ECO");
      break;
    case PILOTWIRE_MODE_FROST_PROTECTION:
      Serial.println("Pilot Wire mode: FROST_PROTECTION");
      break;
    case PILOTWIRE_MODE_COMFORT_MINUS_1:
      Serial.println("Pilot Wire mode: COMFORT_MINUS_1");
      break;
    case PILOTWIRE_MODE_COMFORT_MINUS_2:
      Serial.println("Pilot Wire mode: COMFORT_MINUS_2");
      break;
    default: log_e("Unhandled Pilot Wire mode: %d", mode); break;
  }
}

void setup() {
  Serial.begin(115200);

  Serial.println("Zigbee Pilot Wire Control Example");

  // Init button for factory reset
  pinMode(button, INPUT_PULLUP);

  //Optional: set Zigbee device name and model
  zbPilotWireControl.setManufacturerAndModel("Espressif", "ZBPilotWireControl");

  // Set callback function for fan mode change
  zbPilotWireControl.onPilotWireModeChange(setPilotWire);

  //Add endpoint to Zigbee Core
  Serial.println("Adding ZigbeePilotWireControl endpoint to Zigbee Core");
  Zigbee.addEndpoint(&zbPilotWireControl);

  // When all EPs are registered, start Zigbee in ROUTER mode
  if (!Zigbee.begin(ZIGBEE_ROUTER)) {
    Serial.println("Zigbee failed to start!");
    Serial.println("Rebooting...");
    ESP.restart();
  }
  Serial.println("Connecting to network");
  while (!Zigbee.connected()) {
    Serial.print(".");
    delay(100);
  }
  Serial.println();
}

void loop() {
  // Checking button for factory reset
  if (digitalRead(button) == LOW) {  // Push button pressed
    // Key debounce handling
    delay(100);
    int startTime = millis();
    while (digitalRead(button) == LOW) {
      delay(50);
      if ((millis() - startTime) > 3000) {
        // If key pressed for more than 3secs, factory reset Zigbee and reboot
        Serial.println("Resetting Zigbee to factory and rebooting in 1s.");
        delay(1000);
        Zigbee.factoryReset();
      }
    }
  }
  delay(100);
}
