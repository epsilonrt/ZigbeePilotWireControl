// SPDX-Identifier: BSD-3-Clause
// SPDX-FileCopyrightText: 2025 Pascal JEAN aka epsilonrt

#include "ZigbeePilotWireControl.h"


// ----------------------------------------------------------------------------
ZigbeePilotWireControl::ZigbeePilotWireControl (uint8_t endpoint, bool enableMetering, bool enableTemperature) :
  ZigbeeEP (endpoint), _current_mode (PILOTWIRE_MODE_OFF),
  _state_on_mode (PILOTWIRE_MODE_COMFORT), _on_mode_change (nullptr),
  _current_state (false) {

  _device_id = ESP_ZB_HA_SMART_PLUG_DEVICE_ID;
}

// ----------------------------------------------------------------------------
void
ZigbeePilotWireControl::begin() {

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

  // Add metering and temperature clusters if enabled
  // TODO
  _ep_config = {
    .endpoint = _endpoint,
    .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
    .app_device_id = ESP_ZB_HA_SMART_PLUG_DEVICE_ID,
    .app_device_version = 0
  };

  if (setManufacturerAndModel (PILOT_WIRE_MANUF_NAME, PILOT_WIRE_MODEL_NAME)) {
    log_i ("Manufacturer and Model set for Pilot Wire Control");
  }
  else {
    log_w ("Failed to set Manufacturer and Model for Pilot Wire Control");
  }
}

// ----------------------------------------------------------------------------
// callback method called when an attribute is set from Zigbee network
void
ZigbeePilotWireControl::zbAttributeSet (const esp_zb_zcl_set_attr_value_message_t *message) {

  if (message->info.cluster == PILOT_WIRE_CLUSTER_ID) {

    if (message->attribute.id == PILOT_WIRE_MODE_ATTR_ID && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_U8) {
      uint8_t mode = *reinterpret_cast<uint8_t *> (message->attribute.data.value);

      if (mode != _current_mode) {
        bool is_state_updating = false;
        if (mode == PILOTWIRE_MODE_OFF) {

          // Save current mode when turning off
          _state_on_mode = _current_mode;
          _current_state = false;
          is_state_updating = true;
        }
        else if (_current_mode == PILOTWIRE_MODE_OFF) {

          _current_state = true;
          is_state_updating = true;
        }

        _current_mode = mode;
        pilotWireModeChanged();
        if (is_state_updating) {

          // _current_state changed, report attribute
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
void
ZigbeePilotWireControl::pilotWireModeChanged() {

  log_i ("Pilot Wire mode changed to %d", _current_mode);
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
    bool is_state_updating = false;
    esp_zb_zcl_status_t ret;

    if (mode == PILOTWIRE_MODE_OFF) {

      // Save current mode when turning off
      _state_on_mode = _current_mode;
      _current_state = false;
      is_state_updating = true;
    }
    else if (_current_mode == PILOTWIRE_MODE_OFF) {

      _current_state = true;
      is_state_updating = true;
    }

    _current_mode = mode;
    pilotWireModeChanged();

    log_v ("Updating Pilot Wire mode attribute to %d", _current_mode);
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

    if (is_state_updating) {

      // _current_state changed, report attribute
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
  }
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
