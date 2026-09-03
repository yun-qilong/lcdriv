# Bus\<BusType::SPI, P, dma\> 算法文档（Spec）

> 本文是 Bus 层（SPI）的算法规范。数据来源：SPI 协议语义 + STM32 HAL 环境硬件验证；DMA 部分补充 HAL 完成回调语义与帧缓冲内存约束。

## 1. 定位与职责

SPI 传输层：纯字节管道，只经 SPI 外设搬进/搬出字节，与 `Controller`、`PanelMgr` 同为 Driver 聚合的平行组件（架构 spec §2）。不感知命令/数据语义（由 Controller 经 DC 区分）、不感知分辨率。无错误路径（假定 HAL 成功）。

**片选释放归本类**：pushFrame 的像素段经 `sendBulk` 发出，其**传输完成点即事务收尾点**——本类在完成点调 `cs_->deselect()`（异步口味在 DMA 完成回调、阻塞口味在 sendBulk 末尾）。本类不感知"哪块屏被选中"（`deselect` 无需知道选中谁，PanelMgr 同刻至多一屏选中、全高为幂等写）。

**模板参数**：`P` = 屏数（仅为持有 `PanelMgr<P>*` 以在完成点调 `deselect`，与 LcdDriver 的 P 同义）；`dma` = 传输口味（`false`=阻塞，`true`=异步 DMA）。口味只换机制、不变语义。

## 2. 类签名与公开接口

```cpp
template <BusType bus, int P = 1, bool dma = false>
class Bus;

template <int P, bool dma>
class Bus<BusType::SPI, P, dma> {
public:
    explicit Bus(SPI_HandleTypeDef* spi, PanelMgr<P>* cs);  // 注入句柄指针 + 片选管理指针
    void send(const uint8_t* buf, uint16_t n);               // 阻塞发送（命令/参数/小块）
    void read(uint8_t* buf, uint16_t n);                     // 阻塞读回
    void sendBulk(const uint8_t* buf, uint32_t n);           // 整段；内部 ≤65535 分块
    Bus(const Bus&) = delete;            // 禁拷贝/移动：在飞传输经 active_ 槽以 this 为身份
    Bus& operator=(const Bus&) = delete;
    Bus(Bus&&) = delete;
    Bus& operator=(Bus&&) = delete;
private:
    SPI_HandleTypeDef* spi_;   // 使用方全局句柄指针（不拷贝，见 §5 为何不拷贝）
    PanelMgr<P>* cs_;          // 注入的片选管理（完成点 deselect 用）
    static inline Bus* active_;  // 活跃所有者槽（dma=true 完成路由；零初始化单指针）
    // dma=true 额外链式状态：const uint8_t* next_; uint32_t remaining_;
};
```

- 构造注入句柄**指针**（`spi_ = spi`，**不拷贝**）与 `PanelMgr<P>*`；本类不执行 `HAL_SPI_Init`。
- **禁拷贝/移动**：dma=true 在飞传输以 `active_ = this` 记身份，移动会使在飞槽悬垂；copy 与本约束一并 delete。

## 3. 环境契约

- 本类**不执行 `HAL_SPI_Init`，不配置、不校验任何 SPI 外设参数**：只要使用方提供已初始化的 SPI 句柄即可工作。时钟极性/相位、速率、引脚接线等全部由使用方决定。
- CS 须为 GPIO 软件控制（由 PanelMgr 驱动；勿配置为 SPI 硬件 NSS）；SCK/MOSI/MISO 由 SPI 外设接管。
- 先决：使用方在构造 Bus 前已完成 HAL SPI/GPIO 初始化（如 STM32Cube 生成工程，`hspi` 已 `HAL_SPI_Init`）。违反 = 未定义行为。
- **DMA 完成前提（dma=true）**：使用方须保留 CubeMX 默认的两个 IRQ 处理函数并让它们驱动同一全局句柄 `hspi`——`SPIx_IRQHandler`（`HAL_SPI_IRQHandler(&hspi)`，H7 TX DMA 完成经 EOT 中断走此路径）与 `DMAx_Streamx_IRQHandler`（`HAL_DMA_IRQHandler(&hdma_spiN_tx)`）；使用方不得绕过库用同一 `hspi` 发其他 SPI 数据（同一外设由库独占）。
- **DMA 帧缓冲前置（dma=true）**：`sendBulk` 的源缓冲必须是 **DMA 可达内存**（如 AXI SRAM 0x24000000，**不能放 DTCM**），且须保证 **D-Cache 一致性**（关 D-Cache / MPU 设 non-cacheable / 传输前 `SCB_CleanDCache`）。字节序：RGB565 高字节在前（架构 spec §4）。违反 = 传输错位/花屏。
- **类型来源与 include 约定**：`SPI_HandleTypeDef` 定义于各家族 HAL 头、`GPIO_TypeDef` 定义于 CMSIS 设备头，均属使用方工程（STM32Cube）的 `Drivers/`；二者是 typedef 匿名结构体，无法前向声明。这两个类型名在 STM32 全家族一致（仅头文件名随家族不同）。
- **lcdriv 库头不 include 任何家族/系列头**（否则锁死具体系列）；改用 include 顺序契约：使用 lcdriv 的 TU 先让 STM32 HAL 可见（通常 include 工程 `main.h`），再 include 伞头 `lcdriv.hpp`。

## 4. 公开函数规范

### `void send(const uint8_t* buf, uint16_t n)`

