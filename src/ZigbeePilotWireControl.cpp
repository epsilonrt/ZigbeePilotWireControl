/// @file ZigbeePilotWireControl.cpp
/// SPDX-License-Identifier: BSD-3-Clause
/// SPDX-FileCopyrightText: 2025 Pascal JEAN aka epsilonrt

#include "ZigbeePilotWireControl.h"

// ----------------------------------------------------------------------------
static int16_t
zb_float_to_s16 (float temp) {

  if (isnan (temp)) {
    return ESP_ZB_ZCL_TEMP_MEASUREMENT_MEASURED_VALUE_DEFAULT; // valeur invalide normalisée ZCL
  }
  return (int16_t) (temp * 100.0f);
}

// ----------------------------------------------------------------------------
static float
zb_s16_to_float (int16_t val) {
  if (val == ESP_ZB_ZCL_TEMP_MEASUREMENT_MEASURED_VALUE_DEFAULT) {
    return NAN;
  }
  return static_cast<float> (val) / 100.0f;
}

// ----------------------------------------------------------------------------
static inline uint64_t esp_zb_uint48_to_u64 (const esp_zb_uint48_t &val) {
  return ( (uint64_t) val.high << 32) | val.low;
}

// ----------------------------------------------------------------------------
static inline esp_zb_uint48_t u64_to_esp_zb_uint48 (uint64_t v) {
  esp_zb_uint48_t out;

  out.low  = (uint32_t) (v & 0xFFFFFFFFULL);
  out.high = (uint16_t) (v >> 32);
  return out;
}

// ----------------------------------------------------------------------------
static inline int32_t esp_zb_sint24_to_i32 (const esp_zb_int24_t &val) {
  int32_t out = ( (int32_t) val.high << 16) | val.low;
  // Sign extend if negative
  if (val.high & 0x80) {
    out |= 0xFF000000;
  }
  return out;
}

// ----------------------------------------------------------------------------
static inline esp_zb_int24_t i32_to_esp_zb_sint24 (int32_t v) {
  esp_zb_int24_t out;

  out.low  = (uint16_t) (v & 0xFFFF);
  out.high = (int8_t) ( (v >> 16) & 0xFF);
  return out;
}

// ----------------------------------------------------------------------------
static inline uint32_t esp_zb_uint24_to_u32 (const esp_zb_uint24_t &val) {
  return ( (uint32_t) val.high << 16) | val.low;
}

// ----------------------------------------------------------------------------
static inline esp_zb_uint24_t u32_to_esp_zb_uint24 (uint32_t v) {
  esp_zb_uint24_t out;

  out.low  = (uint16_t) (v & 0xFFFF);
  out.high = (uint8_t) ( (v >> 16) & 0xFF);
  return out;
}

// ----------------------------------------------------------------------------
ZigbeePilotWireControl::ZigbeePilotWireControl (uint8_t endpoint, float tempMin, float tempMax,
                                                uint32_t meteringMultiplier) :
  ZigbeeEP (endpoint), _current_mode (PILOTWIRE_MODE_OFF),
  _state_on_mode (PILOTWIRE_MODE_COMFORT), _on_mode_change (nullptr),
  _current_state (false), _current_state_changed (true), _nvs_enabled (false),
  _temperature_enabled (isnan (tempMin) == false && isnan (tempMax) == false),
  _temperature_cfg ({
  .measured_value = ESP_ZB_ZCL_TEMP_MEASUREMENT_MEASURED_VALUE_DEFAULT, // Invalid value
  .min_value = zb_float_to_s16 (tempMin),
  .max_value = zb_float_to_s16 (tempMax),
}),
_temperature_value (NAN),
                   _metering_enabled (meteringMultiplier != 0),
                   _summationDelivered (u64_to_esp_zb_uint48 (0)),
                   _instantaneousDemand (i32_to_esp_zb_sint24 (0)),
                   _multiplier (u32_to_esp_zb_uint24 (meteringMultiplier)),
                   _divisor (u32_to_esp_zb_uint24 (1000)),
