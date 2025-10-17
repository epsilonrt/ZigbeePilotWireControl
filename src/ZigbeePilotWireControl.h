// SPDX-Identifier: BSD-3-Clause
// SPDX-FileCopyrightText: 2025 Pascal JEAN aka epsilonrt
#pragma once

#include <soc/soc_caps.h>
#include <sdkconfig.h>

#if ! CONFIG_ZB_ENABLED
#error "This library requires Zigbee support to be enabled in sdkconfig"
#endif  // CONFIG_ZB_ENABLED

#include <ZigbeeEP.h>
#include <ha/esp_zigbee_ha_standard.h>

// Custom Arduino-friendly enums for fan mode values
enum ZigbeePilotWireMode {
  PILOTWIRE_MODE_OFF = ESP_ZB_ZCL_FAN_CONTROL_FAN_MODE_OFF,
  PILOTWIRE_MODE_LOW = ESP_ZB_ZCL_FAN_CONTROL_FAN_MODE_LOW,
  PILOTWIRE_MODE_MEDIUM = ESP_ZB_ZCL_FAN_CONTROL_FAN_MODE_MEDIUM,
  PILOTWIRE_MODE_HIGH = ESP_ZB_ZCL_FAN_CONTROL_FAN_MODE_HIGH,
  PILOTWIRE_MODE_ON = ESP_ZB_ZCL_FAN_CONTROL_FAN_MODE_ON,
  PILOTWIRE_MODE_AUTO = ESP_ZB_ZCL_FAN_CONTROL_FAN_MODE_AUTO,
  PILOTWIRE_MODE_SMART = ESP_ZB_ZCL_FAN_CONTROL_FAN_MODE_SMART,
};

// Custom Arduino-friendly enums for fan mode sequence
enum ZigbeePilotWireModeSequence {
  PILOTWIRE_MODE_SEQUENCE_LOW_MED_HIGH = ESP_ZB_ZCL_FAN_CONTROL_FAN_MODE_SEQUENCE_LOW_MED_HIGH,
  PILOTWIRE_MODE_SEQUENCE_LOW_HIGH = ESP_ZB_ZCL_FAN_CONTROL_FAN_MODE_SEQUENCE_LOW_HIGH,
  PILOTWIRE_MODE_SEQUENCE_LOW_MED_HIGH_AUTO = ESP_ZB_ZCL_FAN_CONTROL_FAN_MODE_SEQUENCE_LOW_MED_HIGH_AUTO,
  PILOTWIRE_MODE_SEQUENCE_LOW_HIGH_AUTO = ESP_ZB_ZCL_FAN_CONTROL_FAN_MODE_SEQUENCE_LOW_HIGH_AUTO,
  PILOTWIRE_MODE_SEQUENCE_ON_AUTO = ESP_ZB_ZCL_FAN_CONTROL_FAN_MODE_SEQUENCE_ON_AUTO,
};

class ZigbeePilotWireControl : public ZigbeeEP {
  public:
    ZigbeePilotWireControl (uint8_t endpoint);
    ~ZigbeePilotWireControl() {}

    // Set the fan mode sequence value
    bool setPilotWireModeSequence (ZigbeePilotWireModeSequence sequence);

    // Use to get fan mode
    ZigbeePilotWireMode getPilotWireMode() {
      return _current_mode;
    }

    // Use to get fan mode sequence
    ZigbeePilotWireModeSequence getPilotWireModeSequence() {
      return _current_mode_sequence;
    }

    // On fan mode change callback
    void onPilotWireModeChange (void (*callback) (ZigbeePilotWireMode mode)) {
      _on_mode_change = callback;
    }

  private:
    void zbAttributeSet (const esp_zb_zcl_set_attr_value_message_t *message) override;
    //callback function to be called on fan mode change
    void (*_on_mode_change) (ZigbeePilotWireMode mode);
    void pilotWireModeChanged();

    ZigbeePilotWireMode _current_mode;
    ZigbeePilotWireModeSequence _current_mode_sequence;
};

