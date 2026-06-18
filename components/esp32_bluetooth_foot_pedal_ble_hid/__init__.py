import esphome.codegen as cg
import esphome.config_validation as cv

from esphome.const import CONF_ID

AUTO_LOAD = ["esp32"]
CODEOWNERS = []

CONF_BLE_DEVICE_NAME = "ble_device_name"
CONF_DEFAULT_KEY_NAME = "default_key_name"

esp32_bluetooth_foot_pedal_ble_hid_ns = cg.esphome_ns.namespace("esp32_bluetooth_foot_pedal_ble_hid")
Esp32BluetoothFootPedalBleHid = esp32_bluetooth_foot_pedal_ble_hid_ns.class_(
    "Esp32BluetoothFootPedalBleHid", cg.Component
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(Esp32BluetoothFootPedalBleHid),
        cv.Optional(CONF_BLE_DEVICE_NAME, default="esp32-bluetooth-foot-pedal"): cv.string_strict,
        cv.Optional(CONF_DEFAULT_KEY_NAME, default="F13"): cv.string_strict,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_ble_device_name(config[CONF_BLE_DEVICE_NAME]))
    cg.add(var.set_default_key_name(config[CONF_DEFAULT_KEY_NAME]))
