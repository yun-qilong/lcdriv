# LcdDriver\<BusType::SPI, ControllerType::ILI9341, M, N, P, dma\> 算法文档（Spec）

> 本文是 Driver 层（组合根：聚合 Bus + Controller + PanelMgr，经 PanelMgr 建立事务，对外暴露逐屏门面）的算法规范。数据来源：Bus spec、Controller spec、PanelMgr spec。

## 1. 定位与职责

- 面向使用方的唯一入口。
- **管理同一条 SPI 总线上、同 controller 类型的 P 块屏**：一个 Driver 聚合 Bus + Controller + PanelMgr，PanelMgr 提供各屏的 CS/RST 与互斥；P = 1 时即"单屏 Driver"，与原形态等价。
- 职责：装配（逐屏上电）、为每次针对某屏的公开操作建立 CS 事务（经 PanelMgr）、对外暴露逐屏门面与宽高。
- 本类是总线/控制器组合的**具体偏特化**：`LcdDriver<BusType::SPI, ControllerType::ILI9341, M, N, P, dma>`；其他组合走主模板 `static_assert` 拦截（编译期报错）。`dma`（默认 `false`）= 传输口味：`false`=阻塞（现状），`true`=异步 DMA；口味只换机制、不变门面语义。
- M/N = **逻辑分辨率**（应用帧缓冲列/行，Controller spec §4.1）；P = **屏数**（同一 Driver 下的屏，共用总线与 DC，CS/RST 各自独立）。方向（MADCTL）由 M/N 编译期推导（Controller spec §4.2），当前项目目标为横屏（M > N 实例）。

## 2. 类签名与公开接口

```cpp
template <int M, int N, int P = 1, bool dma = false>
class LcdDriver<BusType::SPI, ControllerType::ILI9341, M, N, P, dma> {
public:
    // 编译期常量：帧字节数 M*N*2（240×320 与 320×240 均为 153600）
    static constexpr int kBytes = M * N * 2;

    // 生命周期：默认构造 + 双范式初始化；不可拷贝、不可移动（见 §3）
    LcdDriver() = default;
    LcdDriver(const LcdDriver&) = delete;
    LcdDriver& operator=(const LcdDriver&) = delete;
    LcdDriver(LcdDriver&&) = delete;
    LcdDriver& operator=(LcdDriver&&) = delete;

    // ── 范式一：传统 C 式（先构造 → 后显式初始化）──
    bool init(SPI_HandleTypeDef* spi,             // SPI 句柄（使用方已初始化，P 屏共用；库持指针）
              GpioPin dc,                         // 命令/数据（低=命令，归 Controller，P 屏共用）
              const GpioPin (&cs)[P],             // 每屏片选（低有效，归 PanelMgr）
              const GpioPin (&rst)[P]);           // 每屏复位（低有效，归 PanelMgr）

    // ── 范式二：构造即初始化（推荐）──
    explicit LcdDriver(SPI_HandleTypeDef* spi,
                       GpioPin dc,
                       const GpioPin (&cs)[P],
                       const GpioPin (&rst)[P]);

    // ── 门面接口（两种范式共用；panel ∈ [0, P)，指定目标屏）──
    bool pushFrame(int panel, const uint8_t* px);  // 推一整帧（M*N*2 字节，RGB565 高字节在前）
    void fillScreen(int panel, uint16_t color);
    uint32_t readID(int panel);
    void setOrientation(int panel, uint8_t madctl);
    int width()  const;   // = M（编译期）
    int height() const;   // = N（编译期）
};
```

- 入参即句柄，不引入配置结构体：SPI 句柄 → `GpioPin dc` → P 组 {CS, RST} `GpioPin` 数组（顺序与引脚归属 §6 一致）。所有 GPIO 线统一用库级公共 `GpioPin`（`GPIO_TypeDef* + pin`，见 架构 spec 公共类型）。
- 两种范式使用同一套装配逻辑与门面接口；区别只在"何时触发"（见 §3）。
- `pushFrame` 返回 `bool`（**缓冲交接契约**，见 §5）：`true` = 已上屏（缓冲交给传输，不能再碰，换另一块渲染）；`false` = 未启动（忙，缓冲仍归调用方，可继续用同一块）。

