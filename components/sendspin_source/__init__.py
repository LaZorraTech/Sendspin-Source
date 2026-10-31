"""Streams audio from an ESPHome microphone (e.g. an I2S ADC line input) to a Sendspin server.

Local addition, not part of ESPHome: it needs the patched `sendspin` component next to it, which
enables the sendspin-cpp source role (`source@v1`). Music Assistant shows the device as a Live
Input through its Sendspin Source plugin.
"""

import esphome.codegen as cg
from esphome.components import microphone, psram
from esphome.components.sendspin import (
    CONF_BUFFER_DURATION,
    CONF_CHUNK_DURATION,
    CONF_LINE_SENSE,
    CONF_SENDSPIN_ID,
    SendspinHub,
    register_source_config,
    sendspin_ns,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_CHANNELS,
    CONF_ID,
    CONF_MICROPHONE,
    CONF_NUM_CHANNELS,
    CONF_SAMPLE_RATE,
    CONF_TASK_STACK_IN_PSRAM,
)
from esphome.types import ConfigType

AUTO_LOAD = ["audio"]
DEPENDENCIES = ["microphone", "sendspin"]

CONF_SIGNAL_THRESHOLD = "signal_threshold"
CONF_SIGNAL_OFF_DELAY = "signal_off_delay"

SendspinSource = sendspin_ns.class_("SendspinSource", cg.Component)


def _register(config: ConfigType) -> ConfigType:
    register_source_config(
        {
            CONF_SAMPLE_RATE: config[CONF_SAMPLE_RATE],
            CONF_CHUNK_DURATION: config[CONF_CHUNK_DURATION].total_milliseconds,
            CONF_BUFFER_DURATION: config[CONF_BUFFER_DURATION].total_milliseconds,
            CONF_NUM_CHANNELS: len(config[CONF_MICROPHONE][CONF_CHANNELS]),
            CONF_LINE_SENSE: config[CONF_LINE_SENSE],
            CONF_TASK_STACK_IN_PSRAM: config.get(CONF_TASK_STACK_IN_PSRAM, False),
        }
    )
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SendspinSource),
            cv.GenerateID(CONF_SENDSPIN_ID): cv.use_id(SendspinHub),
            # The source role streams 16 bit PCM; one channel is mono, two are left and right.
            cv.Required(CONF_MICROPHONE): microphone.microphone_source_schema(
                min_bits_per_sample=16,
                max_bits_per_sample=16,
                min_channels=1,
                max_channels=2,
            ),
            cv.Optional(CONF_SAMPLE_RATE, default=48000): cv.int_range(
                min=8000, max=96000
            ),
            cv.Optional(CONF_CHUNK_DURATION, default="20ms"): cv.All(
                cv.positive_time_period_milliseconds,
                cv.Range(
                    min=cv.TimePeriod(milliseconds=5),
                    max=cv.TimePeriod(milliseconds=150),
                ),
            ),
            cv.Optional(CONF_BUFFER_DURATION, default="500ms"): cv.All(
                cv.positive_time_period_milliseconds,
                cv.Range(
                    min=cv.TimePeriod(milliseconds=100),
                    max=cv.TimePeriod(milliseconds=5000),
                ),
            ),
            # Line sense reports signal present/absent, which lets Music Assistant start and stop
            # the Live Input on its own. The microphone then runs all the time to watch the level.
            cv.Optional(CONF_LINE_SENSE, default=True): cv.boolean,
            cv.Optional(CONF_SIGNAL_THRESHOLD, default=-50): cv.float_range(
                min=-90, max=0
            ),
            cv.Optional(CONF_SIGNAL_OFF_DELAY, default="10s"): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_TASK_STACK_IN_PSRAM): psram.validate_task_stack_in_psram,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_on_esp32,
    _register,
)


def _final_validate(config: ConfigType) -> ConfigType:
    microphone.final_validate_microphone_source_schema(
        "sendspin_source", sample_rate=config[CONF_SAMPLE_RATE]
    )(config[CONF_MICROPHONE])
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    sendspin_hub = await cg.get_variable(config[CONF_SENDSPIN_ID])
    await cg.register_parented(var, sendspin_hub)
    cg.add(sendspin_hub.set_source_listener(var))

    mic_source = await microphone.microphone_source_to_code(config[CONF_MICROPHONE])
    cg.add(var.set_microphone_source(mic_source))

    cg.add(var.set_line_sense(config[CONF_LINE_SENSE]))
    # dBFS to a 16 bit peak amplitude.
    threshold = int(round(32767 * (10 ** (config[CONF_SIGNAL_THRESHOLD] / 20))))
    cg.add(var.set_signal_threshold(max(threshold, 1)))
    cg.add(var.set_signal_off_delay(config[CONF_SIGNAL_OFF_DELAY].total_milliseconds))
