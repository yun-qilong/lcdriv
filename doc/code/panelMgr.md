# PanelMgr\<P\> 算法文档（Spec）

> PanelMgr 管理同一条 SPI 总线上"屏组"的片选（CS）与复位（RST）GPIO：CS 决定"此刻哪块屏采样总线"（互斥），RST 提供每屏独立复位。数据来源：多屏共用总线拓扑（各屏 CS/RST 独立，DC 归 Controller spec，SPI 总线归 Bus spec）。

## 1. 定位与职责

**在架构中的位置（平行组件）**：本类与 `Bus`、`Controller` 同为 **Driver 聚合的三个平行组件**——三者职责正交、无上下级关系：

| 组件 | 管什么 | 对应 spec |
|---|---|---|
| `Bus` | 字节搬运（send/read） | Bus spec |
| `Controller` | 设备协议 + DC（命令/数据态） | Controller spec |
| `PanelMgr` | 每屏的 CS/RST 与互斥 | 本 spec |

协作经引用与参数（Controller 方法以 Bus 为参数；事务的"释放"可由注入本类引用的传输完成路径触发，见 Driver spec §4），各自不感知对方内部。整体结构与聚合关系见 架构 spec §2、Driver spec。

**职责范围**：
- 一个 PanelMgr 对应**一个物理 SPI 总线（一个互斥组）**：同一时刻至多一块屏被选中（CS 低）。独立 SPI 时每屏一个 PanelMgr（P == 1），自然退化；跨总线绝不能共用一个（释放操作会误动其他总线的线）。
- 屏按索引 `0..P-1` 寻址（P 编译期确定）；每屏一对 `{CS, RST}` 引脚（低有效）。**同一 GPIO 可出现在多个屏的条目中**（多屏共线）：本类按索引操作，不关心物理是否同线。
- **职责划分**：CS（选中/释放）→ `select` / `deselect` / `isBusy`；RST（复位脉冲，装配期一次性）→ `reset`。不持有/操作 DC（归 Controller spec）、SPI 总线（归 Bus spec）。

## 2. 类签名与公开接口

```cpp
// P = 屏数（非类型模板参数，编译期确定；引脚数组按 P 精确分配）
// GpioPin：库级公共类型（GPIO 端口 + 引脚，见 架构 spec 公共类型），非本组件私有
template <int P>
class PanelMgr
{
  public:
    // 构造：注入 P 组 {CS, RST} 引脚（长度均为 P 的数组，类型为公共 GpioPin）
    PanelMgr(const GpioPin (&cs)[P], const GpioPin (&rst)[P]);

    // 查询：当前是否有屏被选中（事务进行中）
    [[nodiscard]] bool isBusy() const;

    // 尝试选中第 panel 块屏；忙（已有屏被选中）时返回 false 且不动作
    [[nodiscard]] bool select(int panel);

    // 释放：结束当前事务（所有屏回到未选中状态）
    void deselect();

    // 对第 panel 块屏执行硬件复位脉冲（装配期一次性使用）
    void reset(int panel);
};
```

- 模板参数用 `P`（屏数），与 LcdDriver 的 `P` 同义；`M/N` 全库仅表示分辨率（架构 spec §3），不在本类出现。

## 3. 引脚与环境契约

| 信号 | 语义 |
|---|---|
| CS[panel] | 低 = 选中第 panel 块屏（采样使能）；同一时刻至多一个 CS 低 |
| RST[panel] | 低 = 复位第 panel 块屏（**不受 CS 门控**，拉即复位） |
| DC / SPI | 归 Controller / Bus（本类不碰） |

- **互斥**：同组内同一时刻至多一块屏被选中；"忙" = 已有屏被选中。
- RST **不参与互斥**：`reset` 不依赖、不改变事务忙状态——事务内外均可调用，由调用方决定。**装配流程将复位置于该屏的 CS 事务内**（顺序见 Driver spec §5）；两块屏同时复位不会总线冲突。
- 环境先决：使用方已完成 HAL GPIO 初始化并把相关引脚配为输出；`GPIO_TypeDef` / HAL 函数经 include 顺序契约可见（同 Bus/Controller spec）。
- **调用上下文约束**：`deselect` 可能在 DMA 完成回调（中断上下文）中被调用 → 该操作不得阻塞、只做电平与状态变更（ISR 安全）。`reset` 含毫秒级延时（`HAL_Delay`），仅在任务上下文（装配期）调用。