> 类型说明：`SPI_HandleTypeDef`/`GPIO_TypeDef` 为 STM32 HAL/CMSIS 类型（定义于使用方工程，STM32Cube，非本库定义）。库头**不 include 家族头**，采用 include 顺序契约（使用方 TU 先让 HAL 可见，详见 Bus spec §3 / PanelMgr spec）。

## 3. 生命周期：两种初始化范式

### 范式一：先构造，后显式初始化（对接传统 C 写法）

```cpp
GpioPin dc{GPIOB, GPIO_PIN_0};               // 命令/数据线（归 Controller）
GpioPin cs[1] = {{GPIOB, GPIO_PIN_4}};       // 屏 0 片选（归 PanelMgr）
GpioPin rst[1] = {{GPIOB, GPIO_PIN_1}};      // 屏 0 复位（归 PanelMgr）
LcdDriver<BusType::SPI, ControllerType::ILI9341, 320, 240, 1> lcd;   // 空构造
lcd.init(&hspi1, dc, cs, rst);
lcd.pushFrame(0, px);
```

- 空构造把全部句柄置空。`init(...)` 读入句柄并对每块屏完成上电，成功返回 `true`。
- **无硬件自检**（`init` 恒返回 true 可接受；`bool` 为将来错误路径预留）；接线/型号验证靠 `readID` 由调用方比对。
- **代价（固有风险）**：存在"已构造、未 init"中间态；未 init 就调用显示接口 = 未定义行为（静默黑屏）。

### 范式二：构造即初始化（推荐）

```cpp
std::optional<LcdDriver<BusType::SPI, ControllerType::ILI9341, M, N, P>> lcd_;
lcd_.emplace(&hspi1, dc, cs, rst);   // 构造 = 读入句柄 + 逐屏上电，原子完成（dc/cs/rst 声明见范式一）
lcd_->pushFrame(0, px);
```

- **原子性**：`emplace(...)` 构造结束即完全可用；不存在"构造了但未初始化"的中间态。
- `std::optional` 值语义、内联存储、无堆，符合红线；运行期重建（重新上电）= `lcd_.reset()` + `emplace(...)`。

### 公共前置条件（两范式统一）

- 任何显示接口的前置 = 已完成装配与上电（范式一 `init` 成功；范式二构造完成）；违反 = 未定义行为。
- 入参有效：`spi` 非空；DC/CS/RST 引脚已由使用方配为输出；调用方保证 HAL（SPI/GPIO）已初始化。
- 上电可重复（每次逐屏完整复位 + 序列，幂等）。
- **不可移动（move = delete）**：dma=true 在飞传输以 `Bus::active_` 槽记录 `this` 为身份（Bus spec §5）；移动会使在飞槽悬垂，故 LcdDriver 与 Bus 均禁移动。

## 4. 事务策略（经 PanelMgr）

- **一次针对某屏的公开操作 = 一个 CS 事务**：`init`/构造上电（逐屏各自事务）、`pushFrame(panel)`、`fillScreen(panel)`、`readID(panel)`、`setOrientation(panel)` 各自在"该屏被选中 → 执行 → 释放"的区间内完成；事务期间该屏 CS 有效。
- **事务的选中与释放（两条路径）**：
  - **pushFrame**：选中经 `PanelMgr::select(panel)`；**释放由 Bus 在 `sendBulk` 传输完成点执行**（dma=true 在 DMA 完成回调、dma=false 在 sendBulk 末尾，见 Bus spec §4）。Driver 对 pushFrame 不手动释放。
  - **fillScreen / readID / setOrientation**：同步操作，选中经 `select(panel)`，执行协议后由 **Driver 调 `PanelMgr::deselect`** 释放（这些操作不经 sendBulk）。
