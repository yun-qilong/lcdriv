# LcdDriver\<BusType::SPI, ControllerType::ILI9341, M, N\> 算法文档（Spec）

> **状态：可约束实现。** 本文是 Driver 层（组合根 + CS 事务 + 门面）的算法规范。数据来源：doc/02（Bus）、doc/03（Controller）、实测 `raycaster-demo/stm32/led_blink`（main.c / ili9341.c / .ioc）。

## 1. 定位与职责

- 面向用户的唯一入口（raycaster-demo 直接使用本类）。
- 职责：装配三层（Bus + Controller + CS 引脚）、为每次公开操作建立 CS 事务、对外暴露门面接口与宽高。
- 本类是总线/控制器组合的**具体偏特化**：`LcdDriver<BusType::SPI, ControllerType::ILI9341, M, N>`；其他组合走主模板 `static_assert` 拦截（编译期报错）。
- M/N = **逻辑分辨率**（应用帧缓冲列/行，doc/03 §4.1）；方向（MADCTL）由 M/N 编译期推导（doc/03 §4.2），当前项目目标为横屏（M > N 实例）。

## 2. 类签名与公开接口

```cpp
template <int M, int N>
class LcdDriver<BusType::SPI, ControllerType::ILI9341, M, N> {
public:
    // ── 范式一：传统 C 式（先构造 → 后显式初始化）──
    LcdDriver() = default;                    // 空构造：句柄全空（未挂载）
    bool init(SPI_HandleTypeDef* spi,                     // SPI1 句柄（CubeMX 已初始化）
              GPIO_TypeDef* cs_port, uint16_t cs_pin,     // 片选（低有效，归 Driver）
              GPIO_TypeDef* dc_port, uint16_t dc_pin,     // 命令/数据（低=命令，归 Controller）
              GPIO_TypeDef* rst_port, uint16_t rst_pin);  // 复位（低有效，归 Controller）

    // ── 范式二：构造即初始化（推荐，raycaster-demo 使用）──
    explicit LcdDriver(SPI_HandleTypeDef* spi,
                       GPIO_TypeDef* cs_port, uint16_t cs_pin,
                       GPIO_TypeDef* dc_port, uint16_t dc_pin,
                       GPIO_TypeDef* rst_port, uint16_t rst_pin);

    // ── 门面接口（两种范式共用）──
    void pushFrame(const uint8_t* px);    // 推一整帧（M*N*2 字节，RGB565 高字节在前）
    void fillScreen(uint16_t color);
    uint32_t readID();
    void setOrientation(uint8_t madctl);
    int width()  const;                   // = M（编译期）
    int height() const;                   // = N（编译期）
};
```

- **入参即句柄，不引入配置结构体**（与 Bus/Controller 各层构造风格一致：`Bus(spi)`、`Controller(dc_port, dc_pin, rst_port, rst_pin)`）。参数顺序固定为：SPI 句柄 → CS 端口/引脚 → DC 端口/引脚 → RST 端口/引脚，与引脚归属（§6）一致；调用方按此顺序传参（位置参数的传错序风险由顺序文档约束，调用点通常仅 1~2 处）。
- 两种范式使用同一套装配逻辑与门面接口：`init(...)` 与 `LcdDriver(...)` 行为等价（读入句柄 + 上电序列 doc/03 §6），区别只在"何时触发"。

> 类型说明：`SPI_HandleTypeDef`/`GPIO_TypeDef` 为 STM32 HAL/CMSIS 类型（定义于 CubeMX 工程 `Drivers/`，非本库定义，typedef 匿名结构体无法前向声明；类型名在 STM32 全家族一致）。库头文件**不 include 家族头**（避免锁死具体系列），采用 **include 顺序契约**：使用方 TU 先 include 工程 `main.h` 再 include 伞头 `lcdriv.hpp`（详见 doc/02 §3）。入参的绑定面是"STM32（任一家族）HAL"；非 STM32 平台才需改这些参数类型（模板参数仍为通用化轴）。