_metering_cfg ({
  .current_summation_delivered = _summationDelivered, // 0x0000 U48 Current summation delivered Wh
  .status = ESP_ZB_ZCL_METERING_STATUS_DEFAULT_VALUE, // 0x0200 MAP8 Metering status
  .uint_of_measure = ESP_ZB_ZCL_METERING_UNIT_KW_KWH_BINARY,       // 0x0300 MAP8 kWh/kW
  .summation_formatting = ESP_ZB_ZCL_METERING_FORMATTING_SET (false, 7, 3), // 0x0303 MAP8 Summation formatting, 7 digits before decimal, 3 digits after decimal
  .metering_device_type = ESP_ZB_ZCL_METERING_ELECTRIC_METERING    // 0x0306 MAP8 Electric Energy Meter
}) {

  _device_id = ESP_ZB_HA_SMART_PLUG_DEVICE_ID;

  // Configure endpoint
  _ep_config = {
    .endpoint = _endpoint,
    .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
    .app_device_id = ESP_ZB_HA_SMART_PLUG_DEVICE_ID,
    .app_device_version = 0
  };
}

// ----------------------------------------------------------------------------
ZigbeePilotWireControl::ZigbeePilotWireControl (uint8_t endpoint) :
  ZigbeePilotWireControl (endpoint, NAN, NAN, 0) {
}

// ----------------------------------------------------------------------------
ZigbeePilotWireControl::ZigbeePilotWireControl (uint8_t endpoint, float tempMin, float tempMax) :
  ZigbeePilotWireControl (endpoint, tempMin, tempMax, 0) {
}

// ----------------------------------------------------------------------------
ZigbeePilotWireControl::ZigbeePilotWireControl (uint8_t endpoint, uint32_t meteringMultiplier) :
  ZigbeePilotWireControl (endpoint, NAN, NAN, meteringMultiplier) {
}

