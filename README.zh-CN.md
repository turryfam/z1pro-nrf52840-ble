# 为 Zaopin Z1 Pro（nRF52840）启用蓝牙的固件方案

[English](README.md)

这个项目给采用诺迪克 nRF52840 和 PixArt PAW3395 的 Zaopin Z1 Pro 增加低功耗蓝牙（BLE）鼠标功能，同时支持有线 USB。实测鼠标的厂商程序提供有线和专用 2.4 GHz 接收器模式。本项目移植 ZMK，使用 nRF52840 内置射频发送标准 BLE HID 报告，让鼠标可以直接与电脑的蓝牙适配器配对。[Nordic nRF52840 规格书](https://docs.nordicsemi.com/r/bundle/ps_nrf52840/page/keyfeatures_html5.html) · [ZMK](https://github.com/zmkfirmware/zmk)

## 实现原理

板级配置把 PAW3395 位移传感器、按键 GPIO 和滚轮编码器接入 ZMK。ZMK 和 Zephyr 将输入转换成 USB HID 或 BLE HID 报告。位移队列会合并按键状态相同的连续报告，传感器则在到达上报时间时读取积累的位移，以减少等待和滞留。

实测方案在 Flash 中放置两个应用。启动选择器检查 ZMK 镜像的 CRC 与向量表，通过后进入 ZMK。普通开机默认使用 ZMK，按住 **DPI＋后退侧键**开机进入厂商 V2.17 应用。实验保留了原厂 USB 恢复引导程序。

| Flash 地址 | 内容 |
|---|---|
| `0x1C000` | 厂商 V2.17 应用，启动选择器位于 `0x1C100` |
| `0x34000`–`0x7EFFF` | 重定位后的 ZMK 应用槽 |
| `0x7FE00` | USB 模式切换中继 |
| `0x7FFF0` | ZMK 应用的地址、大小与 CRC 元数据 |

这里的地址、GPIO 和恢复方式对应实测的电路板和厂商版本。移植到其他 nRF52840 设备时，需要重新确认按键与传感器接线、时钟、Flash 布局及可用的编程或恢复入口。

## 实物验证（Z1LAB004）

| 功能 | 实测结果 |
|---|---|
| 蓝牙鼠标 | Windows 和 macOS 可配对 `Z1PRO ZMK`，蓝牙移动、按键和滚轮可用。 |
| 有线鼠标 | USB 移动、五键和滚轮已验证。设为 USB 优先时，拔线可切回可用的蓝牙连接。 |
| 蓝牙槽位 | 保存三组配对，同时保持一条 BLE 连接。首次配对新主机时，旧主机可能先重连。 |
| 原装 2.4 GHz 接收器 | 实测组合下对码失败。 |
| 性能优化 | Windows 实测连接间隔从 15 ms 降到 7.5 ms，外围延迟参数从 30 个连接事件降到 0。传感器最短上报间隔从 8 ms 调整为 4 ms。测试者反馈两端手感均有改善。 |

表中的连接参数来自已测的 Windows 会话。Mac 的结果来自使用手感反馈。功耗和物理输入到屏幕显示的延迟仍待测量。当前传感器设为 1600 CPI。

## 构建 ZMK 应用

源码包含板级模块、性能补丁和启动代码，依赖版本固定在 `west.yml`。实测使用 Python 3.12、Zephyr SDK 0.17.0 及其中的 `arm-zephyr-eabi` 工具链。[Zephyr SDK 下载](https://github.com/zephyrproject-rtos/sdk-ng/releases/tag/v0.17.0)

新建一个工作目录，将本仓库克隆为该目录下的 `z1pro-nrf52840-ble`，启用 Python 环境后在工作目录执行以下命令。每份干净的依赖源码只需应用一次补丁。

```sh
python -m pip install -r z1pro-nrf52840-ble/requirements.txt
west init -l z1pro-nrf52840-ble
west update --narrow
git apply --check z1pro-nrf52840-ble/patches/low-latency.patch
git apply z1pro-nrf52840-ble/patches/low-latency.patch
python z1pro-nrf52840-ble/tools/build.py --sdk /path/to/zephyr-sdk-0.17.0
```

产物 `z1pro-nrf52840-ble/build/zephyr/zmk.bin` 链接在 `0x34000`。键位见 [`firmware/config/zaopin_z1pro.keymap`](firmware/config/zaopin_z1pro.keymap)，构建选项见 [`firmware/config/zaopin_z1pro.conf`](firmware/config/zaopin_z1pro.conf)。

## 离线组装实测的双应用布局

组装工具接收本地取得、已解包的厂商 V2.17 原始应用，校验应用版本和 ZMK 入口向量，再加入选择器和切换中继。输出是用于 `0x1C000`–`0x7FFFF` 的 409,600 字节原始区域。厂商原始应用的 SHA-256 应为 `a36bf594b11375679ab061ac82745f4a43ae1b53254cf86914758a05c6e1dcc3`。

```sh
python -m pip install keystone-engine==0.9.2
python z1pro-nrf52840-ble/tools/assemble_layout.py --factory-app /path/to/local-factory-app.bin --zmk-app z1pro-nrf52840-ble/build/zephyr/zmk.bin --output z1pro-region.bin
```

实物更新器使用厂商专用容器。复现实物安装还需要本地取得匹配的厂商应用、容器工具及恢复更新器。本仓库发布源码与原始布局组装器，厂商镜像、合成二进制、厂商封装工具与密钥、设备转储及原始输入日志留在本地。

## 配对与操作

刷入后，切到一个空槽，在电脑蓝牙设置里配对 `Z1PRO ZMK`。在**已经开机**的状态下按住 DPI，再按以下按键。

| 按键 | 功能 |
|---|---|
| 左键／右键 | 优先蓝牙／有线 USB 输出 |
| 后退／前进侧键 | 上一个／下一个蓝牙槽 |
| 滚轮中键 | 清除当前槽的配对 |

配对新电脑时，如果已配对的电脑先占用了连接，可以在配对期间暂时关闭那台电脑的蓝牙。

## 上游项目

项目基于 ZMK、Zephyr 和社区 PAW3395 驱动，移植也参考了 Project UFA 及 VGN F1/VXE R1 的 ZMK 配置。上游许可声明及适用范围见 [`third-party/NOTICE.md`](third-party/NOTICE.md)。[Project UFA](https://github.com/aroum/ufa) · [鼠标移植参考](https://github.com/greengrocer98/lariska) · [PAW3395 驱动](https://github.com/greengrocer98/zmk-paw3395-driver)