- **功能**：阻塞发送 `n` 字节，返回即全部发出（无超时）。行为等价 `HAL_SPI_Transmit(spi_, buf, n, 无限超时)`。**两种口味均阻塞**（命令/参数/窗口等小块必须先于像素流完成）。
- **边界**：`n` ≤ 65,535（uint16_t）；**`n == 0` 时无操作（不调用 HAL，直接返回）**。
- **前置**：SPI 已初始化；CS 有效（若使用 CS——本类不感知）。
- **后置**：字节已发；MISO 数据丢弃。
- **副作用**：占用 SPI 直至完成；SCK/MOSI 产生时序。**不释放 CS**（事务收尾只在 sendBulk 完成点，见下）。

### `void read(uint8_t* buf, uint16_t n)`

- **功能**：阻塞读回 `n` 字节到 `buf`（2 线全双工主模式）。行为等价 `HAL_SPI_Receive(spi_, buf, n, 无限超时)`。**两种口味均阻塞**（readID 为同步操作）。
- **边界**：`n` ≤ 65,535；**`n == 0` 时无操作**。
- **前置**：SPI 已初始化；MISO 已接；CS 有效。
- **后置**：`buf` 填满读回数据。
- **副作用**：占用 SPI 直至完成；总线产生 `n` 个时钟。

### `void sendBulk(const uint8_t* buf, uint32_t n)`

- **功能**：整段发送 `n` 字节，内部按**满块 65,535 拆分、末块取余**逐段连续发送，段间不插入任何命令、不改变 DC/CS。153,600 = 65,535×2 + 22,530 → 恰 3 段（65,535 / 65,535 / 22,530）；`n` ≤ 65,535 时单段。
- **完成语义（异步 A1）**：
  - `dma=true`：启动首段 DMA **立即返回**；返回 ≠ 传输完成，仅表示 CPU 可干别的。完成回调（ISR）内逐段续发，**收尾只在最后一段**（本类链式逻辑过滤）。
  - `dma=false`：内部循环阻塞委托 `send`，结束处同步收尾。
- **收尾**：两种口味在传输完成点均调 `cs_->deselect()`（拉高全部 CS + 清忙）——即 pushFrame 事务的释放点。`send` 不承担收尾。
- **边界**：**`n == 0` 时无操作（不启动传输、不收尾，与 `send` 对称）**；正常路径 `n = M*N*2 > 0`。
- **前置**：SPI 已初始化；CS 有效（事务内）；`buf` 有效；dma=true 时 `buf` 满足 §3 帧缓冲前置。
- **后置**（dma=true）：首段 DMA 已启动；后续段与收尾在完成回调异步发生。
- **副作用**：占用 SPI 直至完成（dma=true 异步占用）；完成点拉高 CS。

## 5. 完成回调路由与链式续发（dma=true）

- **为何不拷贝句柄（重要）**：H7 的 TX DMA 完成路径需经 **SPI EOT 中断**——DMA 流发完 → `SPI_DMATransmitCplt` 置 EOT 中断 → `SPIx_IRQHandler` → `HAL_SPI_IRQHandler` → 完成回调；而 CubeMX 的 `SPIx_IRQHandler` 固定驱动使用方全局 `hspi`。若本类内嵌句柄拷贝并在拷贝上启动 DMA，传输状态在拷贝上、IRQ 却驱动全局，EOT 完成回调不触发。故必须持有使用方全局句柄指针 `spi_ = &hspi`。
- **路由（活跃所有者槽）**：完成回调 `void(*)(SPI_HandleTypeDef*)` 不带用户上下文，回调拿到 `hspi == spi_` 无法直接反推 `this`。故启动传输前置 `active_ = this`，注册 `HAL_SPI_RegisterCallback(spi_, HAL_SPI_TX_COMPLETE_CB_ID, &onTxComplete)`；回调里读 `active_` 反推 `this`。传输串行 + 单物理 SPI 单 Bus，单槽够用。
- **链式续发在 ISR 内直接执行**：`onTxComplete` 中若 `remaining_ > 0`，立即 `HAL_SPI_Transmit_DMA(spi_, next_, min(remaining_, 65535))` 启动下一段；否则 `active_ = nullptr` 并调 `cs_->deselect()` 收尾。全程不阻塞、不等待，段间零间隙。
- **注册方式（C3）**：采用 `HAL_SPI_RegisterCallback` 以保持 header-only。该接口由编译宏 `USE_HAL_SPI_REGISTER_CALLBACKS == 1U` 门控（`stm32h7xx_hal_conf.h`，CubeMX 默认 `0U` 关闭）——**dma=true 前须在使用方工程把该宏置 1**。若关闭，则只能走弱回调 `HAL_SPI_TxCpltCallback`（需 .cpp 唯一定义），破坏 header-only。

## 6. 当前限制 / 后续

- 无错误路径。
- `dma=true` 的完成回调（ISR）只做 GPIO 写、清标志、启动下一段 HAL 传输，不阻塞、不等待（RTOS 安全，见 PanelMgr spec §6）。
- **范围限定**：`active_` 为单一活跃槽，意味着"系统内同一时刻至多一个 DMA 传输在飞"。共享总线串行天然满足；多条独立 SPI 总线各自并发 DMA 暂不支持——需要时把单槽升级为按句柄 keyed 的小注册表（接口不变，仅改路由内部）。
- 新增总线：新建 `Bus<BusType::I2C, P, dma>` 特化 + 对应算法文档（架构 spec §5）。
