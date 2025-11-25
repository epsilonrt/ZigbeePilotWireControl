/*
  SPDX-License-Identifier: BSD-3-Clause
  SPDX-FileCopyrightText: 2025 Pascal JEAN aka epsilonrt

  This example creates a virtual Zigbee Pilot Wire Control device
  using the ZigbeePilotWireControl class with a serial output to indicate the mode:
  - Off: "Pilot Wire Mode: OFF"
  - Frost: "Pilot Wire Mode: FROST PROTECTION"
  - Eco: "Pilot Wire Mode: ECO"
  - Comfort-2: "Pilot Wire Mode: COMFORT -2"
  - Comfort-1: "Pilot Wire Mode: COMFORT -1"
  - Comfort: "Pilot Wire Mode: COMFORT"
  .

  A button is used to cycle through the modes.
  If the button is held for more than 3 seconds, the Zigbee stack is reset
  to factory defaults.
  The current mode is saved in NVS if restore mode is enabled, and restored
  on startup.
  Make sure to select "ZCZR coordinator/router" mode in Tools->Zigbee mode
*/
#include <Arduino.h>

#ifndef ZIGBEE_MODE_ZCZR
#error "Zigbee coordinator mode is not selected in Tools->Zigbee mode"
#endif

#include <Zigbee.h>
#include <ZigbeePilotWireControl.h>

const uint16_t ZbeeEndPoint = 1;
const uint8_t button = BOOT_PIN;

// Create ZigbeePilotWireControl instance
ZigbeePilotWireControl  zbPilot (ZbeeEndPoint);

// Callback function to handle pilot wire mode changes
void
setPilotWire (ZigbeePilotWireMode mode) {
  // Array of mode names for serial output
  static const char *const modeNames[] = {
    "OFF",
    "COMFORT",
    "ECO",
    "FROST",
    "COMFORT-1",
    "COMFORT-2"
  };
  Serial.printf ("Pilot Wire Mode: %s\n", modeNames[mode]);
}

void setup() {
  Serial.begin (115200);

  // wait for serial port to connect...
  //
  while (!Serial) {

    delay (100);
  }
  delay (2000);

  Serial.println ("Zigbee Virtual Pilot Wire Control starting...");

  // Init button for factory reset
  pinMode (button, INPUT_PULLUP);

  // Set callback function for pilot wire mode change and power state change
  zbPilot.onPilotWireModeChange (setPilotWire);

  // Initialize the Pilot Wire Control endpoint
  zbPilot.begin ();
  zbPilot.enableNvs (true); // restore pilot wire mode, energy summation from NVS

  // Add endpoint to Zigbee Core
  Serial.println ("Adding ZigbeePilotWireControl endpoint to Zigbee Core");
  Zigbee.addEndpoint (&zbPilot);

  // When all EPs are registered, start Zigbee in ROUTER mode
  if (!Zigbee.begin (ZIGBEE_ROUTER)) {
    Serial.println ("Zigbee failed to start! Rebooting...");
    ESP.restart();
  }

  Serial.print ("Connecting to network");
  while (!Zigbee.connected()) {

    Serial.print (".");
    delay (500);
  }
  Serial.println ("\nZigbee connected to network.");

  if (zbPilot.reportAttributes()) {

    Serial.println ("Pilot Wire attributes reported");
  }
  else {

    Serial.println ("Failed to report Pilot Wire attributes");
  }
}

void loop() {

  // Checking button for factory reset
  if (digitalRead (button) == LOW) { // Push button pressed
    // Key debounce handling
    delay (100);
    unsigned long t = millis();
    while (digitalRead (button) == LOW) {
      delay (50);
      if ( (millis() - t) > 3000) {
        // If key pressed for more than 3secs, factory reset Zigbee and reboot
        Serial.println ("Resetting Zigbee to factory and rebooting in 1s.");
        delay (1000);
        Zigbee.factoryReset();
      }
    }

    t = millis() - t;
    Serial.printf ("Button pressed for %lu ms\n", t);
    if (t < 3000) {
      uint8_t mode = zbPilot.pilotWireMode();

      mode = (mode + 1) % PILOTWIRE_MODE_COUNT;
      zbPilot.setPilotWireMode (static_cast<ZigbeePilotWireMode> (mode));
    }
  }
  delay (100);
}
