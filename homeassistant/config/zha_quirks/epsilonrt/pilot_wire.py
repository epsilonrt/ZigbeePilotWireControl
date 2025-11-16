"""EpsilonRT pilot wire heating module."""

import logging
logger = logging.getLogger(__name__)
logger.info("✅ EpsilonRT Pilot Wire quirk loaded for model ERT-MPZ-0X")

from zigpy.quirks import CustomCluster
from zigpy.quirks.v2 import EntityType, QuirkBuilder
import zigpy.types as t
from zigpy.zcl.foundation import BaseAttributeDefs, DataTypeId, ZCLAttributeDef

EPSILONRT = "EpsilonRT"
EPSILONRT_MANUFACTURER_ID = 4747  # Replace with your CSA ID if known
EPSILONRT_PILOT_WIRE_CLUSTER_ID = 0xFC00  # 64512
EPSILONRT_PILOT_WIRE_MODEL = "ERT-MPZ-03"

class EpsilonRTPilotWireMode(t.enum8):
    """Pilot wire mode."""
    Off = 0x00
    Comfort = 0x01
    Eco = 0x02
    FrostProtection = 0x03
    ComfortMinus1 = 0x04
    ComfortMinus2 = 0x05

class EpsilonRTPilotWireCluster(CustomCluster):
    """EpsilonRT manufacturer specific cluster to set Pilot Wire mode."""

    name: str = "PilotWireCluster"
    cluster_id: t.uint16_t = EPSILONRT_PILOT_WIRE_CLUSTER_ID
    manufacturer_id_override: t.uint16_t = EPSILONRT_MANUFACTURER_ID
    ep_attribute: str = "pilot_wire_cluster"

    class AttributeDefs(BaseAttributeDefs):
        pilot_wire_mode = ZCLAttributeDef(
            id=0x0000,
            type=EpsilonRTPilotWireMode,
            zcl_type=DataTypeId.uint8,
            is_manufacturer_specific=True,
        )
    
epsilonrt = (
    QuirkBuilder(EPSILONRT, EPSILONRT_PILOT_WIRE_MODEL)
    .replaces(EpsilonRTPilotWireCluster)
    .enum(
        attribute_name=EpsilonRTPilotWireCluster.AttributeDefs.pilot_wire_mode.name,
        enum_class=EpsilonRTPilotWireMode,
        cluster_id=EpsilonRTPilotWireCluster.cluster_id,
        entity_type=EntityType.STANDARD,
        translation_key="pilot_wire_mode",
        fallback_name="Pilot wire mode",
    )
)

epsilonrt.add_to_registry()