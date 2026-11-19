# ESP32-S3-Touch-LCD-7C 固件

[English](README.md)

| 文件 | 用途 | 烧录偏移 |
| --- | --- | --- |
| [ESP32-S3-Touch-LCD-7C_FactoryOnly_20260529.bin](ESP32-S3-Touch-LCD-7C_FactoryOnly_20260529.bin) | ESP32-S3-Touch-LCD-7C（32 MB Flash）出厂固件，版本 2026-05-29 | `0x0` |
| [ESP32-S3-Touch-LCD-7C-Test.bin](ESP32-S3-Touch-LCD-7C-Test.bin) | 保留的历史测试固件，内容与出厂镜像相同 | `0x0` |

```sh
esptool --chip esp32s3 write_flash 0x0 ESP32-S3-Touch-LCD-7C_FactoryOnly_20260529.bin
```
