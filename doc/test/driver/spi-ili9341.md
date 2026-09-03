# TestLcdDriverSpiIli9341 case design（MT，真实三层）

> 状态：**规划中**——用例设计待 Driver 实现步设计并经 review 后填充。
> 对应：suite `tests/Mt/TestLcdDriverSpiIli9341.cpp`（真实 Bus+Controller+hal_stub）｜算法 spec：`../../code/driver/spi-ili9341.md`
> 范围：Driver 场（生命周期/CS 事务/门面转发）+ 上电序列全序 + 端到端协议真值。