// ----------------------------------------------------------------------------
// protected methods
bool
ZigbeePilotWireControl::createPilotWireCluster() {
  esp_err_t err;

  if (_nvs_enabled) {

    _current_mode = _prefs.getInt ("mode", PILOTWIRE_MODE_OFF);
    log_i ("Restored mode from NVS: %d", _current_mode);
  }
  else {

    log_i ("Starting with default mode: %d", _current_mode);
  }

  _current_state = (_current_mode != PILOTWIRE_MODE_OFF);
  _current_state_changed = true;

  // Create cluster list
  _cluster_list = esp_zb_zcl_cluster_list_create();
  if (_cluster_list == nullptr) {
    log_e ("Failed to create cluster list for Pilot Wire Control");
    return false;
  }

  // Add basic cluster
  err = esp_zb_cluster_list_add_basic_cluster (_cluster_list,
                                               esp_zb_basic_cluster_create (NULL),
                                               ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  if (err != ESP_OK) {
    log_e ("Failed to add Basic cluster to Pilot Wire Control endpoint");
    return false;
  }

  if (setManufacturerAndModel (PILOT_WIRE_MANUF_NAME, PILOT_WIRE_MODEL_NAME)) {
    log_i ("Manufacturer and Model set for Pilot Wire Control");
  }
  else {
    log_w ("Failed to set Manufacturer and Model for Pilot Wire Control");
  }

  // Add identify cluster
  err = esp_zb_cluster_list_add_identify_cluster (_cluster_list,
                                                  esp_zb_identify_cluster_create (NULL),
                                                  ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  if (err != ESP_OK) {
    log_e ("Failed to add Identify cluster to Pilot Wire Control endpoint");
    return false;
  }

  // Add on/off cluster
  err = esp_zb_cluster_list_add_on_off_cluster (_cluster_list,
                                                esp_zb_on_off_cluster_create (NULL),
                                                ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  if (err != ESP_OK) {
    log_e ("Failed to add On/Off cluster to Pilot Wire Control endpoint");
    return false;
  }

  // Create custom Pilot Wire cluster with manufacturer-specific attribute
  esp_zb_attribute_list_t *pilot_wire_cluster = esp_zb_zcl_attr_list_create (PILOT_WIRE_CLUSTER_ID);
  if (pilot_wire_cluster == nullptr) {
    log_e ("Failed to create Pilot Wire cluster attribute list");
    return false;
  }

  // Add manufacturer-specific attribute for Pilot Wire mode
  err = esp_zb_cluster_add_manufacturer_attr (
          pilot_wire_cluster,
          PILOT_WIRE_CLUSTER_ID,
          PILOT_WIRE_MODE_ATTR_ID,
          PILOT_WIRE_MANUF_CODE,
          ESP_ZB_ZCL_ATTR_TYPE_U8,
          ESP_ZB_ZCL_ATTR_ACCESS_READ_WRITE | ESP_ZB_ZCL_ATTR_ACCESS_REPORTING,
          &_current_mode
        );
  if (err != ESP_OK) {
    log_e ("Failed to add Pilot Wire mode attribute to Pilot Wire cluster");
    return false;
  }

  // Add custom Pilot Wire cluster to cluster list
  err = esp_zb_cluster_list_add_custom_cluster (_cluster_list,
                                                pilot_wire_cluster,
                                                ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  if (err != ESP_OK) {
    log_e ("Failed to add Pilot Wire cluster to Pilot Wire Control endpoint");
    return false;
  }

  log_i ("Basic, Identify, On/Off, Pilot Wire clusters added on EP %d", _endpoint);
  return true;
}

// ----------------------------------------------------------------------------
// protected methods
bool
ZigbeePilotWireControl::createTemperatureMeasurementCluster (float currentTemperature) {
  esp_err_t err;

  // Set current temperature value
  _temperature_value = currentTemperature;
  _temperature_cfg.measured_value = zb_float_to_s16 (currentTemperature);

  // Create a standard temperature measurement cluster attribute list.
  // This only contains the mandatory attribute: measured value, min measured value, max measured value
  // Add Temperature measurement cluster (attribute list) in a cluster list.
  err = esp_zb_cluster_list_add_temperature_meas_cluster (_cluster_list,
                                                          esp_zb_temperature_meas_cluster_create (&_temperature_cfg),
                                                          ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  if (err != ESP_OK) {
    log_e ("Failed to add Temperature Measurement cluster to Pilot Wire Control endpoint");
    return false;
  }
  log_i ("Temperature Measurement cluster (0x0402) added on EP %d", _endpoint);
  return true;
}

// ----------------------------------------------------------------------------
// Create and attach Metering cluster (0x0702)
// protected methods
bool
ZigbeePilotWireControl::createMeteringCluster (int32_t currentPower, uint32_t meteringMultiplier) {
  esp_err_t err;

  if (_nvs_enabled) {

    _summationDelivered = u64_to_esp_zb_uint48 (_prefs.getULong64 ("summation"));
    log_i ("Restored summation from NVS: %llu Wh", _summationDelivered);
  }

  if (meteringMultiplier != 0) {
    _multiplier = u32_to_esp_zb_uint24 (meteringMultiplier);
  }
  _instantaneousDemand = i32_to_esp_zb_sint24 (currentPower);
  _metering_cfg.current_summation_delivered = _summationDelivered; // 0x0000 U48 Current summation delivered Wh

  esp_zb_attribute_list_t *metering_cluster = esp_zb_metering_cluster_create (&_metering_cfg); // just to ensure default values are set
  if (metering_cluster == nullptr) {
    log_e ("Failed to create Metering cluster attribute list");
    return false;
  }

  // --- Enable reporting on CurrentSummationDelivered and Status attribute ---
  esp_zb_attribute_list_t *p = metering_cluster;
  while (p != nullptr) {

    if (p->attribute.type != ESP_ZB_ZCL_ATTR_TYPE_NULL) {
      if (p->attribute.id == ESP_ZB_ZCL_ATTR_METERING_CURRENT_SUMMATION_DELIVERED_ID || // 0x0000
          p->attribute.id == ESP_ZB_ZCL_ATTR_METERING_STATUS_ID) { // 0x0200

        p->attribute.access |= ESP_ZB_ZCL_ATTR_ACCESS_REPORTING;
        log_i ("Enabled reporting on Metering attribute 0x%04X", p->attribute.id);
      }
    }
    p = p->next;
  }

  // --- Historical Consumption Attribute Set ---

  // InstantaneousDemand attribute
  err = esp_zb_cluster_add_attr (
          metering_cluster,
          ESP_ZB_ZCL_CLUSTER_ID_METERING,
          ESP_ZB_ZCL_ATTR_METERING_INSTANTANEOUS_DEMAND_ID, // 0x0400
          ESP_ZB_ZCL_ATTR_TYPE_S24,
          ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY | ESP_ZB_ZCL_ATTR_ACCESS_REPORTING,
          &_instantaneousDemand
        );
  if (err != ESP_OK) {
    log_e ("Failed to add InstantaneousDemand attribute to Metering cluster");
    return false;
  }

  // --- Formatting Attribute Set ---

  // Multiplier attribute
  err = esp_zb_cluster_add_attr (
          metering_cluster,
          ESP_ZB_ZCL_CLUSTER_ID_METERING,
          ESP_ZB_ZCL_ATTR_METERING_MULTIPLIER_ID, // 0x0301
          ESP_ZB_ZCL_ATTR_TYPE_U24,
          ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY,
          &_multiplier
        );
  if (err != ESP_OK) {
    log_e ("Failed to add Multiplier attribute to Metering cluster");
    return false;
  }

  // Divisor attribute
  err = esp_zb_cluster_add_attr (
          metering_cluster,
          ESP_ZB_ZCL_CLUSTER_ID_METERING,
          ESP_ZB_ZCL_ATTR_METERING_DIVISOR_ID, // 0x0302
          ESP_ZB_ZCL_ATTR_TYPE_U24,
          ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY,
          &_divisor
        );
  if (err != ESP_OK) {
    log_e ("Failed to add Divisor attribute to Metering cluster");
    return false;
  }

  // DemandFormatting attribute
  static uint8_t  demandFormatting =
    ESP_ZB_ZCL_METERING_FORMATTING_SET (false, 2, 3);   // Instantaneous demand formatting (U8, 0x0304), 2 digits before decimal, 3 digits after decimal
  err = esp_zb_cluster_add_attr (
          metering_cluster,
          ESP_ZB_ZCL_CLUSTER_ID_METERING,
          ESP_ZB_ZCL_ATTR_METERING_DEMAND_FORMATTING_ID, // 0x304
          ESP_ZB_ZCL_ATTR_TYPE_8BITMAP,
          ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY,
          &demandFormatting
        );
  if (err != ESP_OK) {
    log_e ("Failed to add DemandFormatting attribute to Metering cluster");
    return false;
  }

  // Add Metering cluster to cluster list
  err = esp_zb_cluster_list_add_metering_cluster (_cluster_list,
                                                  metering_cluster,
                                                  ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  if (err != ESP_OK) {
    log_e ("Failed to add Metering cluster to cluster list");
    return false;
  }

  log_i ("Metering cluster (0x0702) added on EP %d", _endpoint);
  return true;
}

// ----------------------------------------------------------------------------
bool
ZigbeePilotWireControl::begin () {

  // Init NVS
  _prefs.begin ("PilotWire", false); // namespace "PilotWire"
  _nvs_enabled = _prefs.getBool ("restore");

  return createPilotWireCluster();
}

// ----------------------------------------------------------------------------
bool
ZigbeePilotWireControl::begin (float currentTemperature) {

  if (begin()) {

    if (_temperature_enabled) {

      return createTemperatureMeasurementCluster (currentTemperature);
    }
    else {

      log_w ("Temperature Measurement cluster not enabled");
    }
    return true;
  }
  return false;
}

// ----------------------------------------------------------------------------
bool
ZigbeePilotWireControl::begin (int32_t currentPower, uint32_t meteringMultiplier) {

  if (begin()) {

    if (_metering_enabled) {

      return createMeteringCluster (currentPower, meteringMultiplier);
    }
    else {

      log_w ("Metering cluster not enabled");
    }
    return true;
  }
  return false;
}

// ----------------------------------------------------------------------------
bool
ZigbeePilotWireControl::begin (float currentTemperature, int32_t currentPower, uint32_t meteringMultiplier) {

  if (begin (currentTemperature)) {

    if (_metering_enabled) {

      return createMeteringCluster (currentPower, meteringMultiplier);
    }
    else {

      log_w ("Metering cluster not enabled");
    }
    return true;
  }
  return false;
}

// -----------------------------------------------------------------------------
bool
ZigbeePilotWireControl::setEnergyWh (uint64_t summation_wh) {

  _summationDelivered = u64_to_esp_zb_uint48 (summation_wh);
  if (_nvs_enabled) {

    // Save to NVS
    _prefs.putULong64 ("summation", summation_wh);
  }

  esp_zb_lock_acquire (portMAX_DELAY);
  esp_zb_zcl_status_t ret = esp_zb_zcl_set_attribute_val (
                              _endpoint,
                              ESP_ZB_ZCL_CLUSTER_ID_METERING,
                              ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
                              ESP_ZB_ZCL_ATTR_METERING_CURRENT_SUMMATION_DELIVERED_ID,
                              &_summationDelivered,
                              false // do not check access rights
                            );
  esp_zb_lock_release();
  if (ret != ESP_ZB_ZCL_STATUS_SUCCESS) {

    log_e ("Failed to set CurrentSummationDelivered: 0x%x: %s", ret, esp_zb_zcl_status_to_name (ret));
    return false;
  }
  return true;
}

// -----------------------------------------------------------------------------
bool
ZigbeePilotWireControl::setPowerW (int32_t demand_w) {

  _instantaneousDemand = i32_to_esp_zb_sint24 (demand_w);
  esp_zb_lock_acquire (portMAX_DELAY);
  esp_zb_zcl_status_t ret = esp_zb_zcl_set_attribute_val (
                              _endpoint,
                              ESP_ZB_ZCL_CLUSTER_ID_METERING,
                              ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
                              ESP_ZB_ZCL_ATTR_METERING_INSTANTANEOUS_DEMAND_ID,
                              &_instantaneousDemand,
                              false // do not check access rights
                            );
  esp_zb_lock_release();
  if (ret != ESP_ZB_ZCL_STATUS_SUCCESS) {

    log_e ("Failed to set InstantaneousDemand: 0x%x: %s", ret, esp_zb_zcl_status_to_name (ret));
    return false;
  }
  return true;
}

// -----------------------------------------------------------------------------
uint64_t
ZigbeePilotWireControl::energyWh() const {

  return esp_zb_uint48_to_u64 (_summationDelivered);
}

// -----------------------------------------------------------------------------
int32_t
ZigbeePilotWireControl::powerW() const {

  return esp_zb_sint24_to_i32 (_instantaneousDemand);
}

// -----------------------------------------------------------------------------
bool
ZigbeePilotWireControl::setMeteringStatus (uint8_t status) {

  _metering_cfg.status = status;
  esp_zb_lock_acquire (portMAX_DELAY);
  esp_zb_zcl_status_t ret = esp_zb_zcl_set_attribute_val (
                              _endpoint,
                              ESP_ZB_ZCL_CLUSTER_ID_METERING,
                              ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
                              ESP_ZB_ZCL_ATTR_METERING_STATUS_ID,
                              &_metering_cfg.status,
                              false // do not check access rights
                            );
  esp_zb_lock_release();
  if (ret != ESP_ZB_ZCL_STATUS_SUCCESS) {

    log_e ("Failed to set Metering Status: 0x%x: %s", ret, esp_zb_zcl_status_to_name (ret));
    return false;
  }
  return true;
}

// ----------------------------------------------------------------------------
// callback method called when an attribute is set from Zigbee network
// attributes handled:
// - Pilot Wire Mode (manufacturer-specific attribute)
// - On/Off
void
ZigbeePilotWireControl::zbAttributeSet (const esp_zb_zcl_set_attr_value_message_t *message) {

  if (message->info.cluster == PILOT_WIRE_CLUSTER_ID) {

    if (message->attribute.id == PILOT_WIRE_MODE_ATTR_ID && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_U8) {
      uint8_t mode = *reinterpret_cast<uint8_t *> (message->attribute.data.value);

      if (mode != _current_mode) {
        if (mode == PILOTWIRE_MODE_OFF) {

          // Save current mode when turning off
          _state_on_mode = _current_mode;
          _current_state = false;
          _current_state_changed = true;
        }
        else if (_current_mode == PILOTWIRE_MODE_OFF) {

          _current_state = true;
          _current_state_changed = true;
        }

        _current_mode = mode;
        pilotWireModeChanged();
        if (_current_state_changed) {

          // _current_state changed, report attribute
          _current_state_changed = false;
          log_v ("Updating On/Off attribute to %d", _current_state);
          esp_zb_zcl_status_t ret;
          esp_zb_lock_acquire (portMAX_DELAY);
          ret = esp_zb_zcl_set_attribute_val (
                  _endpoint,
                  ESP_ZB_ZCL_CLUSTER_ID_ON_OFF,
                  ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
                  ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID,
                  &_current_state,
                  false
                );
          esp_zb_lock_release();
          if (ret != ESP_ZB_ZCL_STATUS_SUCCESS) {
            log_e ("Failed to update On/Off attribute: 0x%x: %s", ret, esp_zb_zcl_status_to_name (ret));
          }
        }
      }

    }
    else {

      log_w ("Received message ignored. Attribute ID: 0x%04X not supported for Pilot Wire Control", message->attribute.id);
    }
  }
  else if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_ON_OFF) {

    if (message->attribute.id == ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_BOOL) {
      bool state = *reinterpret_cast<bool *> (message->attribute.data.value);

      if (state != _current_state) {

        log_v ("On/Off attribute changed to %d", state);
        _current_state = state;

        if (state) {
          // new state is ON, restore previous mode
          _current_mode = _state_on_mode;
        }
        else  {
          // new state is OFF, save current mode
          _state_on_mode = _current_mode;
          _current_mode = PILOTWIRE_MODE_OFF;
        }
        pilotWireModeChanged();

        // _current_mode changed, report attribute
        log_v ("Updating Pilot Wire mode attribute to %d", _current_mode);
        esp_zb_zcl_status_t ret;
        esp_zb_lock_acquire (portMAX_DELAY);
        ret = esp_zb_zcl_set_manufacturer_attribute_val (
                _endpoint,
                PILOT_WIRE_CLUSTER_ID,
                ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
                PILOT_WIRE_MANUF_CODE,
                PILOT_WIRE_MODE_ATTR_ID,
                &_current_mode,
                false
              );
        esp_zb_lock_release();
        if (ret != ESP_ZB_ZCL_STATUS_SUCCESS) {
          log_e ("Failed to update Pilot Wire mode attribute: 0x%x: %s", ret, esp_zb_zcl_status_to_name (ret));
        }
      }
    }
    else {

      log_w ("Received message ignored. Attribute ID: 0x%04X not supported for On/Off", message->attribute.id);
    }
  }
  else {

    log_w ("Received message ignored. Cluster ID: 0x%04X not supported for Pilot Wire Control", message->info.cluster);
  }
}

// ----------------------------------------------------------------------------
// Called whenever Pilot Wire mode changes
void
ZigbeePilotWireControl::pilotWireModeChanged() {
  log_i ("Pilot Wire mode changed to %d", _current_mode);

  // Save current mode persistently in NVS
  _prefs.putInt ("mode", _current_mode);
  _current_state = (_current_mode != PILOTWIRE_MODE_OFF);

  if (_on_mode_change) {

    _on_mode_change (static_cast<ZigbeePilotWireMode> (_current_mode));
  }
  else {

    log_w ("No callback function set for pilot wire mode change");
  }
}

// ----------------------------------------------------------------------------
bool
ZigbeePilotWireControl::setPilotWireMode (ZigbeePilotWireMode mode) {

  if (mode != _current_mode) {

    if (mode == PILOTWIRE_MODE_OFF) {

      // Save current mode when turning off
      _state_on_mode = _current_mode;
      _current_state = false;
      _current_state_changed = true;
    }
    else if (_current_mode == PILOTWIRE_MODE_OFF) {

      _current_state = true;
      _current_state_changed = true;
    }

    _current_mode = mode;
    return reportAttributes();
  }
  return true;
}

// ----------------------------------------------------------------------------
bool
ZigbeePilotWireControl::setTemperature (float temperature) {
  if (_temperature_enabled) {

    esp_zb_zcl_status_t ret = ESP_ZB_ZCL_STATUS_SUCCESS;
    int16_t zb_temperature = zb_float_to_s16 (temperature);
    log_v ("Updating temperature sensor value...");
    /* Update temperature sensor measured value */
    log_d ("Setting temperature to %d", zb_temperature);
    esp_zb_lock_acquire (portMAX_DELAY);
    ret = esp_zb_zcl_set_attribute_val (
            _endpoint, ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID, &zb_temperature, false
          );
    esp_zb_lock_release();
    if (ret != ESP_ZB_ZCL_STATUS_SUCCESS) {
      log_e ("Failed to set temperature: 0x%x: %s", ret, esp_zb_zcl_status_to_name (ret));
      return false;
    }
    return true;
  }
  log_w ("Temperature measurement cluster not enabled");
  return false;
}

// ----------------------------------------------------------------------------
bool
ZigbeePilotWireControl::setTemperatureReporting (uint16_t min_interval, uint16_t max_interval, float delta) {

  if (_temperature_enabled) {

    return  setReporting (ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT,
                          ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID,
                          min_interval, max_interval, delta * 100.0f); // delta in 0.01 °C;
  }
  log_w ("Temperature measurement cluster not enabled");
  return true;
}

// ----------------------------------------------------------------------------
float
ZigbeePilotWireControl::temperatureMin() const {
  return zb_s16_to_float (_temperature_cfg.min_value);
}

// ----------------------------------------------------------------------------
float
ZigbeePilotWireControl::temperatureMax() const {
  return zb_s16_to_float (_temperature_cfg.max_value);
}
// -----------------------------------------------------------------------------
bool
ZigbeePilotWireControl::setEnergyWhReporting (uint16_t min_interval, uint16_t max_interval, float delta) {

  if (_metering_enabled) {

    return setReporting (ESP_ZB_ZCL_CLUSTER_ID_METERING,
                         ESP_ZB_ZCL_ATTR_METERING_CURRENT_SUMMATION_DELIVERED_ID,
                         min_interval, max_interval, delta);
  }

  log_w ("Metering cluster not enabled on this endpoint");
  return true;
}

// -----------------------------------------------------------------------------
bool
ZigbeePilotWireControl::setPowerWReporting (uint16_t min_interval, uint16_t max_interval, float delta) {
  if (_metering_enabled) {

    return setReporting (ESP_ZB_ZCL_CLUSTER_ID_METERING,
                         ESP_ZB_ZCL_ATTR_METERING_INSTANTANEOUS_DEMAND_ID,
                         min_interval, max_interval, delta);
  }

  log_w ("Metering cluster not enabled on this endpoint");
  return true;
}

// ----------------------------------------------------------------------------
// protected method with manuf_code parameter
bool
ZigbeePilotWireControl::setReporting (uint16_t cluster_id, uint16_t attr_id,
                                      uint16_t min_interval, uint16_t max_interval, float delta, uint16_t manuf_code) {
  esp_err_t ret;
  esp_zb_zcl_reporting_info_t reporting_info;

  memset (&reporting_info, 0, sizeof (esp_zb_zcl_reporting_info_t));
  reporting_info.direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_SRV;
  reporting_info.ep = _endpoint;
  reporting_info.cluster_id = cluster_id;
  reporting_info.cluster_role = ESP_ZB_ZCL_CLUSTER_SERVER_ROLE;
  reporting_info.attr_id = attr_id;
  reporting_info.u.send_info.min_interval = min_interval;
  reporting_info.u.send_info.max_interval = max_interval;
  reporting_info.u.send_info.def_min_interval = min_interval;
  reporting_info.u.send_info.def_max_interval = max_interval;
  reporting_info.u.send_info.delta.u16 = static_cast<uint16_t> (delta + 0.5f); // Convert delta to ZCL uint16_t
  reporting_info.dst.profile_id = ESP_ZB_AF_HA_PROFILE_ID;
  reporting_info.manuf_code = manuf_code;

  esp_zb_lock_acquire (portMAX_DELAY);
  ret = esp_zb_zcl_update_reporting_info (&reporting_info);
  esp_zb_lock_release();

  if (ret != ESP_OK) {
    log_e ("Failed to set reporting cluster 0x%04X: 0x%x: %s", cluster_id, ret, esp_err_to_name (ret));
    return false;
  }
  return true;
}

// ----------------------------------------------------------------------------
bool
ZigbeePilotWireControl::reportTemperature() {

  if (_temperature_enabled) {
    return reportAttribute (ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT,
                            ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID);
  }
  log_w ("Temperature measurement cluster not enabled");
  return true;
}

// ---------------------------------------------------------------------------
bool
ZigbeePilotWireControl::reportEnergyWh() {

  if (_metering_enabled) {
    return reportAttribute (ESP_ZB_ZCL_CLUSTER_ID_METERING,
                            ESP_ZB_ZCL_ATTR_METERING_CURRENT_SUMMATION_DELIVERED_ID);
  }
  log_w ("Metering cluster not enabled on this endpoint");
  return true;
}

// ----------------------------------------------------------------------------
bool
ZigbeePilotWireControl::reportPowerW() {

  if (_metering_enabled) {
    return reportAttribute (ESP_ZB_ZCL_CLUSTER_ID_METERING,
                            ESP_ZB_ZCL_ATTR_METERING_INSTANTANEOUS_DEMAND_ID);
  }
  log_w ("Metering cluster not enabled on this endpoint");
  return true;
}

// ----------------------------------------------------------------------------
bool
ZigbeePilotWireControl::reportAttributes() {
  esp_zb_zcl_status_t ret;
  bool status = true;

  pilotWireModeChanged();

  // Report Pilot Wire mode attribute
  log_v ("Reporting Pilot Wire mode attribute: %d", _current_mode);
  esp_zb_lock_acquire (portMAX_DELAY);
  ret = esp_zb_zcl_set_manufacturer_attribute_val (
          _endpoint,
          PILOT_WIRE_CLUSTER_ID,
          ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
          PILOT_WIRE_MANUF_CODE,
          PILOT_WIRE_MODE_ATTR_ID,
          &_current_mode,
          false
        );
  esp_zb_lock_release();
  if (ret != ESP_ZB_ZCL_STATUS_SUCCESS) {
    log_e ("Failed to update Pilot Wire mode attribute: 0x%x: %s", ret, esp_zb_zcl_status_to_name (ret));
    status = false;
  }

  // Report On/Off attribute
  if (_current_state_changed) {

    // _current_state changed, report attribute
    _current_state_changed = false;
    log_v ("Updating On/Off attribute to %d", _current_state);
    esp_zb_lock_acquire (portMAX_DELAY);
    ret = esp_zb_zcl_set_attribute_val (
            _endpoint,
            ESP_ZB_ZCL_CLUSTER_ID_ON_OFF,
            ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
            ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID,
            &_current_state,
            false
          );
    esp_zb_lock_release();
    if (ret != ESP_ZB_ZCL_STATUS_SUCCESS) {
      log_e ("Failed to update On/Off attribute: 0x%x: %s", ret, esp_zb_zcl_status_to_name (ret));
      status = false;
    }
  }

  if (_temperature_enabled) {
    if (reportTemperature() == false) {
      status = false;
    }
  }

  if (_metering_enabled) {
    if (reportEnergyWh() == false) {
      status = false;
    }
    if (reportPowerW() == false) {
      status = false;
    }
  }
  return status;
}

// ----------------------------------------------------------------------------
// protected method
bool
ZigbeePilotWireControl::reportAttribute (uint16_t cluster_id, uint16_t attr_id,
                                         uint16_t manuf_code) {
  /* Send report attributes command */
  esp_zb_zcl_report_attr_cmd_t report_attr_cmd;
  report_attr_cmd.address_mode = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT;
  report_attr_cmd.attributeID = attr_id;
  report_attr_cmd.direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_CLI;
  report_attr_cmd.clusterID = cluster_id;
  report_attr_cmd.zcl_basic_cmd.src_endpoint = _endpoint;
  report_attr_cmd.manuf_code = manuf_code;

  esp_zb_lock_acquire (portMAX_DELAY);
  esp_err_t ret = esp_zb_zcl_report_attr_cmd_req (&report_attr_cmd);
  esp_zb_lock_release();

  if (ret != ESP_OK) {
    log_e ("Failed to send attribute report: 0x%x: %s", ret, esp_err_to_name (ret));
    return false;
  }

  log_v ("Attribute report sent for cluster 0x%04X attr 0x%04X", cluster_id, attr_id);
  return true;
}

// ----------------------------------------------------------------------------
void
ZigbeePilotWireControl::printClusterInfo (Print &out) {
  esp_zb_cluster_list_t *cluster_list = _cluster_list;
  int count = 0;

  out.printf ("ZigbeePilotWireControl Endpoint %d Cluster Info:\n", _endpoint);
  while (cluster_list != nullptr) {
    esp_zb_attribute_list_t *attr_list = cluster_list->cluster.attr_list;

    if (attr_list != nullptr) {

      out.printf (
        "  Cluster %d ID: 0x%04X\n", count, cluster_list->cluster.cluster_id);

      while (attr_list != nullptr) {

        if (attr_list->attribute.type != ESP_ZB_ZCL_ATTR_TYPE_NULL) {

          out.printf (
            "    Attr ID: 0x%04X - Type: 0x%02X - Access: 0x%02X - Manuf: 0x%04X\n",
            attr_list->attribute.id,
            attr_list->attribute.type,
            attr_list->attribute.access,
            attr_list->attribute.manuf_code);
        }
        attr_list = attr_list->next;
      }
      count++;
    }
    cluster_list = cluster_list->next;
  }
  out.printf ("Total Clusters: %d\n", count);
}
