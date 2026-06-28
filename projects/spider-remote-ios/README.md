# SpiderRemote iOS App

SwiftUI + CoreBluetooth 遥控 App，连接固件广播名 **SpiderBod**。

## 要求

- Mac + **Xcode 15+**
- iPhone / **iPad** **iOS 15.0+**（含 15.8.x；**iPad BLE 遥控已验证**）
- Apple ID（免费 Personal Team 即可）

## 新手请读完整教程

**从未做过 iPhone 开发、部署、测试**，请按顺序阅读：

**[docs/ios-getting-started.md](docs/ios-getting-started.md)**

内容包括：Xcode 安装、Apple ID 签名、USB 连接 iPhone、信任开发者、蓝牙权限、连接 SpiderBod、与串口并行联调、FAQ。

## 快速打开工程

```bash
# 在 ble-projects 根目录
open projects/spider-remote-ios/SpiderRemote.xcodeproj
```

Xcode 中选真机 → **Signing & Capabilities** 选 Team → **⌘R** 运行。

## 协议

与固件 `projects/spider/include/spider_ble_protocol.h` 对齐；详见 `projects/spider/docs/ble-remote-design.md`。

**遥控 Tab** 覆盖固件全部 Motion（stand / splay / lift×4 / 趴下×3 / 步态×6 / nod·shake·roll / wave×4）+ Stop。  
**速度 Tab** 覆盖两种 speed（预设 + 自定义滑条）。
