/// @file ZigbeePilotWireControl.h
/// SPDX-License-Identifier: BSD-3-Clause
/// SPDX-FileCopyrightText: 2025 Pascal JEAN aka epsilonrt
#pragma once

#include <soc/soc_caps.h>
#include <sdkconfig.h>

#if ! CONFIG_ZB_ENABLED
#error "This library requires Zigbee support to be enabled in sdkconfig"
#endif  // CONFIG_ZB_ENABLED

#include <ZigbeeEP.h>
#include <ha/esp_zigbee_ha_standard.h>
#include <Preferences.h>

/**
   @brief Manufacturer name for the Pilot Wire Control device.
   This is used by Home Assistant Zigbee integration for device identification.
   If you change this value, ensure to update the corresponding quirk in Home Assistant.
   The quirk is located at:
   extras/homeassistant/config/zha_quirks/epsilonrt/pilot_wire.py
*/
#ifndef PILOT_WIRE_MANUF_NAME
#define PILOT_WIRE_MANUF_NAME   "EpsilonRT"
#endif

/**
   @brief Model name for the Pilot Wire Control device.
   This is used by Home Assistant Zigbee integration for device identification.
   If you change this value, ensure to update the corresponding quirk in Home Assistant.
   The quirk is located at:
   extras/homeassistant/config/zha_quirks/epsilonrt/pilot_wire.py
*/
#ifndef PILOT_WIRE_MODEL_NAME
#define PILOT_WIRE_MODEL_NAME   "ERT-MPZ-03"
#endif

/**
   @brief Manufacturer code for the Pilot Wire Control device.
   This is used by Home Assistant Zigbee integration for device identification.
   Can be replaced with official code if available but should not work, so do not touch!!
   If you change this value, ensure to update the corresponding quirk in Home Assistant.
   The quirk is located at:
   extras/homeassistant/config/zha_quirks/epsilonrt/pilot_wire.py
*/
#ifndef PILOT_WIRE_MANUF_CODE
#define PILOT_WIRE_MANUF_CODE   0x1234
#endif

/**
   @brief Manufacturer-specific cluster ID for the Pilot Wire Control device.
   Must be equal to or greater than 0xFC00 for manufacturer-specific clusters.
*/
#define PILOT_WIRE_CLUSTER_ID   0xFC00

/**
   @brief Manufacturer-specific attribute ID for the Pilot Wire mode.
*/
#define PILOT_WIRE_MODE_ATTR_ID 0x0000

/**
   @brief Enum representing the different Pilot Wire modes.
   These modes correspond to standard pilot wire control modes for electric heaters.
*/
enum ZigbeePilotWireMode : uint8_t {
  PILOTWIRE_MODE_OFF = 0, ///< Heater Off
  PILOTWIRE_MODE_COMFORT, ///< Comfort Mode
  PILOTWIRE_MODE_ECO, ///< Eco Mode
  PILOTWIRE_MODE_FROST_PROTECTION, ///< Frost Protection Mode
  PILOTWIRE_MODE_COMFORT_MINUS_1, ///< Comfort Minus 1 Mode
  PILOTWIRE_MODE_COMFORT_MINUS_2 ///< Comfort Minus 2 Mode
};

/**
 * @brief Minimum and maximum values for ZigbeePilotWireMode enum.
 * These constants can be used for validation or iteration over the enum values.
 */
const ZigbeePilotWireMode PILOTWIRE_MODE_MIN = PILOTWIRE_MODE_OFF;

/**
 * @brief Maximum value for ZigbeePilotWireMode enum.
 * This constant can be used for validation or iteration over the enum values.
 */
const ZigbeePilotWireMode PILOTWIRE_MODE_MAX = PILOTWIRE_MODE_COMFORT_MINUS_2;

/**
 * @brief Total number of modes defined in ZigbeePilotWireMode enum.
 * This constant can be used for validation or iteration over the enum values.
 */
const uint8_t PILOTWIRE_MODE_COUNT = (PILOTWIRE_MODE_COMFORT_MINUS_2 - PILOTWIRE_MODE_OFF + 1);

