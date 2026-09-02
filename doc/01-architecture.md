# lcdriv 架构文档

> 正式架构说明（实现依据）。本文与 doc/ 下各算法文档（02–04）为代码的唯一实现依据：本文给出整体结构与决策，算法文档给出每个 public 函数的可核对数据。
> 协议级数据（初始化序列、读 ID、SPI 配置）以实测驱动为准：raycaster-demo `stm32/led_blink`（ili9341.c / main.c / .ioc），详见 doc/02 §3、doc/03 §6。

## 1. 定位与硬约束

**定位**：最小、零依赖、编译期可配置的**裸机 LCD 驱动库**。从 raycaster-demo 的驱动部分独立而来，现阶段目标是实现 raycaster-demo 所需驱动，同时预留通用化扩展点（加屏 / 加总线 / 换分辨率）。

- 只做核心驱动：`init` / `pushFrame` / `fillScreen` / `readID` / `setOrientation`；不带字体、不带 GFX、不带绘图引擎。
- 核心思想：**驱动 = 传输（怎么把字节搬过去）× 控制器（该发什么、按什么顺序）**，二者正交，用模板在编译期确定。

**硬约束（红线）**：

| 约束 | 说明 |
|---|---|
| 无堆 | 不 new/malloc、无 STL 动态容器；帧缓冲由上层预分配 |
| 无异常 / 无 RTTI / 无虚表 | `-fno-exceptions -fno-rtti`；"多态" = 模板特化 + duck-typing |
| 编译期确定 | Bus、Controller、分辨率、像素格式均在编译期固定，零运行时开销 |
| header-only | 模板库：伞头 `include/lcdriv.hpp`（内部按层拆分 `include/lcdriv/{core,bus,controller,lcddriver}.hpp`，声明与实现同文件——模板实现必须在头内）；C 包装后续单独 .cpp。**无任何库产物、无静态/动态链接要求**：模板在使用方 TU 编译期实例化（多 TU 重复实例化由编译器合并）；仅调用 HAL 普通函数，其由 CubeMX 工程静态编入固件 |
| 碰到一个加一个 | 不追求一开始就万能；每次只引入一个变量 |

## 2. 三层架构

```
LcdDriver<BusType, ControllerType, M, N>         ← 组合根 + CS 事务 + 门面
   ├── owns  Bus<BusType>                        纯字节管道：send / read
   └── owns  Controller<ControllerType, M, N>    设备协议 + DC/RST/BL（duck-type 调 Bus）
```

职责契约（各层详细行为见对应算法文档）：

| 层 | 模板参数 | 持有 | 职责 | 知道 | 不知道 |
|---|---|---|---|---|---|
| **Bus\<BusType\>** | BusType | SCK/MOSI/MISO + 句柄 | 字节搬运 `send`/`read`（含后续 DMA） | 无 | 命令/数据、CS、分辨率 |
| **Controller\<C, M, N\>** | ControllerType, M, N | DC / RST（+BL）引脚 | 命令集、初始化序列、窗口/推帧、读 ID | 设备协议 + 分辨率 M/N | Bus 具体类型 |
| **LcdDriver\<B, C, M, N\>** | 4 个 | Bus + Controller + CS 引脚 | 组合根、CS 事务起止、对外门面（width/height 等） | 哪块屏 + 事务起止 | — |

**引脚归属**：SCK / MOSI / MISO → Bus（SPI 外设）；DC / RST → Controller；CS → Driver。VCC/GND 为电源；BL 背光可选（若走 GPIO 归 Controller）。

**关键机制——duck-typing**：Controller 的方法都是函数模板 `template <typename B> ... (B& bus, ...)`，只要 B 有 `send` / `read` 就能用。编译期绑定、零运行时开销；传错类型在编译期报错（无虚表、无 RTTI）。

## 3. 模板参数与特化策略

| 参数 | 语义 | 维度性质 |
|---|---|---|
| `BusType` | 物理总线（SPI / I2C / …） | 离散、封闭 → **偏特化维度** |
| `ControllerType` | 控制器芯片（ILI9341 / ST7789 / …） | 离散、封闭 → **偏特化维度** |
| `M, N` | **逻辑分辨率** = 应用帧缓冲的宽/高（如竖屏 240×320、横屏 320×240） | 连续 → **自由模板参数，不特化**；方向（MADCTL）由 M/N 相对面板编译期推导（doc/03 §4.2） |

