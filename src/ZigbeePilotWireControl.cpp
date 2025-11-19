/// @file ZigbeePilotWireControl.cpp
/// SPDX-License-Identifier: BSD-3-Clause
/// SPDX-FileCopyrightText: 2025 Pascal JEAN aka epsilonrt

#include "ZigbeePilotWireControl.h"

// ----------------------------------------------------------------------------
ZigbeePilotWireControl::ZigbeePilotWireControl (uint8_t endpoint) :
  ZigbeeEP (endpoint), _current_mode (PILOTWIRE_MODE_OFF),
  _state_on_mode (PILOTWIRE_MODE_COMFORT), _on_mode_change (nullptr),
  _current_state (false), _current_state_changed (true), _restore_mode (false),
  _temperature_enabled (false), _temperature_value (NAN),
  _temperature_min (NAN), _temperature_max (NAN), _temperature_tolerance (NAN) {

  _device_id = ESP_ZB_HA_SMART_PLUG_DEVICE_ID;
}

// ----------------------------------------------------------------------------
void
ZigbeePilotWireControl::begin (bool enableMetering, bool enableTemperature) {
  // Init NVS
  _prefs.begin ("PilotWire", false); // namespace "PilotWire"
  _restore_mode = _prefs.getBool ("restore");

  // Create cluster list
  _cluster_list = esp_zb_zcl_cluster_list_create();
  ESP_ERROR_CHECK (_cluster_list != nullptr ? ESP_OK : ESP_FAIL);

  // Add basic cluster
  ESP_ERROR_CHECK (esp_zb_cluster_list_add_basic_cluster (_cluster_list,
                                                          esp_zb_basic_cluster_create (NULL),
                                                          ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));

  // Add identify cluster
  ESP_ERROR_CHECK (esp_zb_cluster_list_add_identify_cluster (_cluster_list,
                                                             esp_zb_identify_cluster_create (NULL),
                                                             ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));

  // Add on/off cluster
  ESP_ERROR_CHECK (esp_zb_cluster_list_add_on_off_cluster (_cluster_list,
                                                           esp_zb_on_off_cluster_create (NULL),
                                                           ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));


  // Create custom Pilot Wire cluster with manufacturer-specific attribute
  esp_zb_attribute_list_t *pilot_wire_cluster = esp_zb_zcl_attr_list_create (PILOT_WIRE_CLUSTER_ID);
  ESP_ERROR_CHECK (pilot_wire_cluster != nullptr ? ESP_OK : ESP_FAIL);

  // Add manufacturer-specific attribute for Pilot Wire mode
  ESP_ERROR_CHECK (esp_zb_cluster_add_manufacturer_attr (
                     pilot_wire_cluster,
                     PILOT_WIRE_CLUSTER_ID,
                     PILOT_WIRE_MODE_ATTR_ID,
                     PILOT_WIRE_MANUF_CODE,
                     ESP_ZB_ZCL_ATTR_TYPE_U8,
                     ESP_ZB_ZCL_ATTR_ACCESS_READ_WRITE | ESP_ZB_ZCL_ATTR_ACCESS_REPORTING,
                     &_current_mode
                   ));

  // Add custom Pilot Wire cluster to cluster list
  ESP_ERROR_CHECK (esp_zb_cluster_list_add_custom_cluster (_cluster_list,
                                                           pilot_wire_cluster,
                                                           ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));

  if (enableTemperature) {
    // esp_zb_temperature_meas_cluster_cfg_t temp_cfg = {
    //   .measured_value = ESP_ZB_ZCL_TEMP_MEASUREMENT_MEASURED_VALUE_DEFAULT,
    //   .min_value = ESP_ZB_ZCL_TEMP_MEASUREMENT_MIN_MEASURED_VALUE_DEFAULT,
    //   .max_value = ESP_ZB_ZCL_TEMP_MEASUREMENT_MAX_MEASURED_VALUE_DEFAULT,
    // };
    // Create a standard temperature measurement cluster attribute list.
    // This only contains the mandatory attribute: measured value, min measured value, max measured value
    // Add Temperature measurement cluster (attribute list) in a cluster list.
    ESP_ERROR_CHECK (esp_zb_cluster_list_add_temperature_meas_cluster (_cluster_list,
                                                                       esp_zb_temperature_meas_cluster_create (NULL),
                                                                       ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));
    _temperature_enabled = true;
  }

  if (enableMetering) {
    // esp_zb_metering_cluster_cfg_t metering_cfg = {
    //   .current_summation_delivered = 0, // This attribute represents the most recent summed value of Energy, Gas, or Water delivered and consumed in the premises
    //   .status = 0, // This attribute provides indicators reflecting the current error conditions found by the metering device
    //   .uint_of_measure = ESP_ZB_ZCL_METERING_UNIT_KW_KWH_BINARY, // This attribute provides a label for the Energy, Gas, or Water being measured by the metering device. refer to esp_zb_zcl_metering_unit_of_measure_t
    //   .summation_formatting = ESP_ZB_ZCL_METERING_SUMMATION_FORMATTING_32_BITS, // This attribute provides a method to properly decipher the number of digits and the decimal location of the values found in the Summation Information Set
    //   .metering_device_type = ESP_ZB_ZCL_METERING_ELECTRIC_METERING
    // };

    // Create a standard metering attribute list
    // Add electrical measurement cluster (attribute list) in a cluster list.
    ESP_ERROR_CHECK (esp_zb_cluster_list_add_metering_cluster (_cluster_list,
                                                               esp_zb_metering_cluster_create (NULL),
                                                               ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));
  }

  _ep_config = {
    .endpoint = _endpoint,
    .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
    .app_device_id = ESP_ZB_HA_SMART_PLUG_DEVICE_ID,
    .app_device_version = 0
  };

  if (_restore_mode) {
    _current_mode = _prefs.getInt ("mode", PILOTWIRE_MODE_OFF);
    log_i ("Restored mode from NVS: %d", _current_mode);
  }
  else {
    log_i ("Starting with default mode: %d", _current_mode);
  }

  if (setManufacturerAndModel (PILOT_WIRE_MANUF_NAME, PILOT_WIRE_MODEL_NAME)) {
    log_i ("Manufacturer and Model set for Pilot Wire Control");
  }
  else {
    log_w ("Failed to set Manufacturer and Model for Pilot Wire Control");
  }

  // Force synchronization with Zigbee attributes
  setPilotWireMode (static_cast<ZigbeePilotWireMode> (_current_mode));
  log_i ("Zigbee Pilot Wire Control endpoint initialized on EP %d", _endpoint);
}

