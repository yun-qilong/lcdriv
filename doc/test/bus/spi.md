# TestBusSPI case design（Bus\<BusType::SPI, P, dma\> UT）

> 对应 suite：`tests/Bus/TestBusSPI.cpp`；算法 spec：Bus spec。

## 夹具（fixture）

```cpp
class TestBusSPI : public ::testing::Test {
  protected:
    void SetUp() override { hal::g_transcript.reset(); }

    SPI_HandleTypeDef h1, h2;
    GPIO_TypeDef p0, p1;
    GpioPin cs[2]  = {{&p0, GPIO_PIN_0}, {&p1, GPIO_PIN_1}};
    GpioPin rst[2] = {{&p0, GPIO_PIN_8}, {&p1, GPIO_PIN_9}};
    PanelMgr<2> mgr{cs, rst};   // Bus 构造注入 PanelMgr 指针（完成点 deselect 用）
};
```

- Bus 模板为 `Bus<BusType::SPI, P, dma>`：用例按 `Bus<SPI, 2, false>`（阻塞）与 `Bus<SPI, 2, true>`（DMA）分别构造，`Bus(&h1, &mgr)`。
- **`send`/`read` 不碰 GPIO、不产生延时**（这两个不变量仅对 send/read 成立）；`sendBulk` 在完成点调 `mgr.deselect()` → 产生"CS 全高 + 忙清"的 GPIO 写，属预期副作用，单独断言。
- 读回内容经 `hal::g_transcript.rxPreset` 预置（跨调用 FIFO 消费，取尽补 0）。
- 观测点：`hal::g_transcript.tx / rx / gpio / delays / events`；`events` 为**全局调用序**（'T'/'R'/'G'/'D' + 对应列表序号）。
- dma=true：hal_stub 补 `HAL_SPI_Transmit_DMA`（记录每段启动）与完成事件触发（供链式/收尾断言）。

## 用例表

| case 名（gtest 标识符） | 场景 | 步骤（Arrange+Act） | 校验内容（Assert） | 备注 |
|---|---|---|---|---|
| ctorStoresHandle | 构造后发送落在注入句柄 | `Bus<SPI,2,false> bus(&h1,&mgr);` `bus.send({0xAA},1)` | `tx` 恰 1 条；`tx.back().h==&h1`；`bytes=={0xAA}` | Bus spec §2 |
| sendForwardsSingleByte | 单字节逐字转发 | `send({0x01},1)` | `tx.size()==1`；`bytes=={0x01}` | Bus spec §4 |
| sendForwardsLongBlock | 长数据整段一次转发 | 30000 字节 v，`send(v,30000)` | `tx.size()==1`；`bytes==v` | Bus spec §4 |
| sendAcceptsMaxLength | uint16 上限单次合法 | 65535 字节 v，`send(v,65535)` | `tx.size()==1`；`bytes.size()==65535` 且 `bytes==v` | Bus spec §4 |
| sendZeroLengthNoOp | n==0 不触碰 HAL | `send(nullptr,0)` | `tx` 为空 | Bus spec §4 |
| sendRecordsPerCallInOrder | 每次调用一条记录、顺序保持 | `send(a,1); send(b,1)` | `tx.size()==2`；顺序 `{1}`、`{2}` | Bus spec §4 |
| sendFromBufferOffset | 指针偏移只发 n 字节 | v={0,1,2,3}；`send(v+1,2)` | `bytes=={1,2}` | 指针语义 |
| sendLeavesBufferUntouched | const 语义：发送不改 buf | v 100 字节；快照后 `send` | `v == 快照` | 不变量 |
| readFillsFromPreset | 读回内容与调用长度 | rxPreset={0x93,0x41,0x00}；`read(buf,3)` | `rx` 恰 1 条、`n==3`；`buf=={0x93,0x41,0x00}` | Bus spec §4 |
| readConsumesPresetInOrder | 多次读跨调用 FIFO | rxPreset={1,2,3,4}；`read(a,2); read(b,2)` | `a=={1,2}`、`b=={3,4}` | 支撑契约 |
| readZeroLengthNoOp | read 的 n==0 无操作 | `read(buf,0)` | `rx` 为空 | Bus spec §4 |
| readFillsZeroWhenPresetExhausted | 读长于预置 → 补 0 | rxPreset={0x93}；`read(buf,3)` | `buf=={0x93,0x00,0x00}` | 支撑契约 |
| mixedOpsKeepHandleAndOrder | 多实例交替：句柄归属 + 全局顺序 | 两实例 send/read 交替 | `events` 顺序正确；各记录 `h` 归属对应实例 | Bus spec §2 |
| sendBulkChunksAt65535 | sendBulk 分块（dma=false） | `sendBulk(px,153600)` | 3 次 send（65535/65535/22530）连续；末段后 `deselect`（gpio CS 全高 + busy 清） | Bus spec §4 |
| sendBulkSingleChunk | n<65535 单块 | `sendBulk(px,48000)` | 1 次 send + deselect | Bus spec §4 |
| sendBulkZeroNoOp | n==0 无操作（对称） | `sendBulk(nullptr,0)` | 无 send、无 deselect | Bus spec §4 边界 |
| sendBulkDmaStartsAndReturns | dma=true 启动即返回 | `Bus<SPI,2,true>` `sendBulk(px,153600)` | 返回时仅首段 `HAL_SPI_Transmit_DMA` 记 1 条（65535），未 deselect、busy 仍真 | Bus spec §4（异步实证） |
| sendBulkDmaChainsThenDeselectLast | dma=true 链式续发 + 收尾 | 模拟完成事件 ×3 | 前 2 次各续发一段（第 2、3 段 DMA 启动），第 3 次才 deselect；共 3 段、1 次收尾 | Bus spec §5 |

## 支撑说明

- **发送粒度即 spec 契约**（Bus spec §4：sendBulk 满块 65,535 拆分、完成点 deselect、n==0 无操作）——本文按该粒度断言；spec 若改粒度，本文与用例须同步。
- 刻意不测：DMA 实际时序（替身不可测）；CS 有效（归 Driver）；PanelMgr 互斥（归 TestPanelMgr）。
