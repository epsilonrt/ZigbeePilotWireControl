# ZigbeePilotWireControl
An Arduino library for controlling pilot wire electric heaters via Zigbee with ESP32-H2 or ESP32-C6.

[![Build](https://github.com/epsilonrt/ZigbeePilotWireControl/actions/workflows/build.yml/badge.svg)](https://github.com/epsilonrt/ZigbeePilotWireControl/actions/workflows/build.yml)
[![GitHub release (latest by date including pre-releases)](https://img.shields.io/github/v/release/epsilonrt/ZigbeePilotWireControl?include_prereleases)](https://github.com/epsilonrt/ZigbeePilotWireControl/releases)
[![PlatformIO Registry](https://badges.registry.platformio.org/packages/epsilonrt/library/ZigbeePilotWireControl.svg)](https://registry.platformio.org/libraries/epsilonrt/ZigbeePilotWireControl)
[![Arduino Registry](https://www.ardu-badge.com/badge/ZigbeePilotWireControl.svg)](https://www.arduinolibraries.info/libraries/ZigbeePilotWireControl)  
[![Framework](https://img.shields.io/badge/Framework-Arduino-blue)](https://www.arduino.cc/)
[![Platform ESP32](https://img.shields.io/badge/Platform-Espressif32-orange)](https://www.espressif.com/en/products/socs/esp32)

-------

## Example: Virtual Pilot Wire Control Device

The following example creates a virtual Zigbee Pilot Wire Control device using the `ZigbeePilotWireControl` class. It outputs the current pilot wire mode to the serial console whenever it changes.

The controller can be tested using a Zigbee coordinator with support for the Pilot Wire Control cluster, such as [ZHA (Zigbee Home Automation)](https://www.home-assistant.io/integrations/zha/) in Home Assistant.

From Home Assistant with Zigbee integration, you can control the pilot wire mode using the Pilot Wire Control card automtically created for the device with the [quirks](extras/homeassistant/config/zha_quirks/epsilonrt/pilot_wire.py) provided:

![Pilot Wire Control card in Home Assistant](https://raw.githubusercontent.com/epsilonrt/ZigbeePilotWireControl/main/extras/images/ha_lovelace_without_mes.png)


On the serial console, you will see output like this when changing the mode from the controller:

```
Zigbee Virtual Pilot Wire Control starting...
Adding ZigbeePilotWireControl endpoint to Zigbee Core
Connecting to network
Zigbee connected to network.
Pilot Wire Mode: ECO
Pilot Wire attributes reported
Pilot Wire Mode: FROST
Pilot Wire Mode: OFF
Pilot Wire Mode: FROST
Button pressed for 0 ms
Pilot Wire Mode: COMFORT-1
```

The simplified code for this example is as follows:

```cpp
#include <Zigbee.h>
#include <ZigbeePilotWireControl.h>

const uint16_t ZbeeEndPoint = 1;

// Create ZigbeePilotWireControl instance
ZigbeePilotWireControl  zbPilot (ZbeeEndPoint);

// Callback function to handle pilot wire mode changes
// This is a simulation, for real hardware control, implement the necessary 
// to control the pilot wire output accordingly (with opto-triac...)
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
  Serial.println ("Zigbee Virtual Pilot Wire Control starting...");

  // Set callback function for pilot wire mode change and power state change
  zbPilot.onPilotWireModeChange (setPilotWire);

  // Initialize the Pilot Wire Control endpoint
  zbPilot.begin ();

  // Add endpoint to Zigbee Core
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

  // Report initial attributes
  zbPilot.reportAttributes();
}

void loop() {

  delay (100);
}
```

The complete example code can be found in the `examples/VirtualPilotSerial` folder of this repository. Other examples demonstrate using an RGB LED or real pilot wire output control.

## Temperature and Electric Metering

The library also includes support for optional Temperature Measurement and Electrical Measurement clusters. These can be used to report the ambient temperature and power consumption of the heater. See the `examples/VirtualPilotWithTempAndMeter` example for a demonstration of these features.

![Pilot Wire Control in Home Assistant with measurements](https://raw.githubusercontent.com/epsilonrt/ZigbeePilotWireControl/main/extras/images/ha_lovelace_full.png)