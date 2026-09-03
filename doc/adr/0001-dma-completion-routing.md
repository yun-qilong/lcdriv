# ADR-0001：DMA 传输完成回调路由 —— 指针 + 单一活跃所有者槽

## 状态（Status）

**已接受（Accepted，2026-09-06）**。实现与维护以此为准。

## 背景（Context）

lcdriv 引入 `dma=true` 传输口味（架构 spec §3、Bus spec）：`Bus<SPI, P, true>::sendBulk` 用 `HAL_SPI_Transmit_DMA` 异步发送像素段；一帧 153,600 字节须按 HAL 单次 65,535 上限链式拆成 3 段。每段完成后要：(1) 就地续发下一段；(2) 最后一段完成后调 `PanelMgr::deselect()` 拉高 CS、清忙（事务收尾点，见 Driver spec §4 / Bus spec §4）。

核心难题：STM32 HAL 完成回调签名是 `void (*)(SPI_HandleTypeDef*)`，**只带句柄、不带用户上下文**——回调里无法直接知道"是哪个 Bus 实例发起这次传输"。

硬约束（红线）：
- header-only（无 .cpp / 无库产物）→ 不能用"弱回调需 .cpp 唯一定义"的路子；
- 无堆 / 无异常 / 无 RTTI / 无虚表 / 无动态静态初始化；
- 尽量"无全局 / 单例、依赖注入、多实例"。

## 方案（Options）与取舍

### 方案 A：拷贝句柄 + container_of

Bus 内嵌 `SPI_HandleTypeDef spi_`（值成员），在拷贝上启动 DMA；完成回调拿到 `&spi_` 后用 `container_of` 反推 Bus。

**否决**：H7 的 TX DMA 完成是**两段**——DMA 流发完后 HAL 内部 `SPI_DMATransmitCplt` 并不直接回调，而是打开 **SPI EOT 中断**，最终由 `SPIx_IRQHandler → HAL_SPI_IRQHandler` 走完收尾再回调；而 CubeMX 生成的 `SPIx_IRQHandler` **写死驱动使用方全局 `hspi`**。若在拷贝上启动传输，`spi_.State = BUSY_TX` 而全局 `hspi.State = READY`，EOT 收尾查不到忙态 → **完成回调永不触发**。要让拷贝成立，必须把 `SPIx_IRQHandler` 改指向库内句柄，而 ISR 是自由函数、要够到库对象又得再放一个全局指针——**更侵入，且没省掉全局**。

### 方案 B：指针 + 每句柄注册表

Bus 持 `SPI_HandleTypeDef* spi_`（指向全局 `hspi`），用一个按句柄 keyed 的注册表 `{hspi → Bus*}` 路由。

**不采纳（暂缓）**：正确且支持"多独立总线各自并发 DMA"，但注册表是更强的全局状态（且需固定容量上限），对当前单屏 / 单总线 / 串行传输是过度设计。保留为将来升级路径。

### 方案 C（采纳）：指针 + 单一活跃所有者槽

Bus 持 `SPI_HandleTypeDef* spi_ = &hspi`（**不拷贝**）；另设一个零初始化单槽 `static inline Bus* active_`。启动传输前置 `active_ = this`，完成回调读 `active_` 反推 `this`，再按剩余字节续发、或 `deselect()` 收尾。

## 决策（Decision）

采用 **方案 C：指针 + 单一活跃所有者槽**。

- **句柄**：持有使用方全局句柄指针（不拷贝），与 CubeMX 生成的 `SPIx_IRQHandler` / `DMAx_Streamx_IRQHandler` 天然一致。
- **路由**：`active_` 单槽（`static inline Bus*`，零初始化）。传输串行 ⇒ 单槽够用。
- **回调注册**：`HAL_SPI_RegisterCallback`（需使用方在 CubeMX 把 `USE_HAL_SPI_REGISTER_CALLBACKS` 置 1；raycaster-demo 已置 1 并实测确认）。
- **链式续发**在 ISR 内直接执行（段间零间隙）。

## 后果（Consequences）

**正面**：
- app 的 CubeMX 代码（`hspi1`、两个 IRQ handler）零改动；
- header-only 保持（`HAL_SPI_RegisterCallback` 只是普通 HAL 函数调用）；
- 零堆 / 零动态初始化（槽是零初始化的静态指针）。

**代价 / 范围限定**：
- 对"无全局 / 单例"红线做一处**最小、有界的放松**：一个 `static inline Bus*` 零初始化单槽，仅存在于 DMA 完成路由路径。
- **单槽 ⇒ 系统内同一时刻至多一个 DMA 传输在飞**（写入 Bus spec §6 范围限定）。共享总线串行天然满足；"多条独立 SPI 总线各自并发 DMA"暂不支持。
- **未来升级路径**：若需多路并发，把单槽改为按句柄 keyed 的小注册表（方案 B），**接口不变，仅改路由内部**。

## 关联决策

- `Bus<SPI, P, dma>` 模板形态（Bus 因持 `PanelMgr<P>*` 而新增 P）、`sendBulk` 完成语义（异步 A1）、`PanelMgr` 注入与 `deselect` ISR 安全、CS 拉高归 Bus（C7 消解）、`move = delete`（在飞槽以 `this` 为身份）——见架构 spec §2/§3、Bus spec、Driver spec、PanelMgr spec。
