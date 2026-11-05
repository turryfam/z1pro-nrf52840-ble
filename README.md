# Bluetooth LE firmware for the Zaopin Z1 Pro (nRF52840)

[简体中文](README.zh-CN.md)

This firmware port enables Bluetooth Low Energy (BLE) mouse input on a Zaopin Z1 Pro with a Nordic nRF52840 and a PixArt PAW3395 sensor. It uses the Nordic SoC's built-in radio to send mouse reports over BLE HID, and supports wired USB HID as well. The tested mouse's factory application offered wired USB and a proprietary 2.4 GHz receiver mode. The ZMK application adds direct pairing with a computer's Bluetooth adapter. [Nordic nRF52840 specification](https://docs.nordicsemi.com/r/bundle/ps_nrf52840/page/keyfeatures_html5.html) · [ZMK](https://github.com/zmkfirmware/zmk)

## How it works

The board port connects the PAW3395 motion sensor, button GPIOs, and wheel encoder to ZMK. ZMK and Zephyr turn those events into standard USB HID or BLE HID reports. The port combines adjacent motion reports when the button state is the same and schedules sensor reads at the reporting deadline to reduce waiting and stale motion.

The tested installation keeps two applications in flash. A small boot selector checks the ZMK image's CRC and vector table before handing control to it. A normal boot selects ZMK. Holding **DPI + rear side button** at power-on selects the factory V2.17 application. The factory USB recovery bootloader was retained on the test unit.

| Flash address | Content |
|---|---|
| `0x1C000` | Factory V2.17 application with selector at `0x1C100` |
| `0x34000`–`0x7EFFF` | Relocated ZMK application slot |
| `0x7FE00` | USB mode-switch relay |
| `0x7FFF0` | ZMK image address, size and CRC metadata |

The application addresses, GPIO assignments and recovery mechanism are specific to the tested board and factory revision. Adapting the approach to a different nRF52840 board requires mapping its inputs and sensor, checking its clocks and flash layout, and identifying a working programming or recovery path.

## Hardware results (Z1LAB004)

| Feature | Result on the tested Z1 Pro |
|---|---|
| BLE HID | Paired with Windows and macOS as `Z1PRO ZMK`. Motion, buttons and wheel work over BLE. |
| Wired USB HID | Motion, five buttons and wheel verified. The USB-preferred setting allows fallback to an available BLE connection when unplugged. |
| Bluetooth profiles | Three stored bonds, one active BLE connection. An already paired computer may reconnect first while another profile is open. |
| Factory 2.4 GHz receiver | Pairing with the original receiver failed in the tested configuration. |
| Performance | On Windows, the negotiated connection interval fell from 15 ms to 7.5 ms, with peripheral latency changing from 30 to 0 connection events. The sensor's minimum reporting interval was reduced from 8 ms to 4 ms. The tester reported improved feel on Windows and macOS. |

The connection values above are specific to the Windows session that was measured. The Mac session was assessed by feel. Power consumption and physical input-to-display latency remain unmeasured. The current sensor setting is 1600 CPI.

## Build the ZMK application

The source release contains the board module, performance patch and boot code. The dependency revisions are pinned in `west.yml`. The tested environment used Python 3.12 and Zephyr SDK 0.17.0 with the `arm-zephyr-eabi` toolchain. [Zephyr SDK releases](https://github.com/zephyrproject-rtos/sdk-ng/releases/tag/v0.17.0)

From a new workspace directory containing a clone of this repository named `z1pro-nrf52840-ble`, run the following with your Python environment active. Run the patch step once per clean dependency checkout.

```sh
python -m pip install -r z1pro-nrf52840-ble/requirements.txt
west init -l z1pro-nrf52840-ble
west update --narrow
git apply --check z1pro-nrf52840-ble/patches/low-latency.patch
git apply z1pro-nrf52840-ble/patches/low-latency.patch
python z1pro-nrf52840-ble/tools/build.py --sdk /path/to/zephyr-sdk-0.17.0
```

The resulting `z1pro-nrf52840-ble/build/zephyr/zmk.bin` is linked for `0x34000`. Change key bindings in [`firmware/config/zaopin_z1pro.keymap`](firmware/config/zaopin_z1pro.keymap) and build settings in [`firmware/config/zaopin_z1pro.conf`](firmware/config/zaopin_z1pro.conf).

## Assemble the tested application layout

The optional offline tool takes a locally obtained, decoded factory V2.17 application. It checks the exact application revision and the new ZMK entry vector, assembles the selector and relay, and writes the raw 409,600-byte region for `0x1C000`–`0x7FFFF`. Supply the raw factory application with SHA-256 `a36bf594b11375679ab061ac82745f4a43ae1b53254cf86914758a05c6e1dcc3`.

```sh
python -m pip install keystone-engine==0.9.2
python z1pro-nrf52840-ble/tools/assemble_layout.py --factory-app /path/to/local-factory-app.bin --zmk-app z1pro-nrf52840-ble/build/zephyr/zmk.bin --output z1pro-region.bin
```

The device updater expects a vendor-specific container. Reproducing the installation requires local access to the matching factory application, container tooling and recovery updater. This release provides the source and raw layout assembler. Factory images, combined binaries, container tooling and keys, device dumps, and raw input logs are kept outside this repository.

## Pairing and controls

On a flashed unit, select a free profile and pair `Z1PRO ZMK` in the host's Bluetooth settings. Hold DPI **after boot**, then press one of these buttons.

| Button | Action |
|---|---|
| Left / right | Prefer BLE / USB output |
| Rear / front side | Previous / next Bluetooth profile |
| Wheel click | Clear the current profile's bond |

If an already paired computer takes the sole connection during pairing, temporarily turn Bluetooth off on that computer while pairing the new host.

## Upstream projects

This work builds on ZMK, Zephyr, the community PAW3395 driver, and earlier mouse-porting work by Project UFA and the VGN F1/VXE R1 ZMK port. Upstream license notices and scope are recorded in [`third-party/NOTICE.md`](third-party/NOTICE.md). [Project UFA](https://github.com/aroum/ufa) · [Mouse port reference](https://github.com/greengrocer98/lariska) · [PAW3395 driver](https://github.com/greengrocer98/zmk-paw3395-driver)
