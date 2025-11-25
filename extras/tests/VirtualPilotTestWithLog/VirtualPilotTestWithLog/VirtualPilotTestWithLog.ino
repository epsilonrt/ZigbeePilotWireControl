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

  This example also simulates 
  1. a temperature sensor using the thermometer cluster.
    The temperature value is updated every minute, cycling between 18.0 and
    24.0 degrees Celsius in steps of 0.1 degree.  
  2. a power meter using the metering cluster.
    The instantaneous demand (power in W) and summation delivered (energy in Wh)
    attributes are updated every minute. The power value cycles between
    1kW and 2kW in steps of 0.1kW. The energy value is calculated based on
    the power and time elapsed.

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

// Temperature values for simulate thermometer cluster
constexpr float TempMin = 18.0;
constexpr float TempMax = 24.0;
constexpr float TempStep = 0.1;
constexpr float TempDefault = (TempMin + TempMax) / 2.0;

constexpr int32_t PowerMin = 1000; // 1kW
constexpr int32_t PowerMax = 2000; // 5kW
constexpr int32_t PowerStep = 100; // 0.1kW
constexpr int32_t PowerDefault = (PowerMin + PowerMax) / 2;

constexpr unsigned UpdateIntervalMs = 60000; // 60 seconds

float temperature = TempDefault;
float tempStep = TempStep;
float powerStep = PowerStep;
unsigned long lastTempUpdate = 0;

// Create ZigbeePilotWireControl instance with temperature measurement and metering enabled
ZigbeePilotWireControl  zbPilot (ZbeeEndPoint, 5.0f, 35.0f, 1); // temp min 5C, temp max 35C, metering enabled

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

  Serial.begin (115200);

  // wait for serial port to connect, for debugging purposes
  // comment this out if not needed
#if CORE_DEBUG_LEVEL >= 5
  while (!Serial) {
    ledBlink (CRGB::Blue, 50, 50);
  }
#endif
  delay (2000);

  // Init button for factory reset
  pinMode (button, INPUT_PULLUP);

  log_i ("Zigbee Virtual Pilot Wire Control starting...");

  // Set callback function for pilot wire mode change and power state change
  zbPilot.onPilotWireModeChange (setPilotWire);

  if (zbPilot.begin (temperature, PowerDefault) == false) {
    while (1) {
      ledBlink (CRGB::Red, 50, 50);
    }
  }

  zbPilot.enableNvs (true); // restore mode from NVS

  zbPilot.printClusterInfo();

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

    ledBlink (CRGB::Green, 100, 100);
  }

  lastTempUpdate = millis(); // Initialize last temperature and power update time

  // Report initial attributes
  if (zbPilot.reportAttributes()) {

    log_i ("Pilot Wire attributes reported");
  }
  else {

    log_w ("Failed to report Pilot Wire attributes");
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

  // Update temperature and metering every UpdateIntervalMs milliseconds
  unsigned long now = millis();
  unsigned long delta = now - lastTempUpdate;

  if (delta >= UpdateIntervalMs) {

    // The time is up, update temperature and metering
    temperature += tempStep; // increment temperature, if tempStep is negative, temperature will decrease

    if (temperature >= TempMax || temperature <= TempMin) {
      // Reverse direction when reaching limits
      tempStep = -tempStep;
    }

    if (zbPilot.setTemperature (temperature)) {

      // The temperature was set successfully
      log_i ("Pilot Wire temperature set to %.1f C", temperature);

      if (zbPilot.reportTemperature() == false) { // Force report of temperature

        // Failed to report temperature
        log_w ("Failed to report Pilot Wire temperature");
      }
    }
    else {

      // Failed to set temperature
      log_w ("Failed to set Pilot Wire temperature");
    }

    float w = zbPilot.powerW(); // get current power in W
    float wh  = static_cast<float> (zbPilot.energyWh()) + ( (w * delta) / 3600000.0f); // Wh = W * h
    log_i ("Pilot Wire temperature reported, updating metering: Power=%.1f W, Energy=%.1f Wh", w, wh);

    w += powerStep; // increment power, if powerStep is negative, power will decrease
    if (w >= PowerMax || w <= PowerMin) {
      // Reverse direction when reaching limits
      powerStep = -powerStep;
    }

    if (zbPilot.setPowerW (static_cast<int32_t> (w))) { // set new power value

      log_i ("Pilot Wire instantaneous demand set to %d W", zbPilot.powerW());
      if (!zbPilot.reportPowerW()) { // Force report of power

        log_w ("Failed to report Pilot Wire instantaneous demand");
      }
    }
    else {

      log_w ("Failed to set Pilot Wire instantaneous demand");
    }

    if (zbPilot.setEnergyWh (static_cast<uint64_t> (wh + 0.5f))) { // set new energy value with rounding

      log_i ("Pilot Wire summation delivered set to %llu Wh", zbPilot.energyWh());
      if (!zbPilot.reportEnergyWh()) { // Force report of energy

        log_w ("Failed to report Pilot Wire summation delivered");
      }
    }
    else {

      log_w ("Failed to set Pilot Wire summation delivered");
    }

    lastTempUpdate = now; // Update last update time
  }

  delay (100);
}
