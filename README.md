# lcdriv

裸机 LCD 驱动库，C++ 实现（禁用堆分配、异常、RTTI、虚表）。只做单帧传输，不带字体、不带 GFX。

## 目标

- **干净的驱动**：只做 `init` / `pushFrame` / `fillScreen` / `readID`
- **解耦设计**：传输（Bus）和控制器（Controller）正交，模板参数独立
- **编译期确定**：Bus、Controller、分辨率、像素格式均在编译期固定
- **零开销**：无堆、无异常、无 RTTI、无虚表

## 当前支持

- SPI + ILI9341（240×320，RGB565）