**策略：分辨率不特化，只偏特化 bus+controller。** 一个 (bus, controller) 偏特化服务该组合的所有分辨率；`M*N*2` 等仍为编译期常量。

```cpp
enum class BusType { SPI, I2C };
enum class ControllerType { ILI9341, ST7789 };

// 主模板：不支持的组合 → 编译期报错（IsSupported 白名单）
template <BusType bus, ControllerType ctrl, int M, int N>
class LcdDriver {
    static_assert(IsSupported<bus, ctrl>::value, "不支持的 (bus, controller) 组合");
};

// 偏特化：只固定 bus+controller，M/N 自由（一个实现，所有分辨率）
template <int M, int N>
class LcdDriver<BusType::SPI, ControllerType::ILI9341, M, N> { /* ... */ };
```

- 报错机制：`IsSupported` 白名单 + static_assert（依赖表达式，仅在实例化时触发）；或主模板只声明不定义。
- 逃生舱：某分辨率确实需要完全不同实现时，可用全特化覆盖偏特化（`std::vector<bool>` 同款机制）。
- 分辨率相关分支（如 ST7789 偏移寄存器）：在同一偏特化内用 `if constexpr`，不产生新特化。

## 4. 接口契约（对外门面）

| 函数 | 语义 | 备注 |
|---|---|---|
| `init(...)` | 装配 + 硬件初始化（复位 + 初始化序列）；成功返回 true | 入参 = SPI 句柄 + CS/DC/RST 端口/引脚（doc/04 §2） |
| `pushFrame(px)` | 推一整帧（M×N×2 字节） | 帧缓冲由上层预分配，驱动不持有 |
| `fillScreen(color)` | 整屏填充单色 | |
| `readID()` | 读芯片 ID 验证接线/型号 | 需 MISO 已接 |
| `setOrientation(madctl)` | 设置方向/镜像/BGR | 只写 MADCTL 寄存器 |
| `width()` / `height()` | 对外宽高 | 编译期常量 |

- **字节序（重点）**：RGB565 像素先高字节后低字节（`0xF800` → `0xF8 0x00`）。帧缓冲用字节数组、高字节在前，直接 DMA 不错序。
- **生命周期（已定，双范式）**：① 传统 C 式——默认构造（句柄空）+ `bool init(...)`；② 推荐（raycaster-demo 用）——`explicit LcdDriver(...)` **原子构造**（构造 = 装配 + 上电），配合调用方 `std::optional` 原地构造，不存在"构造了但未初始化"的中间态。未完成装配即调用显示接口 = 未定义行为。细节见 doc/04 §3。
- **DMA 归属 Bus 层**：`transmitDMA` + busy 标志 + 双缓冲；Controller / Driver 无感（仍是 `send`/`read` 语义）。

## 5. 支持矩阵与扩展路径

| | SPI | I2C |
|---|---|---|
| **ILI9341** | ✅ 当前（240×320 / RGB565） | 待加 |
| **ST7789** | 待加 | — |

| 动作 | 要写的东西 |
|---|---|
| 加一款屏 | `IsSupported` 一行特化 + Controller 偏特化 + LcdDriver 偏特化 + 一篇算法文档 |
| 加一种总线 | Bus 特化 + 对应组合的 LcdDriver 偏特化 + 一篇算法文档；**Controller 协议不用动**（duck-type） |
| 换分辨率 | **什么都不用写**——M/N 是自由参数，偏特化自动匹配 |

## 6. 文档导航

- 阅读顺序：`doc/README.md`（索引）→ 本文件 → 自下而上读算法文档：`02-bus-spi` → `03-controller-ili9341` → `04-lcddriver-spi-ili9341` → `05-ut-design`。
- 算法文档模板与约定见 `doc/README.md`「约定」；新增类特化时在 doc/ 登记。

## 7. 阶段路线

1. **阶段一**：SPI + ILI9341，12MHz 阻塞传输，验证「读 ID + 测试图」。
2. **阶段二**：DMA（帧缓冲放 AXI SRAM、D-Cache 处理、busy 标志、双缓冲）+ 提频（24 → 30 → 40MHz，受杜邦线信号完整性约束）。
