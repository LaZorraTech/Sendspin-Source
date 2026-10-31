# Sendspin Source

ESPHome firmware that streams an I2S ADC line input to Music Assistant as a
Sendspin source. The Music Assistant Sendspin Source plugin shows the device as
a Live Input.

**Status:** working on the board below, built with ESPHome 2026.10.1.

## Hardware

Waveshare ESP32-P4-WIFI6-DEV-KIT (ESP32-P4 v3.1, 16 MB flash, 32 MB PSRAM). WiFi
runs on the on-board ESP32-C6 through `esp32_hosted`.

| ADC signal | GPIO |
|------------|------|
| MCLK       | 3    |
| BCK        | 4    |
| DATA       | 5    |
| WS (LRCK)  | 20   |

The P4 generates all ADC clocks: MCLK = 256 x fs, BCK = 64 x fs, 48 kHz, I2S
format. Don't use GPIO6 for WS, because it also connects to the C6's IO2 on this
board.

## Build

1. Copy `secrets.yaml.example` to `secrets.yaml` and fill it in.
2. Run `esphome run esp32p4-wifi6-sendspin-source-claude2.yaml`. The first flash
   has to be over USB.

## Pairing

Music Assistant only accepts a source from a paired device. Open the device's
player settings in Music Assistant, press Setup and enter
`sendspin_pairing_code`, then press **Sendspin Pair** in Home Assistant.

## Components

- `components/sendspin`: copy of ESPHome 2026.10.0's `sendspin` component, patched
  to turn on the source role. Changes are marked "Local addition". After an
  ESPHome upgrade it must be copied from the new release and patched again.
- `components/sendspin_source`: reads the microphone (the ADC) and sends it to
  the Sendspin source role.
