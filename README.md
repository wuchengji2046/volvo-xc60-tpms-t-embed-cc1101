# Volvo XC60 TPMS Display with LILYGO T-Embed-CC1101

Volvo XC60 2012 只要胎压低报警，没有数值显示，本项目是基于 **LILYGO T-Embed-CC1101 / ESP32-S3 / CC1101 / ST7789 1.9 寸屏幕 / rtl_433_ESP** 的 Volvo XC60 2012 胎压温度数字显示项目。

This project builds a standalone TPMS digital display for a 2012 Volvo XC60 using the LILYGO T-Embed-CC1101 board, ESP32-S3, CC1101 RF receiver, ST7789 320x170 display, and rtl_433_ESP.

---

## 项目功能 / Features

- 使用 LILYGO T-Embed-CC1101 一体化硬件；
- 使用板载 CC1101 接收 433.92 MHz TPMS 信号；
- 使用 rtl_433_ESP 解码 TPMS 数据；
- 当前实测使用 `Abarth 124 Spider TPMS` 解码器可解析 Volvo XC60 2012 TPMS 数据；
- 1.9 寸 ST7789 320x170 彩屏显示四轮胎压和温度；
- 胎压单位：bar；
- 温度单位：°C；
- 支持四轮白名单 ID 显示；
- 支持历史值缓存：重新上电后先显示上次有效胎压和温度；
- 支持状态显示：
  - `WT`：等待本次启动后的新数据；
  - `OK`：本次启动后已收到有效数据；
  - `OD`：数据较旧，超过设定时间未更新。

---

## 已验证硬件 / Verified Hardware

- Board: LILYGO T-Embed-CC1101
- MCU: ESP32-S3
- RF receiver: CC1101
- Display: ST7789 1.9 inch, 320x170
- Vehicle: Volvo XC60 2012
- Development environment: VS Code + PlatformIO

---

## 重要说明 / Important Notes

本项目的关键发现是：

> Volvo XC60 2012 的 TPMS 数据可以通过 rtl_433_ESP 中的 `Abarth 124 Spider TPMS` 解码器成功解析。

This project is based on a practical compatibility finding:

> Volvo XC60 2012 TPMS packets can be decoded by the `Abarth 124 Spider TPMS` decoder in rtl_433_ESP.

不同车辆、不同年份、不同传感器批次可能存在差异。本项目只对作者当前车辆和传感器组合完成实车验证。

---

## 项目结构 / Project Structure

```text
.
├─ platformio.ini
├─ src/
│  └─ main.cpp
├─ include/
│  ├─ board_pins.h
│  ├─ display_layout.h
│  ├─ wheel_state.h
│  └─ xc60_tpms_config.h
├─ docs/
│  ├─ hardware.md
│  ├─ build-guide.md
│  ├─ vehicle-test.md
│  └─ troubleshooting.md
├─ assets/
├─ README.md
├─ LICENSE
└─ .gitignore
```

## 开发环境 / Development Environment

建议使用：

- Visual Studio Code
- PlatformIO IDE extension
- USB 数据线连接 LILYGO T-Embed-CC1101
## 编译和烧录 / Build and Upload
1. 克隆或下载项目
- git clone https://github.com/wuchengji2046/volvo-xc60-tpms-t-embed-cc1101.git

- 进入项目目录：

- cd volvo-xc60-tpms-t-embed-cc1101
2. 使用 VS Code 打开项目

- 在 VS Code 中打开整个项目文件夹，而不是单独打开 main.cpp。

3. 编译
- pio run
4. 烧录
- pio run -t upload
5. 打开串口监视器
- pio device monitor
- TPMS ID 配置

## 请在 include/xc60_tpms_config.h 中配置你的 TPMS 传感器 ID。

示例：

```cpp
// Replace these example IDs with your own TPMS sensor IDs.
static constexpr WheelConfig WHEEL_CONFIGS[4] = {
  {"LF", "your_lf_sensor_id"},
  {"RF", "your_rf_sensor_id"},
  {"LR", "your_lr_sensor_id"},
  {"RR", "your_rr_sensor_id"},
};
```

## 建议通过实车测试或靠近轮胎接收测试确认每个传感器 ID 与轮位的对应关系。
- 我是采用RTL-SDR这个USB设备在WINDOWS下测试出的ID。详见https://github.com/merbanan/rtl_433/

## 显示状态说明 / Display Status
Status	Meaning
- WT	Waiting. 正在等待本次启动后的新 TPMS 数据
- OK	Updated. 本次启动后已经收到该轮有效数据
- OD	Old. 数据超过设定时间未更新
- 当前验证结果 / Vehicle Test Result

## 本项目已完成实车验证：

- CC1101 接收链路正常；
- rtl_433_ESP 解码正常；
- Abarth 124 Spider TPMS 解码器可解析当前 Volvo XC60 2012 TPMS 数据；
- 四个白名单 ID 均可命中；
- 胎压和温度可在 T-Embed-CC1101 的 1.9 寸屏幕上显示；
- 断电重启后可显示上次有效缓存值。
## 注意事项 / Limitations
- TPMS 传感器不一定在车辆启动后立即发送数据；
- 需要行驶或等待一段时间后才可能收到四轮数据；
- 不同年份、不同车型、不同 TPMS 传感器需要重新确认解码器和 ID；
- 当前 UI 针对 LILYGO T-Embed-CC1101 的 320x170 ST7789 屏幕优化；
- 本项目不替代原车 TPMS 系统，仅作为独立显示和实验项目。
## Credits
- LILYGO T-Embed-CC1101
- rtl_433_ESP
## License

This project is released under the MIT License
