# TestPanelMgr case design（PanelMgr\<P\> UT）

> 对应算法 spec：PanelMgr spec。

## Mock 与夹具

- GPIO/延时 → hal_stub（`hal::g_transcript`：gpio 写记录 port/pin/state、delays、全局事件序）。
- 夹具：3 块屏（P=3），CS/RST 引脚取自三个 GPIO 端口对象：

```cpp
GPIO_TypeDef p0, p1, p2;
GpioPin cs[3]  = {{&p0, GPIO_PIN_0}, {&p1, GPIO_PIN_1}, {&p2, GPIO_PIN_2}};
GpioPin rst[3] = {{&p0, GPIO_PIN_8}, {&p1, GPIO_PIN_9}, {&p2, GPIO_PIN_10}};
PanelMgr<3> mgr{cs, rst};
```

## 用例表

| case 名 | 场景 | Arrange+Act | 校验（Assert） | 备注 |
|---|---|---|---|---|
| initiallyIdle | 初始空闲 | 构造后直接查 | `isBusy()==false`；gpio 记录为空 | P 编译期（无 count 查询） |
| selectLowersRequestedCs | 选中拉低对应 CS | `select(1)` | 返回 true；gpio 末条 = (p1, PIN_1, RESET)；`isBusy()==true` | PanelMgr spec §4 |
| selectRejectedWhileBusy | 忙时拒绝第二片 | `select(0)` 后 `select(2)` | 第二次返回 false；gpio 仅 1 条（未动作） | 互斥语义 |
| deselectRaisesAllCsAndFreesBus | 释放 | select(0) 后 `deselect()` | `isBusy()==false`；共 4 条 gpio：先 RESET 后 3×SET（含幂等写）；可再次 select | 释放不关心"选中了谁" |
| deselectWhenIdleRaisesAllCs | 空闲释放幂等 | 直接 `deselect()` | 3×SET；`isBusy()==false` | 全高幂等 |
| resetPulsesRstPinWithTimings | 复位脉冲 | `reset(1)` | gpio 3 条 = (p1, PIN_9) SET→RESET→SET；delays == {5,10,120}；不改变 isBusy | PanelMgr spec §4 reset 时序 |
| resetIndependentOfBusyState | 复位与忙解耦 | select(0) 后 `reset(2)` | reset 正常执行（p2/PIN_10 三条）；busy 仍 true；deselect 后 false | reset 不依赖忙状态 |

## 支撑说明

- **断言粒度 = 行为契约**：GPIO 写序列（port/pin/state）与延时序列逐条核对（对应 PanelMgr spec §4 可核对数据）。
- 刻意不测：越界 panel（未定义，无校验）；接线模式（板上物理接线，本类不感知）。
