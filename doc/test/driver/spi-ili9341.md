# TestLcdDriverSpiIli9341 case design（MT，真实三层）

> 用例设计待 Driver 实现步设计并经 review 后填充。
> 对应：suite `tests/Mt/TestLcdDriverSpiIli9341.cpp`（真实 Bus+Controller+hal_stub）｜算法 spec：Driver spec
> 范围：Driver 场（生命周期/CS 事务/门面转发）+ 上电序列全序 + 端到端协议真值。

## DMA（dma=true）MT 规划（随 DMA 实现步填充）

- **pushFrame 非阻塞实证**：`pushFrame(panel, px)` 返回 true 后首段 DMA 已启动、CS 仍低（忙），完成事件触发后才 deselect（CS 高、忙清）。
- **busy 门禁**：上一帧在飞时再次 `pushFrame` → 返回 false、无新传输启动、CS/缓冲不被触碰。
- **CS 由完成路径 release**：阻塞口味（dma=false）在 sendBulk 末尾 release；DMA 口味（dma=true）在完成回调 release——两条释放路径各自验证。
- **缓冲交接契约**：true → 换缓冲；false → 复用同一缓冲（结合 latest-wins 语义断言）。
- **链式段数端到端**：153600 帧 → 3 段 DMA，收尾仅最后一段。
- **编译期口味实证**：`dma=false` 不产生任何 DMA 调用（`HAL_SPI_Transmit_DMA`/`HAL_SPI_RegisterCallback` 零调用）。
