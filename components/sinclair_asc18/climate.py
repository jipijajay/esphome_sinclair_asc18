import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate, uart
from esphome.const import CONF_ID, CONF_UART_ID

AUTO_LOAD = ["uart"]

sinclair_ns = cg.esphome_ns.namespace("sinclair_asc18")
SinclairASC18Climate = sinclair_ns.class_(
    "SinclairASC18Climate",
    climate.Climate,
    uart.UARTDevice,
    cg.Component,
)

CONFIG_SCHEMA = climate.CLIMATE_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(SinclairASC18Climate),
        cv.GenerateID(CONF_UART_ID): cv.use_id(uart.UARTComponent),
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await climate.register_climate(var, config)

    uart_comp = await cg.get_variable(config[CONF_UART_ID])
    cg.add(var.set_uart_parent(uart_comp))
