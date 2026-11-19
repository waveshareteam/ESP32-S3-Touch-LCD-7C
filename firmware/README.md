# ESP32-S3-Touch-LCD-7C firmware

[简体中文](README_ZH.md)

| File | Purpose | Flash offset |
| --- | --- | --- |
| [ESP32-S3-Touch-LCD-7C_FactoryOnly_20260529.bin](ESP32-S3-Touch-LCD-7C_FactoryOnly_20260529.bin) | Factory firmware for ESP32-S3-Touch-LCD-7C (32 MB Flash), version 2026-05-29 | `0x0` |
| [ESP32-S3-Touch-LCD-7C-Test.bin](ESP32-S3-Touch-LCD-7C-Test.bin) | Archived test firmware with the same contents as the factory image | `0x0` |

```sh
esptool --chip esp32s3 write_flash 0x0 ESP32-S3-Touch-LCD-7C_FactoryOnly_20260529.bin
```
