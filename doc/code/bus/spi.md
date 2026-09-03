# Bus\<BusType::SPI\> 算法文档（Spec）

> **状态：可约束实现。** 本文是 Bus 层（SPI）的算法规范。数据来源：SPI 协议语义 + STM32 HAL 环境硬件验证（Mode 0、~12MHz）。

## 1. 定位与职责

SPI 传输层，纯字节管道。只负责把字节经 SPI 外设搬进/搬出；不知道命令/数据语义（DC 归 Controller）、不操作 CS（归 Driver）、不知道分辨率。不含错误路径（假定 HAL 成功）。

## 2. 类签名与公开接口

```cpp
template <>
class Bus<BusType::SPI> {
public:
    explicit Bus(SPI_HandleTypeDef* spi);   // 构造：注入 SPI1 句柄
    void send(const uint8_t* buf, uint16_t n);   // 阻塞发送
    void read(uint8_t* buf, uint16_t n);         // 阻塞读回
};
```

- 构造注入句柄；本类**不**执行 `HAL_SPI_Init`（由使用方在 HAL 初始化阶段完成）。

## 3. 环境契约（实测配置，库不改动）

SPI 环境契约（本类的运行前提，由使用方配置）：

| 项 | 实测值 |
|---|---|
| 模式 | Master，2 线全双工 |
| 数据位 | 8 bit |
| 时钟极性/相位 | CPOL=Low、CPHA=1st Edge（SPI Mode 0） |
| 位序 | MSB first |
| NSS | 软件控制（CS 由 Driver 用 GPIO 拉） |
| 分频 | BAUDRATEPRESCALER_4（≈12MHz，验证阶段） |
| 引脚 | SCK/MOSI/MISO 接 SPI 外设（使用方接线并配置 AF 复用） |

- 先决：使用方在构造 Bus 前已完成 HAL SPI/GPIO 初始化（如 STM32Cube 生成工程）。违反 = 未定义行为。
- **类型来源与 include 约定**：`SPI_HandleTypeDef` 定义于各家族 HAL 头（如 `stm32h7xx_hal_spi.h`）、`GPIO_TypeDef` 定义于 CMSIS 设备头（如 `stm32h743xx.h`），均属使用方工程（STM32Cube）的 `Drivers/`；二者是 typedef 匿名结构体，**无法前向声明**。这两个类型名在 STM32 全家族一致（仅头文件名随家族不同）。
- **lcdriv 库头文件（伞头 `lcdriv.hpp` 与内部各层头）不 include 任何家族/系列头**（否则锁死具体系列）；改用 **include 顺序契约**：使用 lcdriv 的 TU 先让 STM32 HAL 可见（通常 include 工程 `main.h`，它按工程配置带入 `stm32h7xx_hal.h`），再 include 伞头 `lcdriv.hpp`。家族选择权留在工程侧，库在 STM32 范围内家族无关（换系列 = 换工程头，库零改动）。

## 4. 公开函数规范

### `void send(const uint8_t* buf, uint16_t n)`

- **功能**：阻塞发送 `n` 字节。等价语义：`HAL_SPI_Transmit(spi_, buf, n, HAL_MAX_DELAY)`，返回后 `buf` 已全部发出（轮询完成，无超时）。
- **边界**：`n` ≤ 65,535（uint16_t）；**`n == 0` 时无操作（不调用 HAL，直接返回）**；超长数据由调用方拆分（见 Controller spec §7）。
- **前置**：SPI1 已初始化；CS 有效（若使用 CS——本类不感知）。
- **后置**：字节已发；MISO 数据丢弃。
- **副作用**：占用 SPI1 直至完成；SCK/MOSI 产生时序。

### `void read(uint8_t* buf, uint16_t n)`

- **功能**：阻塞读回 `n` 字节到 `buf`。等价语义：`HAL_SPI_Receive(spi_, buf, n, HAL_MAX_DELAY)`（2 线全双工主模式，时钟与 MOSI 电平由 HAL 驱动；实测可正常读回面板 ID）。
- **边界**：`n` ≤ 65,535。
- **前置**：SPI1 已初始化；MISO 已接；CS 有效。
- **后置**：`buf` 填满读回数据。
- **副作用**：占用 SPI1 直至完成；总线产生 `n` 个时钟。

## 5. 当前限制 / 后续

- 当前：阻塞传输、无 DMA、无错误路径（验证环境 ~12MHz）。
- 后续（接口不变，Controller/Driver 无感）：非阻塞 DMA 传输 + busy 标志 + 双缓冲；帧缓冲需放 DMA 可达内存并处理 D-Cache（见 架构 §7）。
- 新增总线（如 I2C）：新建 `Bus<BusType::I2C>` 特化 + 本模板文档，本类不动。
