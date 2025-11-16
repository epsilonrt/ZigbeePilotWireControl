# VirtualPilotTestWithLog Example

This example shows how to configure the Zigbee end device and use it as a Home Automation (HA) pilot wire control device.

This example creates a virtual Zigbee Pilot Wire Control device using the ZigbeePilotWireControl class and a RGB LED to indicate the mode:  
- **Off**: Black (LED off)  
- **Frost Protection**: Cyan  
- **Eco**: Green  
- **Comfort -2**: Navy  
- **Comfort -1**: Magenta  
- **Comfort**: Red  

The RGB LED blinks green while connecting to the Zigbee network.
A button is used to cycle through the modes. If the button is held for more than 3 seconds, the Zigbee stack is reset
to factory defaults.  
The current mode is saved in NVS if restore mode is enabled, and restored on startup.

Serial output is used in this example to provide detailed logs for debugging purposes.

# Supported Targets

Currently, this example supports the following targets.

| Supported Targets | ESP32-C6 | ESP32-H2 |
| ----------------- | -------- | -------- |

## Hardware Required

* A USB cable for power supply and programming
* Board (ESP32-H2 or ESP32-C6) as Zigbee end device and upload the this example
* Zigbee network / coordinator (Other board with switch examples or Zigbee2mqtt or ZigbeeHomeAssistant like application)

### Configure the Project

#### Using Arduino IDE

* Before Compile/Verify, select the correct board: `Tools -> Board`.
* Select the End device Zigbee mode: `Tools -> Zigbee mode: Zigbee ZCZR (coordinator/router)`.
* Select Tools / USB CDC On Boot: "Enabled"
* Select Partition Scheme for Zigbee: `Tools -> Partition Scheme: Zigbee ZCZR 4MB with spiffs`
* Select the COM port: `Tools -> Port: xxx` where the `xxx` is the detected COM port.
* Optional: Set debug level to verbose to see all logs from Zigbee stack: `Tools -> Core Debug Level: Verbose`.