// ----------------------------------------------------------------------------
// callback method called when an attribute is set from Zigbee network
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
ZigbeePilotWireControl::reportAttributes() {
  esp_zb_zcl_status_t ret;

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
    return false;
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
      return false;
    }
  }

  if (_temperature_enabled) {

    if (reportTemperature() == false) {
      log_e ("Failed to report Temperature attribute");
      return false;
    }
  }
  return true;
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
static int16_t
zb_float_to_s16 (float temp) {
  if (isnan (temp)) {
    return ESP_ZB_ZCL_TEMP_MEASUREMENT_MEASURED_VALUE_DEFAULT; // valeur invalide normalisée ZCL
  }
  return (int16_t) (temp * 100.0f);
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
ZigbeePilotWireControl::reportTemperature() {
  if (_temperature_enabled) {
    return reportAttribute (ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT,
                            ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID,
                            ESP_ZB_ZCL_ATTR_NON_MANUFACTURER_SPECIFIC);
  }
  log_w ("Temperature measurement cluster not enabled");
  return false;
}

// ----------------------------------------------------------------------------
bool
ZigbeePilotWireControl::setTemperatureReporting (uint16_t min_interval, uint16_t max_interval, float delta) {

  if (_temperature_enabled) {
    if (setReporting (ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT,
                      ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID,
                      min_interval, max_interval, delta,
                      ESP_ZB_ZCL_ATTR_NON_MANUFACTURER_SPECIFIC) == false) {
      return false;
    }
  }
  return true;
}

// ----------------------------------------------------------------------------
// TODO: debug and test
bool
ZigbeePilotWireControl::setMinMaxTemperature (float min, float max) {

  if (_temperature_enabled) {

    int16_t zb_min = zb_float_to_s16 (min);
    int16_t zb_max = zb_float_to_s16 (max);
    esp_zb_attribute_list_t *temp_measure_cluster =
      esp_zb_cluster_list_get_cluster (_cluster_list, ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_err_t ret = esp_zb_cluster_update_attr (temp_measure_cluster, ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_MIN_VALUE_ID, (void *) &zb_min);
    if (ret != ESP_OK) {
      log_e ("Failed to set min value: 0x%x: %s", ret, esp_err_to_name (ret));
      return false;
    }
    ret = esp_zb_cluster_update_attr (temp_measure_cluster, ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_MAX_VALUE_ID, (void *) &zb_max);
    if (ret != ESP_OK) {
      log_e ("Failed to set max value: 0x%x: %s", ret, esp_err_to_name (ret));
      return false;
    }
    _temperature_min = min;
    _temperature_max = max;
    return true;
  }
  log_w ("Temperature measurement cluster not enabled");
  return false;
}

// ----------------------------------------------------------------------------
// TODO: debug and test
bool
ZigbeePilotWireControl::setTemperatureTolerance (float tolerance) {
  if (_temperature_enabled) {

    // Convert tolerance to ZCL uint16_t
    uint16_t zb_tolerance = (uint16_t) (tolerance * 100);
    esp_zb_attribute_list_t *temp_measure_cluster =
      esp_zb_cluster_list_get_cluster (_cluster_list, ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_err_t ret = esp_zb_temperature_meas_cluster_add_attr (temp_measure_cluster, ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_TOLERANCE_ID, (void *) &zb_tolerance);
    if (ret != ESP_OK) {
      log_e ("Failed to set tolerance: 0x%x: %s", ret, esp_err_to_name (ret));
      return false;
    }
    _temperature_tolerance = tolerance;
    return true;
  }
  log_w ("Temperature measurement cluster not enabled");
  return false;
}

// ----------------------------------------------------------------------------
// protected method with manuf_code parameter
bool
ZigbeePilotWireControl::setReporting (uint16_t cluster_id, uint16_t attr_id,
                                      uint16_t min_interval, uint16_t max_interval, float delta, uint16_t manuf_code) {

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
  reporting_info.u.send_info.delta.u16 = (uint16_t) (delta * 100); // Convert delta to ZCL uint16_t
  reporting_info.dst.profile_id = ESP_ZB_AF_HA_PROFILE_ID;
  reporting_info.manuf_code = manuf_code;
  esp_zb_lock_acquire (portMAX_DELAY);
  esp_err_t ret = esp_zb_zcl_update_reporting_info (&reporting_info);
  esp_zb_lock_release();
  if (ret != ESP_OK) {
    log_e ("Failed to set reporting: 0x%x: %s", ret, esp_err_to_name (ret));
    return false;
  }
  return true;
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
  log_v ("Attribute report sent");
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
