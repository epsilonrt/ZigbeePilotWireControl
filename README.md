# ZigbeePilotWireControl
An Arduino library to control pilot wire electric heaters via Zigbee

À quoi sert configuration.yaml dans Zigbee2MQTT ?
Ce fichier est le fichier principal de configuration de Zigbee2MQTT. Il permet de :

Définir les paramètres du réseau Zigbee
Connecter Zigbee2MQTT à ton broker MQTT
Activer l’intégration avec Home Assistant
Charger des convertisseurs externes (comme ton module fil pilote)
Définir les noms des appareils Zigbee


🛠️ Où le placer ?
Tu dois le placer dans le dossier de configuration de Zigbee2MQTT, généralement :
Shell/opt/zigbee2mqtt/data/configuration.yamlAfficher plus de lignes
ou si tu utilises Docker :
Shell~/zigbee2mqtt/data/configuration.yamlAfficher plus de lignes

🧩 Et côté Home Assistant ?
Home Assistant ne lit pas directement ce fichier. Il interagit avec Zigbee2MQTT via MQTT. Grâce à ce fichier :

Zigbee2MQTT publie les entités MQTT
Home Assistant les détecte automatiquement si homeassistant: true est activé


✅ Étapes à suivre

Copie le fichier configuration.yaml dans le dossier de Zigbee2MQTT
Redémarre Zigbee2MQTT
Vérifie dans Home Assistant que ton module apparaît (via MQTT → Intégration Zigbee2MQTT)
Teste les entités : switch, select, sensor, etc.