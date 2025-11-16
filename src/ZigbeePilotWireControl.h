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

// Set manufacturer and model name
// Home Assistant Zigbee integration uses these values to auto-detect the device type
// and assign the correct icon and features.
// If you change these values, you may need to modify the integration settings accordingly
// in homeassistant/config/zha_quirks/epsilonrt/pilot_wire.py
#define PILOT_WIRE_MANUF_NAME   "EpsilonRT"
#define PILOT_WIRE_MODEL_NAME   "ERT-MPZ-03"
//
#define PILOT_WIRE_CLUSTER_ID   0xFC00 // must be upper or equal than 0xFC00 for manufacturer-specific clusters
#define PILOT_WIRE_MODE_ATTR_ID 0x0000
#define PILOT_WIRE_MANUF_CODE   0x1234 // Used by Home Assistant to control custom cluster, can be replaced with official code if available but should not work, so do not touch!!

// Custom Arduino-friendly enums for pilot wire mode values
enum ZigbeePilotWireMode : uint8_t {
  PILOTWIRE_MODE_OFF = 0,
  PILOTWIRE_MODE_COMFORT,
  PILOTWIRE_MODE_ECO,
  PILOTWIRE_MODE_FROST_PROTECTION,
  PILOTWIRE_MODE_COMFORT_MINUS_1,
  PILOTWIRE_MODE_COMFORT_MINUS_2
};

const ZigbeePilotWireMode PILOTWIRE_MODE_MIN = PILOTWIRE_MODE_OFF;
const ZigbeePilotWireMode PILOTWIRE_MODE_MAX = PILOTWIRE_MODE_COMFORT_MINUS_2;
const uint8_t PILOTWIRE_MODE_COUNT = (PILOTWIRE_MODE_COMFORT_MINUS_2 - PILOTWIRE_MODE_OFF + 1);

class ZigbeePilotWireControl : public ZigbeeEP {
  public:
    ZigbeePilotWireControl (uint8_t endpoint, bool enableMetering = false, bool enableTemperature = false);
    ~ZigbeePilotWireControl() {}


    ZigbeePilotWireMode pilotWireMode() const {
      return  static_cast<ZigbeePilotWireMode> (_current_mode);
    }

    bool powerState() const {
      return _current_state;
    }

    // On fan mode change callback
    void onPilotWireModeChange (void (*callback) (ZigbeePilotWireMode mode)) {
      _on_mode_change = callback;
    }

    void begin();
    bool setPilotWireMode (ZigbeePilotWireMode mode); // TODO: not functional yet

    // for debugging purpose
    void printClusterInfo (Print &out = Serial);
  
  protected:
    void zbAttributeSet (const esp_zb_zcl_set_attr_value_message_t *message) override;

  private:
    void pilotWireModeChanged();
    void powerStateChanged();

    uint8_t _current_mode;
    uint8_t _state_on_mode;
    void (*_on_mode_change) (ZigbeePilotWireMode mode);

    bool _current_state;
};

