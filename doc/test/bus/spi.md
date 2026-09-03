# TestBusSPI case design（Bus\<BusType::SPI\> UT）

> 状态：**v3（已随实现落地）**。对应 suite：`tests/Bus/TestBusSPI.cpp`；算法 spec：`../../code/bus/spi.md`。

## 夹具（fixture）

```cpp
class TestBusSPI : public ::testing::Test {
  protected:
    void SetUp() override   { hal::g_transcript.reset(); }
    void TearDown() override {
        EXPECT_TRUE(hal::g_transcript.gpio.empty());   // 不变量：Bus 不碰 GPIO
        EXPECT_TRUE(hal::g_transcript.delays.empty()); // 不变量：Bus 不产生延时
    }
    SPI_HandleTypeDef h1, h2;
};
```

- **suite 级不变量放 TearDown**：任何用例结束后 GPIO/delays 都必须为空（取代逐用例"不碰 GPIO"，覆盖所有路径含 read/零长）。
- 读回内容经 `hal::g_transcript.rxPreset` 预置（跨调用 FIFO 消费，取尽补 0）。
- 观测点：`hal::g_transcript.tx / rx / gpio / delays / events`；`events` 为**全局调用序**（'T'/'R'/'G'/'D' + 对应列表序号），供跨类型顺序断言。

## 用例表

| case 名（gtest 标识符） | 场景 | 步骤（Arrange+Act） | 校验内容（Assert） | 备注 |
|---|---|---|---|---|
| ctorStoresHandle | 构造后发送落在注入句柄 | `Bus<BusType::SPI> bus(&h1);` `bus.send({0xAA},1)` | `tx` 恰 1 条；`tx.back().h==&h1`；`bytes=={0xAA}` | Bus spec §2 |
| sendForwardsSingleByte | 单字节逐字转发 | `send({0x01},1)` | `tx.size()==1`；`bytes=={0x01}` | Bus spec §4 |
| sendForwardsLongBlock | 长数据整段一次转发 | 30000 字节 v，`send(v,30000)` | `tx.size()==1`；`bytes==v`（全量逐字节） | Bus spec §4 |
| sendAcceptsMaxLength | uint16 上限单次合法 | 65535 字节 v，`send(v,65535)` | `tx.size()==1`；`bytes.size()==65535` 且 `bytes==v` | Bus spec §4；>65535 类型不可表示 |
| sendZeroLengthNoOp | n==0 不触碰 HAL | `send(nullptr,0)` | `tx` 为空 | Bus spec §4；回归：防 0 长误调 HAL |
| sendRecordsPerCallInOrder | 每次调用一条记录、顺序保持 | `send(a,1); send(b,1)` | `tx.size()==2`；顺序 `{1}`、`{2}` | Bus spec §4 |
| sendFromBufferOffset | 指针偏移只发 n 字节 | v={0,1,2,3}；`send(v+1,2)` | `bytes=={1,2}` | 指针语义（corner） |
| sendLeavesBufferUntouched | const 语义：发送不改调用方 buf | v 100 字节；快照后 `send(v.data(),100)` | `v == 快照` | 不变量（实现经 const_cast 给 HAL） |
| readFillsFromPreset | 读回内容与调用长度 | rxPreset={0x93,0x41,0x00}；`read(buf,3)` | `rx` 恰 1 条、`rx.back().n==3`；`buf=={0x93,0x41,0x00}` | Bus spec §4 |
| readConsumesPresetInOrder | 多次读跨调用 FIFO | rxPreset={1,2,3,4}；`read(a,2); read(b,2)` | `a=={1,2}`、`b=={3,4}`；`rx.size()==2` | 支撑契约：预置按序消费 |
| readZeroLengthNoOp | read 的 n==0 无操作（与 send 对称） | `read(buf,0)` | `rx` 为空 | Bus spec §4；覆盖实现 guard |
| readFillsZeroWhenPresetExhausted | 读长于预置 → 补 0 | rxPreset={0x93}；`read(buf,3)` | `buf=={0x93,0x00,0x00}`；`rx.back().n==3` | 支撑契约：取尽补 0 |
| mixedOpsKeepHandleAndOrder | 多实例交替：句柄归属 + **全局顺序** | `Bus a(&h1), b(&h2);` `a.send(x,1); b.send(y,1); a.read(z,1)` | `events == [T0, T1, R0]`（跨类型顺序）；`tx[0].h==&h1`、`tx[1].h==&h2`、`rx[0].h==&h1` | Bus spec §2；顺序经全局 events 断言（防"先读后发"类实现错误） |

## 支撑说明

- `events` 全局序列由 hal_stub 维护：每次 HAL 调用记 `{kind, 对应列表序号}`；Controller/MT 的 readID 等"先命令后读"顺序断言同样依赖它。
- 刻意不测（spec 未定义）：`send/read(nullptr, n>0)`（使用方前置保证）；HAL 内部时序/超时（替身不可测）。

## 实现要点

- include：`support/hal_stub.hpp` 与 `lcdriv.hpp` 分两个 include 块（空行隔开）。
- 注册：`lcdriv_add_ut(TestBusSPI SOURCES Bus/TestBusSPI.cpp support/hal_stub.cpp)`。
