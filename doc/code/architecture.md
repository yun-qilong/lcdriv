# lcdriv 代码架构（Architecture）

> 正式架构说明（实现依据）。本文与 code/ 下各层算法 spec（bus/controller/driver）为代码的唯一实现依据：本文给出整体结构与决策，算法文档给出每个 public 函数的可核对数据。
> 协议级数据（初始化序列、读 ID 等）经硬件实测验证，数据出处见各层 spec 头部「数据来源」。

## 1. 定位与硬约束

**定位**：最小、零依赖、编译期可配置的**裸机 LCD 驱动库**（独立、通用）。只做单帧传输，不带字体与 GFX；预留扩展点（加屏 / 加总线 / 换分辨率）。

- 只做核心驱动：`init` / `pushFrame` / `fillScreen` / `readID` / `setOrientation`；不带字体、不带 GFX、不带绘图引擎。
- 核心思想：**驱动 = 传输（怎么把字节搬过去）× 控制器（该发什么、按什么顺序）**，二者正交，用模板在编译期确定。

**硬约束（红线）**：

| 约束 | 说明 |
|---|---|
| 无堆 | 不 new/malloc、无 STL 动态容器；帧缓冲由上层预分配 |
| 无异常 / 无 RTTI / 无虚表 | `-fno-exceptions -fno-rtti`；"多态" = 模板特化 + duck-typing |
| 编译期确定 | Bus、Controller、分辨率、像素格式均在编译期固定，零运行时开销 |
| header-only | 模板库：伞头 `include/lcdriv.hpp`（内部按层拆分 `include/lcdriv/{core,bus,controller,lcdDriver,panelMgr}.hpp`，声明与实现同文件——模板实现必须在头内）；C 包装后续单独 .cpp。**无任何库产物、无静态/动态链接要求**：模板在使用方 TU 编译期实例化（多 TU 重复实例化由编译器合并）；仅调用 HAL 普通函数，其由使用方工程（如 STM32Cube 生成工程）静态编入固件 |
| 碰到一个加一个 | 不追求一开始就万能；每次只引入一个变量 |

## 2. 三层架构

```
LcdDriver<BusType, ControllerType, M, N, P, dma>  ← 组合根 + CS 事务（经 PanelMgr）+ 门面
   ├── owns  Bus<BusType, P, dma>                纯字节管道：send / read / sendBulk
   ├── owns  Controller<ControllerType, M, N>    设备协议 + DC（duck-type 调 Bus）
   └── owns  PanelMgr<P>                       屏组 GPIO：每屏 {CS, RST} + 事务忙状态
```

> **聚合与互动**：Driver 是组合根，聚合三个平行组件——`Bus`（字节搬运）、`Controller`（设备协议与 DC）、`PanelMgr`（每屏 CS/RST 与互斥）。三者各司其职、互不感知对方内部；互动经引用与参数：Controller 协议方法以 Bus 为参数；Bus 构造时注入 `PanelMgr<P>*`，pushFrame 的像素段经 `Bus::sendBulk`，其**传输完成点由 Bus 调 `PanelMgr::deselect`**（异步口味在 DMA 完成回调、阻塞口味在 sendBulk 末尾），故事务的释放方是 Bus 而非 Driver（Driver spec §4）。组件各自的算法规范见下文对应文件。

职责契约（各层详细行为见对应算法文档）：

| 层 | 模板参数 | 持有 | 职责 | 知道 | 不知道 |
|---|---|---|---|---|---|
| **Bus\<BusType, P, dma\>** | BusType, P, dma | SCK/MOSI/MISO + 句柄（指针） | 字节搬运 `send`/`read`/`sendBulk`；完成点释放 CS | 无 | 命令/数据、CS（除完成点释放）、分辨率 |
| **Controller\<C, M, N\>** | ControllerType, M, N | DC 引脚 | 命令集、初始化命令表、窗口/推帧、读 ID | 设备协议 + 分辨率 M/N | Bus 具体类型、CS/RST |
| **PanelMgr\<P\>** | P（屏数，与 LcdDriver 同义） | 每屏 {CS, RST} 引脚 | 屏组片选/释放/忙 + 复位脉冲 | 屏索引与引脚 | 设备协议 |
| **LcdDriver\<B, C, M, N, P\>** | 5 个（P=屏数，默认 1） | Bus + Controller + PanelMgr | 组合根、事务起止（经 PanelMgr）、逐屏门面 | 屏索引 + 事务起止 | — |

**引脚归属**：SCK / MOSI / MISO → Bus（SPI 外设）；DC → Controller；CS / RST → PanelMgr（每屏一对）。VCC/GND 为电源；BL 背光预留（归属未定义）。

**公共类型（core）**：`GpioPin { GPIO_TypeDef *port; uint16_t pin; }` 为库级 GPIO 引脚描述（端口 + 引脚），凡组件需要注入/持有 GPIO 输出线（Controller 的 DC、PanelMgr 屏表的 CS/RST、Driver 装配入参）一律以 `GpioPin` 表达，不属于任何组件私有。基础类型（`BusType` / `ControllerType` / `IsSupported`）与模板前向声明同归 core 层；本库不设命名空间，类型置于全局作用域。

**关键机制——duck-typing**：Controller 的方法都是函数模板 `template <typename B> ... (B& bus, ...)`，只要 B 有 `send` / `read` 就能用。编译期绑定、零运行时开销；传错类型在编译期报错（无虚表、无 RTTI）。

## 3. 模板参数与特化策略