## 3. 生命周期：两种初始化范式（已定决策）

### 范式一：先构造，后显式初始化（对接传统 C 写法）

```cpp
LcdDriver<BusType::SPI, ControllerType::ILI9341, 240, 320> lcd;   // 空构造：句柄全空
lcd.init(&hspi1, GPIOA, GPIO_PIN_4, GPIOB, GPIO_PIN_0, GPIOB, GPIO_PIN_1);
lcd.pushFrame(px);
```

- 空构造把全部句柄置空（未挂载任何硬件）。`init(...)` 把句柄读入本地成员并完成上电，成功返回 `true`。
- 阶段一**无硬件自检**（`init` 恒返回 true 是可接受的实现；`bool` 为将来错误路径预留）；接线/型号验证靠 `readID` 由调用方比对。
- **代价（本范式的固有风险，需文档明示）**：存在"已构造、未 init"的中间态；未 init 就调用显示接口 = 未定义行为（静默黑屏）。

### 范式二：构造即初始化（推荐，raycaster-demo 使用）

```cpp
// 存储处：optional 占位——内存空间预留，对象尚未构造
std::optional<LcdDriver<BusType::SPI, ControllerType::ILI9341, M, N>> lcd_;

// 构造点：原地构造，构造 = 读入句柄 + 上电，原子完成
lcd_.emplace(&hspi1, GPIOA, GPIO_PIN_4, GPIOB, GPIO_PIN_0, GPIOB, GPIO_PIN_1);

// 之后访问必然已初始化
lcd_->pushFrame(px);
```

- **原子性**：`emplace(...)` 调 `LcdDriver(...)`，构造函数内读入全部句柄并完成上电序列；构造结束即完全可用。
- **不存在"构造了但没初始化"的中间态**：对象未构造（optional 空）时无法访问到"半初始化"实例——空 optional 上调用 `operator->`/`operator*` 立即 UB（崩溃/断言），而不是静默黑屏。"没初始化"必然表现为"没构造"。
- `std::optional` 为值语义、内联存储、无堆，符合红线。
- 若要在运行期重建（重新上电）：先 `lcd_.reset()` 再 `lcd_.emplace(...)`，原子性保持。

### 公共前置条件（两范式统一）

- 任何显示接口（pushFrame/fillScreen/readID/setOrientation）的前置 = **已完成装配与上电**（范式一：`init` 成功；范式二：构造完成）。违反 = 未定义行为。
- 入参有效：`spi` 非空；引脚已由 CubeMX 配置为输出；调用方保证 HAL（SPI1/GPIO）已初始化。
- 上电可重复（每次完整硬件复位 + 序列，幂等）。

## 4. CS 事务策略（已定决策）

- **CS 引脚归 Driver，低有效**；空闲（非事务期间）保持高电平。
- **一次公开操作 = 一个 CS 事务**：`init`/构造上电、`pushFrame`、`fillScreen`、`readID`、`setOrientation` 各自在"CS 拉低 → 执行 → CS 拉高"之内完成，事务期间 CS 全程有效。
- 推论：`pushFrame` 的 153,600 字节像素流（多次 `bus.send`）与上电的长延时（≈380ms）都发生在 CS 有效期间——ILI9341 允许（CS 为纯使能；实测驱动为命令级 CS，与本策略字节协议等价，均已验证可用）。
- 本类内部的 CS 电平操作为唯一例外（其他层不碰 CS）。

## 5. 公开函数规范

### 装配（`init(...)` 与 `LcdDriver(...)` 共用行为）

