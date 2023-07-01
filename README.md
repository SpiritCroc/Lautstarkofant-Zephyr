# Lautstarkofant

A project for Arduino Nano 33 BLE to implement a bluetooth volume/media controller.  
Work-in-progress rewrite of [mbed-based Lautstarkofant](https://github.com/SpiritCroc/Lautstarkofant), using [Zephyr](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/zephyr/boards/arm/arduino_nano_33_ble/doc/index.html).

## Compile and flash

From your [Zephyr workspace](https://docs.zephyrproject.org/latest/develop/getting_started/index.html):

```
> west build -p always -b arduino_nano_33_ble /path/to/lautstarkofant
> west flash --bossac=/path/to/bossac
```

## Roadmap

- [x] LEDs
- [x] Media keys
- [x] Reset button to disconnect
- [x] Persistent paing, which can be cleared using reset button
- [ ] Code cleanup
    - The code was more or less copied over from the sample code linked below, and the old mbed-based Lautstarkofant code, and would benefit from some cleanup and restructuring
- [ ] Page Down/Up buttons
    - mbed-based Lautstarkofant had these, but I haven't used them much, so we'll see if it's worth to port if I miss it later
    - Would need updating the `report_map` and `hid_key_report` data structures, similar to [mbed-ble-hid](https://github.com/SpiritCroc/mbed-ble-hid)

## Wiring scheme

- Status-LED: D9
- Action-LED: D10
- Reset-button on back of the device: D12
- Volume-down button (bottom-left): D3
- Volume-up button (bottom-right): D5
- Functionality-TBD button top-right: D4
- Functionality-TBD button top-right: D6

Customize in `app.overlay`.

## Acknowledgements

Builds on [zephyr](https://github.com/zephyrproject-rtos/zephyr) sample code, released under the Apache-2.0 license.