| 参数 | 语义 | 维度性质 |
|---|---|---|
| `BusType` | 物理总线（SPI / I2C / …） | 离散、封闭 → **偏特化维度** |
| `ControllerType` | 控制器芯片（ILI9341 / ST7789 / …） | 离散、封闭 → **偏特化维度** |
| `M, N` | **逻辑分辨率** = 应用帧缓冲的宽/高（如竖屏 240×320、横屏 320×240） | 连续 → **自由模板参数，不特化**；方向（MADCTL）由 M/N 相对面板编译期推导（Controller spec §4.2） |
| `P` | **屏数**（同总线同 controller，由一个 Driver 管理；PanelMgr 模板参数同用 `P` 同义，见 PanelMgr spec §2；默认 1） | 连续 → 自由模板参数，不特化 |
| `dma` | **传输口味**（`false`=阻塞，`true`=异步 DMA；默认 false） | 离散二值 → **自由模板参数**（`bool`，非特化维度）；只换机制、不变语义 |

**策略：分辨率不特化，只偏特化 bus+controller。** 一个 (bus, controller) 偏特化服务该组合的所有分辨率；`M*N*2` 等仍为编译期常量。

```cpp
enum class BusType { SPI, I2C };
enum class ControllerType { ILI9341, ST7789 };

// 主模板：不支持的组合 → 编译期报错（IsSupported 白名单）
template <BusType bus, ControllerType ctrl, int M, int N, int P = 1, bool dma = false>
class LcdDriver {
    static_assert(IsSupported<bus, ctrl>::value, "不支持的 (bus, controller) 组合");
};

// 偏特化：只固定 bus+controller，M/N/P/dma 自由（一个实现，所有分辨率/屏数/口味）
template <int M, int N, int P, bool dma>
class LcdDriver<BusType::SPI, ControllerType::ILI9341, M, N, P, dma> { /* ... */ };

// Bus 主模板与 SPI 偏特化（P 为持有 PanelMgr<P>* 所需，dma 为口味开关）
template <BusType bus, int P = 1, bool dma = false> class Bus;
template <int P, bool dma> class Bus<BusType::SPI, P, dma> { /* ... */ };
```

- 报错机制：`IsSupported` 白名单 + static_assert（依赖表达式，仅在实例化时触发）；或主模板只声明不定义。
- 逃生舱：某分辨率确实需要完全不同实现时，可用全特化覆盖偏特化（`std::vector<bool>` 同款机制）。
- 分辨率相关分支（如 ST7789 偏移寄存器）：在同一偏特化内用 `if constexpr`，不产生新特化。

## 4. 接口契约（对外门面）

| 函数 | 语义 | 备注 |
|---|---|---|
| `init(...)` | 装配 + 逐屏硬件初始化（复位 + 初始化序列）；成功返回 true | 入参 = SPI 句柄 + `GpioPin dc` + P 组 CS/RST `GpioPin` 数组（Driver spec §2） |
| `pushFrame(panel, px)` | 向第 panel 屏推一整帧（M×N×2 字节）；返回 bool：true=已上屏（缓冲交给传输）/ false=未启动（忙，缓冲仍归调用方） | 帧缓冲由上层预分配，驱动不持有；返回即缓冲交接（Driver spec §5） |
| `fillScreen(panel, color)` | 向第 panel 屏整屏填充单色 | 单事务完成（panel 语义同 pushFrame） |
| `readID(panel)` | 在第 panel 屏读芯片 ID 验证接线/型号 | 需 MISO 已接 |
| `setOrientation(panel, madctl)` | 在第 panel 屏设置方向/镜像/BGR | 只写 MADCTL 寄存器 |

> 逐屏门面以 `panel`（∈ [0, P)）指定目标屏；P = 1 时 panel 恒 0。`panel` 语义见 Driver spec §2/§4。
| `width()` / `height()` | 对外宽高 | 编译期常量 |

- **字节序（重点）**：RGB565 像素先高字节后低字节（`0xF800` → `0xF8 0x00`）。帧缓冲用字节数组、高字节在前，直接 DMA 不错序。
- **生命周期（双范式）**：① 传统 C 式——默认构造（句柄空）+ `bool init(...)`；② 推荐——`explicit LcdDriver(...)` **原子构造**（构造 = 装配 + 上电），配合调用方 `std::optional` 原地构造，不存在"构造了但未初始化"的中间态。未完成装配即调用显示接口 = 未定义行为。细节见 Driver spec §3。
- **DMA 归属 Bus 层**：`sendBulk`（dma=true 异步、dma=false 阻塞）+ 完成回调 + 链式续发；Controller / Driver 对口味无感（仍是 `send`/`sendBulk`/`read` 语义）。详见 Bus spec §4/§5。
- **DMA 帧缓冲前置（dma=true）**：`pushFrame` 的帧缓冲必须是 DMA 可达内存（如 AXI SRAM 0x24000000，**不能放 DTCM**），并保证 D-Cache 一致性（关 D-Cache / MPU 设 non-cacheable / 传输前 `SCB_CleanDCache`）；字节序按上条"高字节在前"。违反 = 传输错位/花屏。详见 Bus spec §3。

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

- 阅读顺序：`doc/README.md`（索引）→ 本文件 → 自下而上读算法 spec：`code/bus/spi.md` → `code/controller/ili9341.md` → `code/driver/spi-ili9341.md`；写测试前读 `test/architecture.md`。
- 算法 spec 模板与约定见 `doc/README.md`「约定」；新增类特化时在 code/、test/、tests/ 三处各加一个文件。

## 7. 阶段路线

1. SPI + ILI9341 阻塞传输验证「读 ID + 测试图」。
2. DMA（`dma=true` 口味：`sendBulk` 异步 + 完成回调 + 链式续发 + 完成点释放 CS；帧缓冲放 DMA 可达内存、D-Cache 处理、字节序）+ 提频（受信号完整性约束）。
