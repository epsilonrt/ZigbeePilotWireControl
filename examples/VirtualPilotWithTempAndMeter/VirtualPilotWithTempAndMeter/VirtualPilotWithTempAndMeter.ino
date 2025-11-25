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

  This example also simulates
  1. a temperature sensor using the a ESP32-C6 built-in sensor used to
    measure the chip's internal temperature. The temperature value is updated
    every minute and reported via the thermometer cluster (0x0402).
  2. a power meter using a potentiometer as analog input to simulate power measurement via
    the ADC and the metering cluster. An analog voltage between 0 and 3.3V is mapped to
    a power value between 0 and 5000W. The instantaneous demand (power in W) and summation delivered (energy in Wh)
    attributes are updated every minute via the metering cluster (0x0702).

  Make sure to select "ZCZR coordinator/router" mode in Tools->Zigbee mode

  This sketch uses pelicanhu/ESPCPUTemp @ ^0.2.0 library to read the internal temperature of the ESP32-C6.
*/
#include <Arduino.h>

#ifndef ZIGBEE_MODE_ZCZR
#error "Zigbee coordinator mode is not selected in Tools->Zigbee mode"
#endif

#include <Zigbee.h>
#include <ZigbeePilotWireControl.h>
#include <ESPCPUTemp.h>

const uint16_t ZbeeEndPoint = 1;
const uint8_t button = BOOT_PIN;
const uint8_t powerMeterPin = A0; // GPIO0 / ADC1 channel 0

// Create ZigbeePilotWireControl instance
ZigbeePilotWireControl  zbPilot (ZbeeEndPoint, -10.0f, 80.0f, 1); // temp min -10°C, temp max 80°C, power multiplier 1
ESPCPUTemp tempSensor;

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

int32_t
readPowerMeter() {
  // Read the analog value from the power meter pin
  int32_t analogValue = analogRead (powerMeterPin);
  // Map the analog value (0-4095) to power in W (0-5000W)
  int32_t powerW = map (analogValue, 0, 4095, 0, 5000);
  return powerW;
}

void setup() {
  Serial.begin (115200);

  // wait for serial port to connect...
  //
  // while (!Serial) {

  //   delay (100);
  // }
  delay (2000);

  Serial.println ("Zigbee Virtual Pilot Wire Control starting...");

  tempSensor.begin(); // Initialize the built-in temperature sensor
  // analogSetAttenuation (ADC_11db); // Set ADC attenuation for full range (0-3.3V) for power meter pin

  // Init button for factory reset
  pinMode (button, INPUT_PULLUP);

  // Set callback function for pilot wire mode change and power state change
  zbPilot.onPilotWireModeChange (setPilotWire);

  float temperature = tempSensor.getTemp();
  int32_t powerW = readPowerMeter();
  Serial.printf ("Initial temperature: %.2f C, Initial power meter: %d W\n", temperature, powerW);

  // Initialize the Pilot Wire Control endpoint with current temperature and power meter reading
  zbPilot.begin (temperature, powerW);
  // zbPilot.enableNvs (true); // restore pilot wire mode, energy summation from NVS

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
  static unsigned long lastUpdate = 0;
  unsigned long t;

  if (digitalRead (button) == LOW) { // Push button pressed

    // Key debounce handling
    delay (100);

    // Checking button for factory reset
    t = millis();
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

      mode = (mode + 1) % PILOTWIRE_MODE_COUNT; // Cycle through modes
      zbPilot.setPilotWireMode (static_cast<ZigbeePilotWireMode> (mode));
    }
  }

  // Update temperature and power meter every minute
  t = millis();
  if (t - lastUpdate >= 60000) { // 60 seconds interval
    float temperature = tempSensor.getTemp();
    if (zbPilot.setTemperature (temperature)) {
      Serial.printf ("Pilot Wire temperature set to %.2f C\n", temperature);
      if (!zbPilot.reportTemperature()) { // Force report of temperature
        Serial.println ("Failed to report Pilot Wire temperature");
      }
    }
    else {
      Serial.println ("Failed to set Pilot Wire temperature");
    }

    int32_t powerW = readPowerMeter();
    // Update power metering
    if (zbPilot.setPowerW (powerW)) {
      Serial.printf ("Pilot Wire power metering set to %d W\n", powerW);
      // Force report of power metering
      if (!zbPilot.reportPowerW()) {
        Serial.println ("Failed to report Pilot Wire power metering");
      }
    }
    else {
      Serial.println ("Failed to set Pilot Wire power metering");
    }

    uint64_t energyWh = zbPilot.energyWh() + (powerW * (t - lastUpdate)) / 3600000; // Wh = W * h
    // Update energy summation
    if (zbPilot.setEnergyWh (energyWh)) {
      Serial.printf ("Pilot Wire energy summation set to %llu Wh\n", energyWh);
      // Force report of energy summation
      if (!zbPilot.reportEnergyWh()) {
        Serial.println ("Failed to report Pilot Wire energy summation");
      }
    }
    else {
      Serial.println ("Failed to set Pilot Wire energy summation");
    }

    lastUpdate = t;
  }
  delay (100);
}