- **互斥由 PanelMgr 保证**：同一时刻至多一块屏被选中；`select` 失败（忙）时 `pushFrame` 返回 `false`；同步接口（fillScreen/readID/setOrientation）前置 = 无异步传输在飞（使用方保证总线空闲）。
- panel 越界 = 未定义（无运行时校验；调用方保证 `panel ∈ [0, P)`）。

## 5. 公开函数规范

### 装配（`init(...)` 与 `LcdDriver(...)` 共用行为）

- **功能**：完成一次完整上电初始化。读入句柄：装配总线（SPI 句柄）、控制器（`GpioPin dc`）、PanelMgr（P 组 CS/RST `GpioPin` 数组）；随后**逐屏上电，每屏一个 CS 事务**，顺序固定：选中该屏（PanelMgr spec `select`）→ 硬件复位（PanelMgr spec `reset`）→ 初始化命令表（Controller spec §6 #2..#15）→ 释放该屏（PanelMgr spec `deselect`）。
- `init` 返回 `true`；构造完成即处于同一状态。
- **前置**：HAL 已初始化；入参有效（§3）。
- **后置**：全部 P 块屏可显示（若接线正确）；MADCTL 已按 M/N 推导（Controller spec §4.2）。
- **边界**：只支持 SPI + ILI9341；其他组合编译期报错。非法输入未定义（无入参校验）。

### `bool pushFrame(int panel, const uint8_t* px)`

- **功能**：向第 panel 块屏推送一整帧（协议序列见 Controller spec `pushFrame`），全程处于该屏的 CS 事务内。
- **参数**：`panel` = 目标屏；`px` = `M*N*2` 字节帧缓冲（上层预分配；驱动不持有、不复制、不改写）。
- **返回（缓冲交接契约）**：`true` = 已上屏，传输已开始读该缓冲（dma=true 同步启动），调用方须换另一块缓冲渲染；`false` = 未上屏（事务忙），缓冲未被接管，可继续用同一块渲染。latest-wins：A 帧传输期间往 B 缓冲渲多帧，只送 B 最新帧。
- **前提**：`true` 时传输同步启动（缓冲指针即刻被捕获）；渲染目标不得是正在被传输读的缓冲。dma=true 时 `px` 须满足 Bus spec §3 帧缓冲前置。
- **前置**：已完成装配与上电；`panel` 有效；`px` 长度正确。

### `void fillScreen(int panel, uint16_t color)`

- 向第 panel 块屏整屏填充单色（Controller spec `fillScreen`）；单事务完成。
- 前置：已完成装配与上电；`panel` 有效。

### `uint32_t readID(int panel)`

- 在第 panel 块屏的 CS 事务内读芯片 ID（Controller spec `readID` 协议），返回组合值（ILI9341 期望 `0x9341??`，实测 `0x934100`）。
- 前置：已完成装配与上电；MISO 已接；`panel` 有效。校验由调用方比对 `0x9341` 段。

### `void setOrientation(int panel, uint8_t madctl)`

- 在第 panel 块屏的 CS 事务内写 0x36（MADCTL）（Controller spec `setOrientation`）。
- 注意：正常使用不调用（方向已编译期推导）；仅运行时覆盖/调试用。

### `int width() const` / `int height() const`

- 返回编译期逻辑分辨率 M / N（所有屏相同，Controller spec §4）。

## 6. 引脚归属（语义约定，无具体板级接线）

| 信号 | 归属层 | 对应入参 |
|---|---|---|
| SCK / MOSI / MISO | Bus | 不进参数（SPI 外设接管，含于 `spi` 句柄） |
| DC | Controller | `GpioPin dc`（P 屏共用） |
| CS / RST（每屏） | PanelMgr | `cs[P]` / `rst[P]` 数组 |

> 引脚号由使用方接线并在构造时注入（本库不绑定任何具体 GPIO）；同线复用 = 数组重复存同一 GPIO。

## 7. 当前限制

- 无错误路径（假定硬件正常）；无背光控制；`dma=false` 同步阻塞、`dma=true` 异步 DMA（机制见 Bus spec）。
- 竖屏（240×320，MADCTL 0x00）已硬件验证；横屏（M > N）MADCTL 待实测锁定（Controller spec §4.2）。