- **功能**：完成一次完整上电初始化。读入句柄：装配总线（SPI 句柄）与控制器（DC/RST 引脚）、保存 CS 引脚并置空闲高；随后在单事务内执行上电序列（硬件复位 + doc/03 §6 命令表，阻塞总耗时 ≈380ms）。
- `init` 返回 `true`；`LcdDriver(...)` 构造完成后对象即处于同一状态。
- **前置**：HAL 已初始化；入参有效（见 §3）。
- **后置**：屏幕可显示（若接线正确）；MADCTL 已按 M/N 推导（doc/03 §4.2）。
- **边界**：只支持 SPI + ILI9341；其他组合编译期报错。`spi == nullptr` 等非法输入未定义（阶段一无校验）。

### `void pushFrame(const uint8_t* px)`

- **功能**：单事务内推送一整帧。事务序列 = 0x2A 全屏列窗口（应用空间 0..M-1）→ 0x2B 全屏行窗口（0..N-1）→ 0x2C + `M*N*2` 字节像素（协议细节、分块与终止语义见 doc/03 `pushFrame`）。
- **参数**：`px` = `M*N*2` 字节帧缓冲（上层预分配；驱动不持有、不复制、不改写）。
- **前置**：已完成装配与上电；`px` 长度正确。
- **后置**：屏幕显示 `px`。
- **性能边界**：153,600 字节 @ ~12MHz ≈ **102ms/帧（≈10fps）**；实现不得引入逐像素级低效路径（本函数为阻塞调用）。

### `void fillScreen(uint16_t color)`

- **功能**：单事务内整屏填充单色（doc/03 `fillScreen`）。
- **前置**：已完成装配与上电。
- **后置**：所有像素变为 `color`。

### `uint32_t readID()`

- **功能**：单事务内读芯片 ID（doc/03 `readID` 协议），返回组合值（ILI9341 期望 `0x9341??`，实测 `0x934100`）。
- **前置**：已完成装配与上电；MISO 已接。
- **校验**：驱动不校验，由调用方比对 `0x9341` 段。

### `void setOrientation(uint8_t madctl)`

- **功能**：单事务内写 0x36（MADCTL）（doc/03 `setOrientation`）。
- **注意**：正常使用不调用（方向已由模板参数在装配期推导）；本函数仅运行时覆盖/调试用。

### `int width() const` / `int height() const`

- **功能**：返回编译期逻辑分辨率常量 M / N（如横屏实例 M=320、N=240）。
- **说明**：与应用帧缓冲一致（doc/03 §4）。

## 6. 引脚归属与接线（实测基准）

| 信号 | 归属层 | 实测引脚 | 对应入参 |
|---|---|---|---|
| SCK / MOSI / MISO | Bus | PA5 / PA7 / PA6 | 不进参数（SPI1 外设接管，含于 `spi` 句柄） |
| CS | Driver（本类） | PA4 | `cs_port` / `cs_pin` |
| DC | Controller | PB0 | `dc_port` / `dc_pin` |
| RST | Controller | PB1 | `rst_port` / `rst_pin` |

## 7. 当前限制 / 修订记录

- 无错误路径（阶段一）；无背光控制；阻塞传输 12MHz（DMA 属 Bus 层后续，本类接口与事务语义不变）。
- 当前硬件实测实例为竖屏 240×320（MADCTL 0x00）；**横屏实例（M > N）的 MADCTL 值待实测锁定**（doc/03 §4.2），锁定后同步本表。
- **修订记录**：
  1. 生命周期定为**双范式**：范式一"默认构造 + `bool init(...)`"（传统 C 式），范式二"`explicit LcdDriver(...)` 原子构造 + `std::optional` 原地构造"（推荐，raycaster-demo 用）。原稿只有范式一。
  2. 明确 CS 事务粒度 = 一次公开操作一个事务（原稿未定死）。
  3. **入参形式（本版）**：取消 `SpiConfig` 配置结构体，改为直接接收句柄入参（SPI 句柄 + CS/DC/RST 端口/引脚），与 Bus/Controller 构造风格一致（原稿为结构体聚合）。
  4. M/N 语义明确为**逻辑分辨率**，方向由模板参数推导（原稿按物理面板表述）。
