/*
  SPDX-License-Identifier: BSD-3-Clause
  SPDX-FileCopyrightText: 2025 Pascal JEAN aka epsilonrt

  This example creates a virtual Zigbee Pilot Wire Control device
  using the ZigbeePilotWireControl class and a RGB LED to indicate the mode:
  - Off: Black (LED off)
  - Frost Protection: Cyan
  - Eco: Green
  - Comfort -2: Navy
  - Comfort -1: Magenta
  - Comfort: Red
  .

  The RGB LED blinks green while connecting to the Zigbee network.
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
#include <FastLED.h>

const uint16_t ZbeeEndPoint = 1;
const uint8_t button = BOOT_PIN;

// Create ZigbeePilotWireControl instance
ZigbeePilotWireControl  zbPilot (ZbeeEndPoint);

// WS2812B RGB LED that is used on the board to indicate various states
// Off: black
// Frost: Cyan
// Eco: Green
// Comfort -2: Navy
// Comfort -1: Magenta
// Comfort: Red
CRGB led;

// Function to set the RGB LED color
void
setLed (CRGB color) {

  led = color;
  FastLED.show();
}

// Callback function to handle pilot wire mode changes
void
setPilotWire (ZigbeePilotWireMode mode) {
  static const CRGB modeColors[PILOTWIRE_MODE_COUNT] = {
    CRGB::Black,   // PILOTWIRE_MODE_OFF
    CRGB::Red,     // PILOTWIRE_MODE_COMFORT
    CRGB::Green,   // PILOTWIRE_MODE_ECO
    CRGB::Cyan,    // PILOTWIRE_MODE_FROST_PROTECTION
    CRGB::Magenta, // PILOTWIRE_MODE_COMFORT_MINUS_1
    CRGB::Navy     // PILOTWIRE_MODE_COMFORT_MINUS_2
  };

  setLed (modeColors[mode]);
}

// Function to blink the RGB LED with specified color and timings and count
void
ledBlink (CRGB::HTMLColorCode color, unsigned onTimeMs = 100, unsigned offTimeMs = 100, unsigned count = 1) {
  for (unsigned i = 0; i < count; i++) {
    setLed (color);
    delay (onTimeMs);
    setLed (CRGB::Black);
    delay (offTimeMs);
  }
}

void setup() {

  // Init RGB LED
  FastLED.addLeds<WS2812B, PIN_RGB_LED, GRB> (&led, 1);
  FastLED.setBrightness (32);

  // Init button for factory reset
  pinMode (button, INPUT_PULLUP);

  // Set callback function for pilot wire mode change and power state change
  zbPilot.onPilotWireModeChange (setPilotWire);

  zbPilot.begin ();
  zbPilot.enableNvs (true); // restore mode from NVS

  // Add endpoint to Zigbee Core
  Zigbee.addEndpoint (&zbPilot);

  // When all EPs are registered, start Zigbee in ROUTER mode
  if (!Zigbee.begin (ZIGBEE_ROUTER)) {
    ledBlink (CRGB::Red, 20, 50, 10); // fast red blink on error
    ESP.restart();
  }

  while (!Zigbee.connected()) {

    ledBlink (CRGB::Green, 100, 100);
  }

  if (!zbPilot.reportAttributes()) {

    ledBlink (CRGB::Pink, 20, 50, 10); // fast pink blink on error
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
        ledBlink (CRGB::Cyan, 200, 200, 5);
        Zigbee.factoryReset();
      }
    }

    t = millis() - t;
    if (t < 3000) {
      uint8_t mode = zbPilot.pilotWireMode();

      mode = (mode + 1) % PILOTWIRE_MODE_COUNT;
      zbPilot.setPilotWireMode (static_cast<ZigbeePilotWireMode> (mode));
    }
  }
  delay (100);
}
