# VirtualPilotSerial Example

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

To see if the communication with your Zigbee network works, use the Serial monitor and watch for output there. 
You should see output similar to this:

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
