// SPDX-Identifier: BSD-3-Clause
// SPDX-FileCopyrightText: 2025 Pascal JEAN aka epsilonrt

#include "ZigbeePilotWireControl.h"

ZigbeePilotWireControl::ZigbeePilotWireControl (uint8_t endpoint) :
  ZigbeeEP (endpoint), _current_mode (PILOTWIRE_MODE_OFF), _on_mode_change (nullptr) {
  esp_err_t ret;

  _device_id = ESP_ZB_HA_THERMOSTAT_DEVICE_ID;

  // Create cluster list
  _cluster_list = esp_zb_zcl_cluster_list_create();

  // Add basic cluster
  esp_zb_cluster_list_add_basic_cluster (_cluster_list, esp_zb_basic_cluster_create (NULL), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

  // Add identify cluster
  esp_zb_cluster_list_add_identify_cluster (_cluster_list, esp_zb_identify_cluster_create (NULL), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

  // Création du cluster custom Pilot Wire avec attribut manufacturer-specific
  esp_zb_attribute_list_t *pilot_wire_cluster = esp_zb_zcl_attr_list_create (PILOT_WIRE_CLUSTER_ID);

  uint8_t initial_mode = (uint8_t) _current_mode;

  ret = esp_zb_cluster_add_manufacturer_attr (
          pilot_wire_cluster,
          PILOT_WIRE_CLUSTER_ID,
          PILOT_WIRE_ATTR_MODE,
          MANUFACTURER_CODE,
          ESP_ZB_ZCL_ATTR_TYPE_8BIT_ENUM,
          ESP_ZB_ZCL_ATTR_ACCESS_READ_WRITE | ESP_ZB_ZCL_ATTR_ACCESS_REPORTING,
          &initial_mode
        );
  if (ret != ESP_OK) {
    log_e ("Failed to add manufacturer-specific attribute to Pilot Wire cluster (error code: %d)", ret);
  }

  esp_zb_cluster_list_add_custom_cluster (_cluster_list, pilot_wire_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

  _ep_config = {
    .endpoint = _endpoint,
    .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
    .app_device_id = ESP_ZB_HA_THERMOSTAT_DEVICE_ID,
    .app_device_version = 0
  };
}


void
ZigbeePilotWireControl::zbAttributeSet (const esp_zb_zcl_set_attr_value_message_t *message) {

  if (message->info.cluster == PILOT_WIRE_CLUSTER_ID) {

    if (message->attribute.id == PILOT_WIRE_ATTR_MODE && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_8BIT_ENUM) {
      _current_mode = * (ZigbeePilotWireMode *) message->attribute.data.value;
      log_i ("Pilot Wire mode changed to: %d", _current_mode);
      pilotWireModeChanged();
    }
    else {
      log_w ("Received message ignored. Attribute ID: 0x%04X not supported for Pilot Wire Control", message->attribute.id);
    }
  }
  else {
    log_w ("Received message ignored. Cluster ID: 0x%04X not supported for Pilot Wire Control", message->info.cluster);
  }
}

void ZigbeePilotWireControl::pilotWireModeChanged() {

  if (_on_mode_change) {

    _on_mode_change (_current_mode);
  }
  else {

    log_w ("No callback function set for pilot wire mode change");
  }
}