/**
   @brief Class representing a Zigbee Pilot Wire Control endpoint.
   This class extends the ZigbeeEP class to implement a custom cluster
   for controlling pilot-wire electric heaters via Zigbee.
*/
class ZigbeePilotWireControl : public ZigbeeEP {
  public:
    /**
       @brief Constructor for ZigbeePilotWireControl.

       You must call begin() after constructing the object to initialize the endpoint.
       @param endpoint The Zigbee endpoint number to use for this device.
    */
    ZigbeePilotWireControl (uint8_t endpoint);

    /**
       @brief Set a callback function to be called when the Pilot Wire mode changes.
       This function must be called to register a callback that will be invoked before
       to call begin().
       @param callback A function pointer to the callback function.
       The callback function should have the following signature:
       void callback(ZigbeePilotWireMode mode);
       The mode parameter will contain the new Pilot Wire mode.
       This function is used to notify the application of mode changes.
       @warning This is the only place where the mode change, after the zigbee network is
       established, the user must use setPilotWireMode() to change mode.
    */
    void onPilotWireModeChange (void (*callback) (ZigbeePilotWireMode mode)) {
      _on_mode_change = callback;
    }

    /**
       @brief Initialize the Zigbee Pilot Wire Control endpoint.
       This method sets up the custom Pilot Wire cluster and its attributes,
       and adds the endpoint to the Zigbee stack.
       @param enableMetering If true, adds a metering cluster to the endpoint.
       @param enableTemperature If true, adds a temperature measurement cluster to the endpoint.
    */
    void begin (bool enableMetering = false, bool enableTemperature = false);

    /**
       @brief Get the current Pilot Wire mode.
       @return The current Pilot Wire mode as a ZigbeePilotWireMode enum value.
    */
    ZigbeePilotWireMode pilotWireMode() const {
      return  static_cast<ZigbeePilotWireMode> (_current_mode);
    }

    /**
       @brief Get the current power state.
       @return true if the power is ON, false if OFF. true if mode is PILOTWIRE_MODE_OFF, false otherwise.
    */
    bool powerState() const {
      return _current_state;
    }

    /**
       @brief Set the Pilot Wire mode.
       This method updates the Pilot Wire mode attribute and notifies the application
       of the mode change via the registered callback function.
       @param mode The new Pilot Wire mode to set.
       @return true if the mode was set successfully, false otherwise.
       @note this method will invoke the callback function registered via onPilotWireModeChange()
       and report the attribute change to the Zigbee network with reportAttributes().
       So, this is not necessarily to call reportAttributes() after calling this method.
    */
    bool setPilotWireMode (ZigbeePilotWireMode mode);

    /**
       @brief Report the current Pilot Wire attributes to the Zigbee network.
       This method updates the Pilot Wire mode and On/Off attributes in the Zigbee stack.
       @return true if the attributes were reported successfully, false otherwise.
    */
    bool reportAttributes();

    /**
       @brief Enable or disable restore mode.
       When restore mode is enabled, the Pilot Wire mode is restored from NVS on startup.
       @param enable true to enable restore mode, false to disable.
       @note This setting is persisted in NVS.
    */
    void enableRestoreMode (bool enable) {
      _restore_mode = enable;
      _prefs.putBool ("restore", enable);
    }

    /**
       @brief Check if restore mode is enabled.
       @return true if restore mode is enabled, false otherwise.
    */
    bool isRestoreModeEnabled() const {
      return _restore_mode;
    }

    /**
       @brief Destructor for ZigbeePilotWireControl.
       Cleans up resources and ends NVS preferences.
    */
    ~ZigbeePilotWireControl() {
      end();
    }

    /**
       @brief End the ZigbeePilotWireControl and clean up resources.
       This method should be called to properly release resources used by the ZigbeePilotWireControl instance.
    */
    void end() {
      _prefs.end();
    }

    /**
       @brief Print the cluster information of the ZigbeePilotWireControl endpoint.
       This method outputs the cluster details to the specified Print object for debugging purposes.
       @param out The Print object to output the cluster information to. Defaults to Serial.
    */
    void printClusterInfo (Print &out = Serial);

  protected:
    void zbAttributeSet (const esp_zb_zcl_set_attr_value_message_t *message) override;

  private:
    void pilotWireModeChanged();

    uint8_t _current_mode;
    uint8_t _state_on_mode;
    void (*_on_mode_change) (ZigbeePilotWireMode mode);

    bool _current_state;
    bool _current_state_changed;
    bool _restore_mode;
    Preferences _prefs;
};

