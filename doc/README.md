# doc/ 文档索引

> 本目录是 lcdriv 的**正式文档（spec）**，面向实现与维护，是代码实现的唯一依据。
> 代码中不写注释——需要解释的内容一律落在此处。

## 目录结构

```
doc/
├── README.md                 # 本索引
├── adr/                      # 架构决策记录（ADR：关键取舍与理由）
│   └── 0001-dma-completion-routing.md   # DMA 完成回调路由（指针 + 单一活跃所有者槽）
├── code/                     # 代码 spec（实现依据）
│   ├── architecture.md       #   架构总览：分层/模板策略/接口契约/路线
│   ├── panelMgr.md          #   PanelMgr 组件 spec（屏组 CS/RST，跨层组件）
│   ├── bus/                  #   Bus 层各特化算法 spec（新总线 = 加文件）
│   │   └── spi.md            #     Bus<SPI>
│   ├── controller/           #   Controller 层各偏特化算法 spec（新屏 = 加文件）
│   │   └── ili9341.md        #     Controller<ILI9341, M, N>（DC 引脚；CS/RST 归 PanelMgr）
│   └── driver/               #   Driver 层各搭配算法 spec（新搭配 = 加文件）
│       └── spi-ili9341.md    #     LcdDriver<SPI, ILI9341, M, N, P>
└── test/                     # 测试 spec（怎么验证）
    ├── architecture.md       #   测试框架总览：分层/mock/目录/CMake/红线门禁
    ├── panelMgr.md          #   PanelMgr UT case design（跨层组件）
    ├── bus/                  #   Bus UT case design（新总线 = 加文件）
    │   └── spi.md            #     TestBusSPI 用例
    ├── controller/           #   Controller UT case design（新屏 = 加文件）
    │   └── ili9341.md        #     TestControllerILI9341 用例
    └── driver/               #   Driver（MT）case design（新搭配 = 加文件）
        └── spi-ili9341.md    #     TestLcdDriverSpiIli9341（MT）用例
```

**约定**：code/ 与 test/ 的叶子文件按层分文件夹、按特化命名，一一对应：
`code/bus/spi ↔ test/bus/spi ↔ tests/Bus/TestBusSPI`、`code/controller/ili9341 ↔ test/controller/ili9341 ↔ tests/Controller/TestControllerILI9341`、`code/driver/spi-ili9341 ↔ test/driver/spi-ili9341 ↔ tests/Mt/TestLcdDriverSpiIli9341`。新增特化 = 三个位置各加一个文件，旧文件不动。

## 职能分层

| 文档 | 职能 | 回答的问题 |
|---|---|---|
| `code/architecture.md` | 代码架构 | 为什么这样分层/为什么编译期配置 |
| `code/{bus,controller,driver}/*.md` | 算法 spec | 每个类每个 public 函数**做什么**（可核对的数据） |
| `test/architecture.md` | 测试框架 | 分层 UT + MT、mock 边界、红线门禁怎么组织 |
| `test/{bus,controller,driver}/*.md` | case design | 各 suite 具体用例（按 suite 评审后填充） |
| 根 `README.md` | 项目门面 | 这是什么、怎么构建/集成 |

## 阅读顺序

1. 先读 `code/architecture.md`，建立整体框架。
2. 自下而上读算法 spec：`code/bus/spi.md` → `code/controller/ili9341.md` → `code/driver/spi-ili9341.md`；`code/panelMgr.md`（跨层组件）随 controller/driver 一起读。
3. 写测试/改协议数据前读 `test/architecture.md`；各 suite 的 case design 在实现步设计并经 review 后读 `test/{层}/*.md`。

## 约定（算法 spec 模板）

- **每篇算法 spec 对应一个具体的类特化**（一个模板偏特化实例族 = 一篇），放在所属层文件夹下。
- **跨层组件（PanelMgr）**：非 (bus, controller) 特化，spec 放 `code/panelMgr.md`（同层文件夹规则不适用）。
- 算法 spec 只描述该类**每个 public 函数的具体功能**，固定小节：
  - 定位、类签名；层间依赖（只用下层 public 接口，不列本类私有拆分，有则写）；
  - 每个 public 函数：**功能**（具体行为，寄存器/引脚层面效果）／**参数与返回值**／**前置/后置条件**／**副作用**／**边界与限制**；
  - 当前限制与后续计划。
- **不写伪代码**；**不体现内部实现**（私有函数拆了几个、内部状态等一律不出现）。
- **算法 spec = 可直接约束实现的 spec**：功能描述必须落到可核对的数据（命令码、参数字节、延时数值、编码规则、边界数值），不停留在"意图层"。
- 每篇 spec 头部标注**数据来源**（实测驱动 / 数据手册）；协议数据以实测为准。
- 修改协议数据（初始化序列、读 ID 时序等）属破坏性变更：必须重新硬件实测，并同步本文与相关测试；变更留痕走 git 历史。
- **跨文档引用用层短名**（Bus spec / Controller spec / Driver spec / PanelMgr spec / 架构 / 测试框架），不写文件路径——文件搬迁不破坏引用。
- **跨层公共小类型**（如 `GpioPin`）不单独成篇，在 架构 spec §2「公共类型」登记；各层 spec 直接引用。
