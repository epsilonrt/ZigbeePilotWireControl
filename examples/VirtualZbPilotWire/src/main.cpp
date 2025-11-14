// SPDX-Identifier: BSD-3-Clause
// SPDX-FileCopyrightText: 2025 Pascal JEAN aka epsilonrt

#ifndef ZIGBEE_MODE_ZCZR
#error "Zigbee coordinator mode is not selected in Tools->Zigbee mode"
#endif

#include <Arduino.h>
#include <FastLED.h>

#include <Zigbee.h>
#include <ZigbeePilotWireControl.h>
#include <memory>

#define ZIGBEE_PILOTWIRE_CONTROL_ENDPOINT 1

uint8_t button = BOOT_PIN;
ZigbeePilotWireControl  zbPilot (ZIGBEE_PILOTWIRE_CONTROL_ENDPOINT);
const ZigbeePilotWireMode InitialMode = PILOTWIRE_MODE_OFF;

// WS2812B RGB LED that is used on the board to indicate various states
// Off: black
// Frost: Cyan
// Eco: Green
// Comfort -2: Navy
// Comfort -1: Pink
// Comfort: Red
CRGB led;

void setLed (CRGB color) {

  led = color;
  FastLED.show();
}

// Callback function to handle pilot wire mode changes
void setPilotWire (ZigbeePilotWireMode mode) {
  switch (mode) {
    case PILOTWIRE_MODE_OFF:
      setLed (CRGB::Black);
      break;
    case PILOTWIRE_MODE_COMFORT:
      setLed (CRGB::Red);
      break;
    case PILOTWIRE_MODE_ECO:
      setLed (CRGB::Green);
      break;
    case PILOTWIRE_MODE_FROST_PROTECTION:
      setLed (CRGB::Cyan);
      break;
    case PILOTWIRE_MODE_COMFORT_MINUS_1:
      setLed (CRGB::Pink);
      break;
    case PILOTWIRE_MODE_COMFORT_MINUS_2:
      setLed (CRGB::Navy);
      break;
    default:
      log_e ("Unhandled Pilot Wire mode: %d", mode);
      return;
  }
  FastLED.show();
}


void ledBlink (CRGB::HTMLColorCode color, unsigned onTimeMs = 100, unsigned offTimeMs = 100) {
  setLed (color);
  delay (onTimeMs);
  setLed (CRGB::Black);
  delay (offTimeMs);
}

void setup() {
  // Init RGB LED
  FastLED.addLeds<WS2812B, PIN_RGB_LED, GRB> (&led, 1);
  FastLED.setBrightness (32);

  Serial.begin (115200);

  // wait for serial port to connect, for debugging purposes
  // comment this out if not needed
  while (!Serial) {
    ledBlink (CRGB::Blue, 50, 50);
  }

  // Init button for factory reset
  pinMode (button, INPUT_PULLUP);

  log_i ("Zigbee Virtual Pilot Wire Control starting...");

  zbPilot.begin();
  zbPilot.printClusterInfo();

  // Set manufacturer and model name
  // Home Assistant Zigbee integration uses these values to auto-detect the device type
  // and assign the correct icon and features.
  // If you change these values, you may need to modify the integration settings accordingly
  // in homeassistant/config/zha_quirks/epsilonrt/pilot_wire.py
  zbPilot.setManufacturerAndModel ("EpsilonRT", "ERT-MPZ-01");

  // Set callback function for pilot wire mode change and power state change
  zbPilot.onPilotWireModeChange (setPilotWire);

  // Add endpoint to Zigbee Core
  log_i ("Adding ZigbeePilotWireControl endpoint to Zigbee Core");
  Zigbee.addEndpoint (&zbPilot);

  // When all EPs are registered, start Zigbee in ROUTER mode
  if (!Zigbee.begin (ZIGBEE_ROUTER)) {
    log_e ("Zigbee failed to start! Rebooting...");
    ESP.restart();
  }

  log_i ("Connecting to network");
  while (!Zigbee.connected()) {

    ledBlink (CRGB::Yellow, 100, 100);
  }

  // zbPilot.setPilotWireMode (InitialMode);
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
        log_i ("Resetting Zigbee to factory and rebooting in 1s.");
        delay (1000);
        Zigbee.factoryReset();
      }
    }

    t = millis() - t;
    log_i ("Button pressed for %lu ms", t);
    if (t < 3000) {
      uint8_t mode = zbPilot.pilotWireMode();

      mode = (mode + 1) % PILOTWIRE_MODE_COUNT;
      zbPilot.setPilotWireMode (static_cast<ZigbeePilotWireMode> (mode));
    }
  }
  // zbPilot.checkModePtr();
  // zbPilot.checkPowerStatePtr();
  delay (100);
}
