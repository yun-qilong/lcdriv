# TestLcdDriverSpiIli9341 case design（MT，真实三层）

> 对应：suite `tests/Mt/TestLcdDriverSpiIli9341.cpp`（真实 Bus+Controller+hal_stub）｜算法 spec：Driver spec
> 范围：Driver 场（生命周期/CS 事务/门面转发）+ 上电序列全序 + 端到端协议真值。

## 夹具（fixture）

```cpp
class TestLcdDriverSpiIli9341 : public ::testing::Test {
  protected:
    void SetUp() override { hal::g_transcript.reset(); }
    SPI_HandleTypeDef h1;
    GPIO_TypeDef portA, portB;
    GpioPin dc{&portA, GPIO_PIN_0};
    GpioPin cs[1] = {{&portA, GPIO_PIN_4}};
    GpioPin rst[1] = {{&portB, GPIO_PIN_1}};
};
```

## 用例表（dma=false）

| case 名（gtest 标识符） | 场景 | 校验内容（Assert） | 备注 |
|---|---|---|---|
| atomicConstructRunsInitSequence | 构造即初始化 | 首条 tx = SWRESET(0x01)；delay 序列 = reset(5,10,120) + init(120,120,...) | Driver spec §5 |
| defaultCtorThenInit | 默认构造 + init | init 返回 true；width/height 正确 | Driver spec §3 范式一 |
| pushFrameWritesPixels | 推帧协议 | tx 序列 = CASET + data + PASET + data + RAMWR；末尾 deselect(GPIO_PIN_SET) | Controller spec §5 |
| fillScreenWritesRowPattern | 单色填充 | tx = 窗口 5 条 + 320 行各 480 字节；首行 = [0xF8,0x00] 重复；末尾 deselect | Controller spec §5 |
| readIDProtocol | 读 ID | tx = 0xD3；rx n=4；返回 0x934100；末尾 deselect | Controller spec §5 |
| setOrientationWritesMadtcl | 写 MADCTL | tx = {0x36} + {0x60}；末尾 deselect | Controller spec §5 |
| multiPanelInitAndPushFrame | 双屏推帧 | P=2：pushFrame(0) 后 pushFrame(1) 各自 deselect；命令序列正确 | Driver spec §4 |
| initSequenceFullOrder | 上电序列全序 | 从 event 流提取 14 条命令字节，与 Controller spec §6 #2..#15 一致 | Driver spec §5 |

## 夹具（DMA fixture）

```cpp
class TestLcdDriverSpiIli9341Dma : public ::testing::Test {
  protected:
    void SetUp() override { hal::g_transcript.reset(); }
    void TearDown() override { EXPECT_TRUE(hal::g_transcript.delays.empty()); }
    SPI_HandleTypeDef h1;
    GPIO_TypeDef portA, portB;
    GpioPin dc{&portA, GPIO_PIN_0};
    GpioPin cs[1] = {{&portA, GPIO_PIN_4}};
    GpioPin rst[1] = {{&portB, GPIO_PIN_1}};
};
```

- DMA fixture 不检查 `gpio.empty()`（sendBulk 完成回调会产生 GPIO 写）。
- 每个用例构造 `LcdDriver<..., true>`（dma=true）。

## 用例表（dma=true）

| case 名（gtest 标识符） | 场景 | 校验内容（Assert） | 备注 |
|---|---|---|---|
| pushFrameReturnsImmediatelyAndDmaStarts | 非阻塞推帧 | pushFrame 返回 true；dmaStarts.size()==1 且首段 65535 字节；CS 保持低电平（无 deselect） | Bus spec §5 / Driver spec §5 |
| pushFrameRejectedWhileDmaInFlight | busy 门禁 | DMA 在飞时再次 pushFrame 返回 false；dmaStarts 仍为 1（无新传输） | Driver spec §5 |
| dmaChainCompletesAndDeselectsCs | 链式完成 + deselect | 3 次 fireTxDmaComplete 后 dmaStarts.size()==3（65535/65535/22530）；末尾 deselect(GPIO_PIN_SET) | Bus spec §5 |
| fillScreenIsBlockingEvenWithDma | dma 不影响 send 路径 | dma=true 的 fillScreen 不产生 DMA 调用（dmaStarts 空）；末尾 deselect(GPIO_PIN_SET) | Controller spec §5 |
