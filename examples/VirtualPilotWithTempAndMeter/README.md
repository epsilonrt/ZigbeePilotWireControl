# VirtualPilotWithTempAndMeter Example

This example shows how to configure the Zigbee end device and use it as a Home Automation (HA) pilot wire control device.

It creates a virtual Zigbee Pilot Wire Control device using the ZigbeePilotWireControl class with a serial output to indicate the mode:  
- **Off**: The heater is off. Serial output: "Pilot Wire Mode: OFF"
- **Frost**:  The heater is in frost protection mode. Serial output: "Pilot Wire Mode: FROST"
- **Eco**: The heater is in eco mode. Serial output: "Pilot Wire Mode: ECO"  
- **Comfort-2**: The heater is in comfort-2 mode. Serial output: "Pilot Wire Mode: COMFORT -2"  
- **Comfort-1**: The heater is in comfort-1 mode. Serial output: "Pilot Wire Mode: COMFORT -1"  
- **Comfort**: The heater is in comfort mode. Serial output: "Pilot Wire Mode: COMFORT"  

A button is used to cycle through the modes. If the button is held for more than 3 seconds, the Zigbee stack is reset
to factory defaults.  
The current mode is saved in NVS if restore mode is enabled, and restored on startup.


This example also simulates:  
1. a temperature sensor using the a ESP32-C6 built-in sensor used to measure the chip's internal temperature. 
 The temperature value is updated every minute and reported via the thermometer cluster (0x0402).
2. a power meter using a potentiometer as analog input to simulate power measurement via
 the ADC and the metering cluster. An analog voltage between 0 and 3.3V is mapped to a power value
 between 0 and 5000W. The instantaneous demand (power in W) and summation delivered (energy in Wh)
 attributes are updated every minute via the metering cluster (0x0702).

To see if the communication with your Zigbee network works, use the Serial monitor and watch for output there. 
You should see output similar to this:

```
21:31:37:900 -> Zigbee Virtual Pilot Wire Control starting...
21:31:37:901 -> New temperature sensor initialized
21:31:37:901 -> Initial temperature: 40.00 C, Initial power meter: 2039 W
21:31:37:903 -> Adding ZigbeePilotWireControl endpoint to Zigbee Core
21:31:37:972 -> Connecting to network
21:31:37:972 -> Zigbee connected to network.
21:31:37:972 -> Pilot Wire Mode: FROST
21:31:37:974 -> Pilot Wire attributes reported
21:32:35:922 -> Pilot Wire temperature set to 43.00 C
21:32:35:924 -> Pilot Wire power metering set to 1742 W
21:32:35:931 -> Pilot Wire energy summation set to 68488 Wh
21:32:41:136 -> Pilot Wire Mode: COMFORT-1
21:33:31:524 -> Button pressed for 50 ms
21:33:31:527 -> Pilot Wire Mode: COMFORT-2
```

And you can see the device in your Home Assistant  interface as a "Zigbee Pilot Wire Control" device:

![Zigbee Pilot Wire Control in Home Assistant](https://raw.githubusercontent.com/epsilonrt/ZigbeePilotWireControl/main/extras/images/ha_lovelace_virtual_pilot_with_temp_and_meter.png)

# Supported Targets

Currently, this example supports the following targets.

| Supported Targets | ESP32-C6 | ESP32-H2 |
| ----------------- | -------- | -------- |

## Hardware Required

* A USB cable for power supply and programming
* Board (ESP32-H2 or ESP32-C6) as Zigbee end device and upload the Zigbee_Window_Covering example
* Zigbee network / coordinator (Other board with switch examples or Zigbee2mqtt or ZigbeeHomeAssistant like application)

### Configure the Project

#### Using Arduino IDE

To get more information about the Espressif boards see [Espressif Development Kits](https://www.espressif.com/en/products/devkits).

* Before Compile/Verify, select the correct board: `Tools -> Board`.
* Select the End device Zigbee mode: `Tools -> Zigbee mode: Zigbee ZCZR (coordinator/router)`.
* Select Tools / USB CDC On Boot: "Enabled"
* Select Partition Scheme for Zigbee: `Tools -> Partition Scheme: Zigbee ZCZR 4MB with spiffs`
* Select the COM port: `Tools -> Port: xxx` where the `xxx` is the detected COM port.
* Optional: Set debug level to verbose to see all logs from Zigbee stack: `Tools -> Core Debug Level: Verbose`.
