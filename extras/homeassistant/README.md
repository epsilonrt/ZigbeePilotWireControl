# Zigbee Pilot Wire Control Home Assistant Integration

This integration allows Home Assistant to control pilot wire heating systems via Zigbee using a custom cluster and attribute for setting the heating mode.

## Features
- Custom Zigbee cluster for pilot wire control
- Manufacturer-specific attribute for heating mode
- Callback mechanism for state changes
- Cluster information printing for debugging
- 
## Installation
1. Ensure you have a Zigbee coordinator compatible with Home Assistant.
2. Add the Zigbee Pilot Wire Control integration to your Home Assistant configuration.
3. Restart Home Assistant to apply the changes.

To use the integration, you must configure HA to recognize the custom cluster, add theses lines to your `configuration.yaml`:

```yaml
zha:
  database_path: /config/zigbee.db
  enable_quirks: true
  custom_quirks_path: /config/zha_quirks/
```

Adapt the path as necessary.

Then, the epsilonrt directory containing the custom cluster code should be placed in the specified `custom_quirks_path`.

Restart Home Assistant again.