**接线模式**（板上物理接线决定本类如何被使用；本类按索引操作、不感知接线）：

| 模式 | 屏数建模 | 可观测行为 |
|---|---|---|
| 独立 CS（默认） | `P` = 屏数，每屏一条独立 {CS, RST} | 逐屏寻址：一次 `pushFrame(panel, ...)` 只更新第 panel 屏，各屏内容独立 |
| 并联 CS（同显） | 多块屏 CS 在板上物理并联于同一 GPIO → 建模为**一条** CS 条目（`P` 不增加） | 一次事务全部屏同时接收、显示相同内容 |

- 同一 GPIO 允许重复出现在多个条目（见 §1），但**勿用重复条目模拟同显**：重复条目与并联在 CS 电气上等价（同刻同低），可观测差异只在 **RST 是否逐物理屏**——需要"共享 CS、各屏独立复位"时才用重复条目；纯同显用并联建模（单条目）。

## 4. 函数规范

### `bool isBusy() const`

- 查询当前是否有屏被选中（事务进行中）。无副作用。

### `bool select(int panel)`

- **功能**：尝试选中第 panel 块屏（拉低其 CS）。
- **返回**：成功选中 → `true`；已有屏被选中（忙）→ `false`，且不改变任何引脚状态。
- **前置**：`panel ∈ [0, P)`；调用方将在事务内完成协议操作并以 `deselect` 收尾。

### `void deselect()`

- **功能**：结束当前事务——所有屏回到未选中状态（CS 拉高），忙状态清除。
- **语义**：本操作不需要知道"本次选中了谁"（同刻至多一片被选中，未选中的屏置高为幂等写）。
- **调用上下文**：任务上下文或中断上下文（见 §3 约束）。

### `void reset(int panel)`

- **功能**：对第 panel 块屏执行一次硬件复位脉冲（装配期一次性；低有效复位）。装配流程在选中该屏（CS 事务内）后调用，见 Driver spec §5。
- **时序**（毫秒级阻塞延时，数值为硬件验证结果）：

  | 步 | 动作 | 延时 |
  |---|---|---|
  | 1 | RST 拉高（先确保高） | 5 ms |
  | 2 | RST 拉低（复位） | 10 ms |
  | 3 | RST 拉高（释放复位） | 120 ms |

- **前置**：`panel ∈ [0, P)`；仅任务上下文调用（含阻塞延时）。
- **约束**：不依赖、不改变事务忙状态。

## 5. 边界与约束

- `P ≥ 1`（编译期约束）；越界 panel = 未定义（无运行时校验）。
- 无堆、无异常、无虚表。
- 无错误路径（假定接线正确）。

## 6. 并发与生命周期约束

- 使用模型为**单调用方串行**（一个事务结束后才发起下一个）。该模型下 `select` 的"查忙 + 选中"无需额外互斥保护（裸机单循环天然无竞争）。
- **busy = 传输在飞**：`select` 置位（主循环写 true）、`deselect` 清除（DMA 完成回调 ISR 写 false）——`volatile` 单比特，是裸机也存在的唯一并发点（主循环与 DMA 完成 ISR 之间）。
- **RTOS 预留（v1 不写 RTOS 代码，但接口按上下文分区定案）**：将来引 RTOS 时只改本类内部、接口不变——
  - `select`：把"查忙 + 拉低"包进互斥量 `tryLock`（timeout=0），解决多任务抢启动权竞态；启动后即放锁（不跨传输持有）。
  - `deselect`：CMSIS-RTOS2 规定 mutex 不能在 ISR 释放 → ISR 半只做"拉高 CS + 清忙 + 给 FromISR 信号量"，锁归还在任务上下文。故 `deselect` 保持 ISR 安全（§3），`select`/`reset` 保持任务态。
  - 映射后 Driver/Bus/Controller 三层零改动。
- 多任务共享同一总线的互斥由 Driver/传输层在上层协作（见 Driver spec §4）。
- 复位脉冲数据（§4 reset 时序）定义于本 spec（复位执行方）；Controller spec §6 #1 仅标注归属。
