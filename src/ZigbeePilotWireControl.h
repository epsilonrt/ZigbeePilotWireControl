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
   @brief Minimum and maximum values for ZigbeePilotWireMode enum.
   These constants can be used for validation or iteration over the enum values.
*/
const ZigbeePilotWireMode PILOTWIRE_MODE_MIN = PILOTWIRE_MODE_OFF;

/**
   @brief Maximum value for ZigbeePilotWireMode enum.
   This constant can be used for validation or iteration over the enum values.
*/
const ZigbeePilotWireMode PILOTWIRE_MODE_MAX = PILOTWIRE_MODE_COMFORT_MINUS_2;

/**
   @brief Total number of modes defined in ZigbeePilotWireMode enum.
   This constant can be used for validation or iteration over the enum values.
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

       Temperature measurement and metering clusters are disabled.
       You must call begin() after constructing the object to initialize the endpoint.
       @param endpoint The Zigbee endpoint number to use for this device.
    */
    ZigbeePilotWireControl (uint8_t endpoint);

    /**
       @brief Constructor for ZigbeePilotWireControl with temperature measurement.
       This constructor enables the temperature measurement cluster.
       The metering cluster is disabled.
       You must call begin(float currentTemperature) after constructing the object to initialize the endpoint.
       @param endpoint The Zigbee endpoint number to use for this device.
       @param tempMin The minimum temperature value for the temperature measurement cluster in degrees Celsius.
       @param tempMax The maximum temperature value for the temperature measurement cluster in degrees Celsius.
    */
    ZigbeePilotWireControl (uint8_t endpoint, float tempMin, float tempMax);

    /**
       @brief Constructor for ZigbeePilotWireControl with metering.
       This constructor enables the metering cluster.
       The temperature measurement cluster is disabled.
       You must call begin() after constructing the object to initialize the endpoint.
       @param endpoint The Zigbee endpoint number to use for this device.
       @param meteringMultiplier The multiplier value for the metering cluster, must be non-zero to enable metering.
        This value must be updated whis begin(uint32_t meteringMultiplier).
    */
    ZigbeePilotWireControl (uint8_t endpoint, uint32_t meteringMultiplier);

    /**
        @brief Constructor for ZigbeePilotWireControl with temperature measurement and metering.
       This constructor enables both the temperature measurement and metering clusters.
       You must call begin(float currentTemperature) after constructing the object to initialize the endpoint.
       @param endpoint The Zigbee endpoint number to use for this device.
       @param tempMin The minimum temperature value for the temperature measurement cluster in degrees Celsius.
       @param tempMax The maximum temperature value for the temperature measurement cluster in degrees Celsius.
       @param meteringMultiplier The multiplier value for the metering cluster, must be non-zero to enable metering.
        This value must be updated whis begin(float currentTemperature, uint32_t meteringMultiplier).
    */
    ZigbeePilotWireControl (uint8_t endpoint, float tempMin, float tempMax, uint32_t meteringMultiplier);

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
       @brief Initialize the ZigbeePilotWireControl endpoint and create clusters.
       This method sets up the necessary clusters for Pilot Wire Control,
       including On/Off, Pilot Wire mode and adds the endpoint to the Zigbee stack.
       @return true if the initialization was successful, false otherwise.
       @note This method must be called before to add the endpoint to the Zigbee core.
    */
    bool begin ();

    /**
       @brief Initialize the ZigbeePilotWireControl endpoint with temperature measurement.
       This method sets up the necessary clusters for Pilot Wire Control,
       including On/Off, Pilot Wire mode, Temperature Measurement and adds the endpoint to the Zigbee stack.
       @param currentTemperature The initial temperature value for the temperature measurement cluster in degrees Celsius.
       @return true if the initialization was successful, false otherwise.
       @note This method must be called before to add the endpoint to the Zigbee core.
    */
    bool begin (float currentTemperature);

    /**
       @brief Initialize the ZigbeePilotWireControl endpoint with metering.
       This method sets up the necessary clusters for Pilot Wire Control,
       including On/Off, Pilot Wire mode, Metering and adds the endpoint to the Zigbee stack.
       @param currentPower The initial instantaneous power demand value for the metering cluster in watts (W).
       @param meteringMultiplier The multiplier value for the metering cluster, this value was set in the constructor.
        If meteringMultiplier is zero, no change is made to the value set in the constructor.
       @return true if the initialization was successful, false otherwise.
       @note This method must be called before to add the endpoint to the Zigbee core.
    */
    bool begin (int32_t currentPower, uint32_t meteringMultiplier = 0);

    /**
       @brief Initialize the ZigbeePilotWireControl endpoint with temperature measurement and metering.
       This method sets up the necessary clusters for Pilot Wire Control,
       including On/Off, Pilot Wire mode, Temperature Measurement, Metering
       and adds the endpoint to the Zigbee stack.
       @param currentTemperature The initial temperature value for the temperature measurement cluster in degrees Celsius.
       @param currentPower The initial instantaneous power demand value for the metering cluster in watts (W).
       @param meteringMultiplier The multiplier value for the metering cluster, this value was set in the constructor.
        If meteringMultiplier is zero, no change is made to the value set in the constructor.
       @return true if the initialization was successful, false otherwise.
       @note This method must be called before to add the endpoint to the Zigbee core.
    */
    bool begin (float currentTemperature, int32_t currentPower, uint32_t meteringMultiplier = 0);

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
        @brief Report the current Pilot Wire mode and On/Off attributes to the Zigbee network.
        This method call the callback to notify the application of the current mode, and
        updates the Pilot Wire mode and On/Off attributes in the Zigbee stack.
    */
    bool reportPilotModeAndOnOff ();

    /**
       @brief Set the temperature value for the temperature measurement cluster.
       @param temperature The temperature value in degrees Celsius, the resolution is 0.01 degree.
       @return true if the temperature was set successfully, false otherwise.
    */
    bool setTemperature (float value);

    /**
       @brief Get the current temperature value.
       @return The current temperature value in degrees Celsius.
    */
    float temperature() const {
      return _temperature_value;
    }

    /**
       @brief Get the minimum temperature value.
       @return The minimum temperature value in degrees Celsius. NAN if temperature measurement cluster is not enabled.
    */
    float temperatureMin() const;

    /**
       @brief Get the maximum temperature value.
       @return The maximum temperature value in degrees Celsius. NAN if temperature measurement cluster is not enabled.
    */
    float temperatureMax() const;

    /**
       @brief Set the reporting interval for the temperature measurement cluster.
       @param min_interval The minimum reporting interval in seconds. This is the shortest time between reports even if the temperature changes.
       @param max_interval The maximum reporting interval in seconds. This is the longest time between reports even if the temperature does not change.
       @param delta The temperature change delta in degrees Celsius, the resolution is 0.01 degree. This is the minimum change in temperature that triggers a report.
       @return true if the reporting interval was set successfully, false otherwise.
    */
    bool setTemperatureReporting (uint16_t min_interval, uint16_t max_interval, float delta);

    /**
       @brief Report the current temperature value to the Zigbee network.
       The reporting is configured via setTemperatureReporting(), so this method
       can be used to force a report outside of the configured intervals.
       @return true if the temperature was reported successfully, false otherwise.
    */
    bool reportTemperature();

    /**
       @brief Set the summation delivered attribute in the metering cluster.
       @param summation_wh The total energy delivered in watt-hours (Wh). uint48_t value.
              if isNvsEnabled() is true, this value is stored in NVS and restored on startup.
       @return true if the attribute was set successfully, false otherwise.
    */
    bool setEnergyWh (uint64_t summation_wh);

    /**
       @brief Get the current summation delivered value.
       @return The current summation delivered value in watt-hours (Wh).
    */
    uint64_t energyWh() const;

    /**
       @brief Report the current summation delivered value to the Zigbee network.
       The reporting is configured via setEnergyWhReporting(), so this method
       can be used to force a report outside of the configured intervals.
       @return true if the energy was reported successfully, false otherwise.
    */
    bool reportEnergyWh();

    /**
       @brief Set the reporting interval for the summation delivered attribute in the metering cluster.
       @param min_interval The minimum reporting interval in seconds. This is the shortest time between reports even if the energy changes.
       @param max_interval The maximum reporting interval in seconds. This is the longest time between reports even if the energy does not change.
       @param delta The energy change delta in watt-hours (Wh). This is the minimum change in energy that triggers a report.
       @return true if the reporting interval was set successfully, false otherwise.
    */
    bool setEnergyWhReporting (uint16_t min_interval, uint16_t max_interval, float delta);

    /**
       @brief Set the electric power attribute in the metering cluster.
       @param demand_w The instantaneous power demand in watts (W). uint24_t value representing -8388608 to 8388607 W.
       @return true if the attribute was set successfully, false otherwise.
    */
    bool setPowerW (int32_t demand_w);

    /**
       @brief Get the current electric power value.
       @return The current electric power value in watts (W).
    */
    int32_t powerW() const;

    /**
       @brief Report the current electric power value to the Zigbee network.
       The reporting is configured via setPowerWReporting(), so this method
       can be used to force a report outside of the configured intervals.
       @return true if the power was reported successfully, false otherwise.
    */
    bool reportPowerW();

    /**
       @brief Set the reporting interval for the electric power attribute in the metering cluster.
       @param min_interval The minimum reporting interval in seconds. This is the shortest time between reports even if the power changes.
       @param max_interval The maximum reporting interval in seconds. This is the longest time between reports even if the power does not change.
       @param delta The power change delta in watts (W). This is the minimum change in power that triggers a report.
       @return true if the reporting interval was set successfully, false otherwise.
    */
    bool setPowerWReporting (uint16_t min_interval, uint16_t max_interval, float delta);

    /**
       @brief Set the metering status attribute in the metering cluster.
       @param status The metering status (bitmap U8 in ZCL).
        bit 6 Service Disconnect Open: Set to true when the service have been disconnected to this premises.
        bit 5 Leak Detect: Set to true when a leak has been detected.
        bit 4 Power Quality: Set to true if a power quality event has been detected such as a low voltage, high voltage.
        bit 3 Power Failure: Set to true during a power outage.
        bit 2 Tamper Detect: Set to true if a tamper event has been detected.
        bit 1 Low Battery: Set to true when the battery needs maintenance.
        bit 0 Check Meter: Set to true when a non fatal problem has been detected on the meter such as a measurement
                           error, memory error, and self check error.
       @return true if the attribute was set successfully, false otherwise.
    */
    bool setMeteringStatus (uint8_t status);

    /**
       @brief Get the current metering status value.
       @return The current metering status (bitmap U8 in ZCL).
    */
    uint8_t meteringStatus() const {
      return _metering_cfg.status;
    }

    /**
       @brief Report the current attributes to the Zigbee network.
       This method updates the Pilot Wire mode, On/Off, temperature (if enabled)
       and metering (if enabled) attributes in the Zigbee stack.
       @return true if the attributes were reported successfully, false otherwise.
    */
    bool reportAttributes();

    /**
       @brief Enable or disable restore mode.
       When restore mode is enabled, the Pilot Wire mode is restored from NVS on startup.
       @param enable true to enable restore mode, false to disable.
       @note This setting is persisted in NVS.
    */
    void enableNvs (bool enable) {
      _nvs_enabled = enable;
      _prefs.putBool ("restore", enable);
    }

    /**
       @brief Check if restore mode is enabled.
       @return true if restore mode is enabled, false otherwise.
    */
    bool isNvsEnabled() const {
      return _nvs_enabled;
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
    bool setReporting (uint16_t cluster_id, uint16_t attr_id,
                       uint16_t min_interval, uint16_t max_interval, float delta,
                       uint16_t manuf_code = ESP_ZB_ZCL_ATTR_NON_MANUFACTURER_SPECIFIC);
    bool reportAttribute (uint16_t cluster_id, uint16_t attr_id, uint16_t manuf_code = ESP_ZB_ZCL_ATTR_NON_MANUFACTURER_SPECIFIC);
    bool createPilotWireCluster();
    bool createTemperatureMeasurementCluster (float currentTemperature);
    bool createMeteringCluster (int32_t currentPower, uint32_t meteringMultiplier);

  private:
    void pilotWireModeChanged();

    uint8_t _current_mode;
    uint8_t _state_on_mode;
    void (*_on_mode_change) (ZigbeePilotWireMode mode);

    bool _current_state;
    bool _current_state_changed;
    bool _nvs_enabled;
    Preferences _prefs;

    bool _temperature_enabled;
    esp_zb_temperature_meas_cluster_cfg_t _temperature_cfg;
    float _temperature_value;

    // Member variables for Simple Metering cluster (0x0702)
    bool _metering_enabled;
    esp_zb_metering_cluster_cfg_t _metering_cfg;
    esp_zb_uint48_t _summationDelivered;
    esp_zb_uint24_t _multiplier;
    esp_zb_uint24_t _divisor;
    esp_zb_int24_t _instantaneousDemand;
};

