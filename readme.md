> 详细内容请见硬件开源项目[ESP32 灯光矩阵](https://oshwhub.com/peterbei/esp32-deng-guang-ju-zhen)。

本项目为一个由 ESP32 驱动的可编程灯板。

# 主要功能介绍

1. 支持从蓝牙和普通串口传输视频到灯板。灯板会循环播放视频。
2. 使用 ESP32 的可触摸引脚，实现亮度的无级调节和画面的暂停/播放。（尚未实现）

# 使用教程

## 硬件

硬件方面不多赘述，注意电池尽量选用容量 1000mah 以上的，WS2132B 耗电量很高。

本人不喜欢在烧录时捏着个镊子短接引脚，所以我将 GPIO0 和排针上的 GND 短接了，也就是板子在插上 USB 转 TTL 模块时 GPIO0 才会短接到地，平常则是悬空。

烧录时你只需要插上 USB 转 TTL 模块，然后重启板子，ESP32 就会自动进入烧录模式。拔下模块后再次重启就能进入正常运行状态。

## 软件

你可以前往 [https://github.com/WelFrog/ESP32-LightBoard](https://github.com/WelFrog/ESP32-LightBoard) 查看项目所有的相关代码。

你也可以在底部的附录中直接下载编译好的固件并直接烧录。

接下来分两方面讲解软件部分：

### 1. ESP32 代码

如果你想自己编译，需要安装库 ``Adafruit_NeoPixel`` 用于控制 WS2132B。

你可以在第 12 行修改全体灯珠亮度，默认 $100\%$。

代码中可能残留了一些未实现成功的触摸引脚的代码，忽略他们即可。

### 2. 上传视频用 Python 代码

相关代码在 ``.\python`` 目录下，共包含以下文件：

- Conversion.py：将视频压缩成 $8 \times 8$，并将其转换成 ESP32 可播放的 json 格式（即文件 led_data.json）；
- send.py：通过串口将 led_data.json 传输到 ESP32 上，需要设置正常的串口号和波特率，如果你想通过蓝牙传输，可以在设备管理器中查看系统给 ESP32 分配的串口号；
- example.mp4：一个爱心示例视频。

# 其它

1. 附件中文件“firmware.bin”为已经编译好的固件，你可以用[乐鑫官方提供的下载工具](https://docs.espressif.com/projects/esp-test-tools/zh_CN/latest/esp32/production_stage/tools/flash_download_tool.html)来烧录。
2. 由于 Github 访问性在部分地区比较玄学，所以附件中也提供了项目压缩包“ESP32-LightBoard-main.zip”供下载使用。