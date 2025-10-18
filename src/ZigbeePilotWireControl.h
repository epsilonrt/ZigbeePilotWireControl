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

#define PILOT_WIRE_CLUSTER_ID 0xFC10
#define PILOT_WIRE_ATTR_MODE  0x0000
#define MANUFACTURER_CODE     0x1234 // Replace with your registered code if available

// Custom Arduino-friendly enums for fan mode values
enum ZigbeePilotWireMode {
  PILOTWIRE_MODE_OFF = 0,
  PILOTWIRE_MODE_COMFORT,
  PILOTWIRE_MODE_ECO,
  PILOTWIRE_MODE_FROST_PROTECTION,
  PILOTWIRE_MODE_COMFORT_MINUS_1,
  PILOTWIRE_MODE_COMFORT_MINUS_2
};


class ZigbeePilotWireControl : public ZigbeeEP {
  public:
    ZigbeePilotWireControl (uint8_t endpoint);
    ~ZigbeePilotWireControl() {}

    ZigbeePilotWireMode getPilotWireMode() {
      return _current_mode;
    }

    // On fan mode change callback
    void onPilotWireModeChange (void (*callback) (ZigbeePilotWireMode mode)) {
      _on_mode_change = callback;
    }

  private:
    void zbAttributeSet (const esp_zb_zcl_set_attr_value_message_t *message) override;
    void (*_on_mode_change) (ZigbeePilotWireMode mode);
    void pilotWireModeChanged();

    ZigbeePilotWireMode _current_mode;
};

