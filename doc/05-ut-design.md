# lcdriv UT 设计（Unit Test Case Design）

> **状态：实现时按本文落地。** 本设计在代码实现阶段同步转化为测试代码；UT 风格参照 FlowHub（`option(..._BUILD_TESTS)` + FetchContent googletest v1.17.0 + `add_ut` 辅助函数 + `gtest_discover_tests`；测试类 `TestXxx`、用例名 `行为_对象`）。数据依据：doc/02、doc/03、doc/04（spec 数值与编码约定）。

## 1. UT 目标与边界

**测什么**：本库全部可测逻辑——协议字节序列、窗口/像素编码、分块边界、上电序列、CS 事务、生命周期、编译期契约。**不测什么**：HAL 自身、真实硬件时序、GPIO/SPI 外设行为。

**关键手段——主机侧 HAL 替身（stub），库代码零改动**：

- 库的实现只调用 4 个 HAL 符号：`HAL_SPI_Transmit`、`HAL_SPI_Receive`、`HAL_GPIO_WritePin`、`HAL_Delay`，且只持有 `SPI_HandleTypeDef*`/`GPIO_TypeDef*` 指针、不触碰内部 → 测试侧用**同名同形的主机替身**（`tests/support/hal_stub.hpp/.cpp`）即可编译真实库代码。
- **include 顺序契约在此显形**：测试 TU 先 include `hal_stub.hpp`（扮演"main.h + HAL"），再 include `lcdriv.hpp`——库源码不含任何 `#ifdef`/测试钩子，主机/目标机同一份代码。这同时验证了 doc/02 §3 的顺序契约可工作。
- 观测点 = stub 的 **Transcript**（记录全部 HAL 调用：SPI 发送字节流、读回预置、GPIO 写、延时累计）；测试断言 transcript 序列与 spec 逐字节/逐序一致。
- 被测对象 = 真实三层（Bus/Controller/LcdDriver 的偏特化实例）；不引入额外 fake bus（duck-typing 下 Bus 本身就是可测的真实对象）。

## 2. 测试支撑（tests/support/）

| 文件 | 内容 |
|---|---|
| `hal_stub.hpp` | 主机替身类型：`struct SPI_HandleTypeDef`（含 tag 字段，可区分句柄）、`struct GPIO_TypeDef`（占位）、`GPIO_PinState`、`GPIO_PIN_0..15`（沿用 STM32 数值）；声明 4 个 HAL 函数与 `hal::Transcript`（字段：`tx`/`rx`/`gpio`/`delays`/`rxPreset`，方法：`reset()`/`lastTx()`/`totalTxBytes()`） |
| `hal_stub.cpp` | 4 个 HAL 函数实现，全部写入全局 `hal::g_transcript`；`HAL_SPI_Receive` 从 `rxPreset` 按序回填、取尽补 0 |
| `hal_assert.hpp`（可选） | 便捷断言宏：`EXPECT_TX(h, bytes...)` 等，让用例读起来贴近 spec 表 |

Transcript 记录格式约定（断言依据）：

```
tx:       (handle, 字节流)            → 每次 HAL_SPI_Transmit，按调用序展开
rx:       (handle, n)                → 每次 HAL_SPI_Receive；读回内容来自 rxPreset（取尽补 0）
gpio:     (port, pin, state)         → 每次 HAL_GPIO_WritePin，按调用序（state: RESET/SET）
delays:   逐次 HAL_Delay 毫秒值
```

> 实现提示：stub 内用 `std::vector`/`std::map`（仅测试侧，堆不受红线约束）。

## 3. 目录与 CMake 接线（实施骨架）

```
lcdriv/
├── CMakeLists.txt              # INTERFACE 库 + LCDRIV_BUILD_TESTS 选项 + FetchContent gtest
├── include/
│   ├── lcdriv.hpp              # 伞头（只 include 内部各层头）
│   └── lcdriv/                 # 按层拆分（声明与实现同文件）
│       ├── core.hpp            # 枚举 + IsSupported 白名单
│       ├── bus.hpp             # Bus<SPI>
│       ├── controller.hpp      # Controller<ILI9341, M, N>
│       └── lcddriver.hpp       # LcdDriver<SPI, ILI9341, M, N>
└── tests/
    ├── CMakeLists.txt
    ├── support/hal_stub.{hpp,cpp}
    ├── TestCompileTime.cpp         # 编译期契约（已落地）
    ├── TestBusSpi.cpp              # Bus<SPI>（实现 Bus 时补）
    ├── TestControllerIli9341.cpp   # Controller<ILI9341, M, N>（实现时补）
    └── TestLcdDriverSpiIli9341.cpp # LcdDriver<SPI, ILI9341, M, N>（实现时补）
```

tests/CMakeLists.txt（实际内容；后续 suite 按 §6 进度追加 `lcdriv_add_ut(...)` 一行，行为类需带 `support/hal_stub.cpp`）：

```cmake
function(lcdriv_add_ut TEST_NAME)
    cmake_parse_arguments(UT "" "" "SOURCES" ${ARGN})
    add_executable(${TEST_NAME} ${UT_SOURCES})
    target_include_directories(${TEST_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/include)
    target_link_libraries(${TEST_NAME} PRIVATE GTest::gtest_main lcdriv)
    include(GoogleTest)
    gtest_discover_tests(${TEST_NAME} PROPERTIES LABELS "lcdriv")
endfunction()

lcdriv_add_ut(TestCompileTime SOURCES TestCompileTime.cpp)
```

- 测试 target **不加** `-fno-exceptions`（gtest 需要异常；库代码本身不抛，双模式编译安全，与 FlowHub/raycaster 做法一致）。
- 测试 TU 统一：`#include "support/hal_stub.hpp"` → `#include "lcdriv.hpp"`；**两者之间必须用空行分成两个 include 块**——clang-format 只做块内排序，否则会把 `lcdriv.hpp` 排到 `hal_stub.hpp` 前破坏 include 顺序契约。
- 实施阶段把 `lcdriv_ut` 挂进仓库现有 CI（scripts/，与 clang-tidy 并列），跑 `ctest`。

## 4. 用例设计

### A. TestCompileTime — 编译期契约

| # | 用例 | 断言 | 依据 |
|---|---|---|---|
| A1 | IsSupported_白名单 | `IsSupported<SPI,ILI9341>::value == true`；未列组合（I2C+ILI9341 / SPI+ST7789 / I2C+ST7789）`== false` | doc/01 §3 |
| A2 | 偏特化_MN自由 | 同一偏特化服务 `240×320` 与 `320×240` 两实例（均可实例化）；`width()/height()` = M/N | doc/03 §4.1 |
| A3 | 帧长常量 | `kBytes == M*N*2`（240×320 → 153600） | doc/03 §4.3 |
| A4 | 窗口编码常量 | 各 (M,N) 的窗口上下界大端字节组与 doc/03 §4.3 表一致（0x00EF/0x013F 等） | doc/03 §4.3 |
| A5 | MADCTL 推导 | 竖屏实例 → 0x00（实测值）；横屏实例 → 当前候选 0x60（TestCompileTime 已断言；硬件实测锁定后同步更新断言与 doc/03 §4.2/§8） | doc/03 §4.2 |
| A6 | 不支持组合编译失败 | `LcdDriver<I2C,ILI9341,...>` / `Controller<ST7789,...>`（未特化）→ static_assert / 不完整类型报错（人工编译验证；可选 try_compile 自动化） | doc/03 §2、doc/04 §2 |

### B. TestBusSpi — Bus\<SPI\>

夹具：两个 stub 句柄 `h1/h2`；用例前 `hal::reset()`。实例：`Bus<BusType::SPI> bus(&h1);`

| # | 用例 | 步骤/断言 | 依据 |
|---|---|---|---|
| B1 | Ctor_保存句柄 | 构造后 send 记录落在 h1（`transcript.spiTx()` 句柄 == &h1） | doc/02 §2 |
| B2 | Send_转发单段 | `send({0x01},1)` → TX = [0x01]，1 次调用，n=1 | doc/02 §4 |
| B3 | Send_转发长段 | `send(vec, 30000)` → 单次调用 n=30000，内容逐字节一致 | doc/02 §4 |
| B4 | Send_零长无操作 | `send(ptr, 0)` → **无** HAL 调用（实现定：n==0 直接返回） | doc/02 §4（边界补充，见 §7） |
| B5 | Send_不碰GPIO | TX 记录存在但 `transcript.gpioWrites()` 为空（CS/DC 均非 Bus 职责） | doc/02 §1 |
| B6 | Read_回填与长度 | 预置 rx=[0x93,0x41,0x00]（3 字节），`read(buf,3)` → buf 一致、调用 1 次 | doc/02 §4 |
| B7 | SendRead_句柄区分 | 两 Bus 实例（h1/h2）交替 send，记录按调用序归属正确句柄 | doc/02 §2 |

### C. TestControllerIli9341 — Controller\<ILI9341, M, N\>（主战场）

夹具：`Controller<ControllerType::ILI9341, M, N> ctrl(&dcPort, DC_PIN, &rstPort, RST_PIN);` + `Bus<BusType::SPI> bus(&h1);`，`hal::reset()`；用例**以竖屏 240×320 为主、横屏 320×240 复跑窗口相关**（模板参数化或用两个夹具）。

| # | 用例 | 断言要点 | 依据 |
|---|---|---|---|
| C1 | WriteCommand_命令态 | TX=[cmd] 1 字节；gpio 记录 DC=RESET（低=命令） | doc/03 §5 |
| C2 | WriteData_数据态 | TX=data 逐字节；DC=SET（高=数据） | doc/03 §5 |
| C3 | WriteData_零长 | `writeData(bus, p, 0)` → 无 TX、DC 无变化 | doc/03 §5 |
| C4 | PushFrame_竖屏窗口与全序 | 全序 = `[0x2A][00 00 00 EF][0x2B][00 00 01 3F][0x2C]` + 像素流；命令参数 4 字节大端 | doc/03 §4.3/§5 |
| C5 | PushFrame_像素透传 | 像素流与 px 逐字节一致（无字节交换）；总数 153600 | doc/03 §4.3 |
| C6 | PushFrame_分块 | bus.send 恰 3 次：65535/65535/22530（帧长>单次上限） | doc/03 §7 |
| C7 | PushFrame_小分辨率单次 | `Controller<...,240,100>` 帧长 48000<65535 → send 恰 1 次（验证分块按上限执行、M/N 自由） | doc/03 §7 |
| C8 | PushFrame_横屏窗口 | `320×240` 实例 → `[0x2A][00 00 01 3F][0x2B][00 00 00 EF]` | doc/03 §4.3 |
| C9 | FillScreen_颜色展开 | 窗口全序 + 153600 字节 = color 高/低字节交替；抽样首/尾/中段字节 | doc/03 §5 |
| C10 | ReadID_丢弃哑字节 | 预置 rx=[0x00,0x93,0x41,0x00] → 返回 `0x934100`；`read` 恰 1 次 n=4 | doc/03 §5 |
| C11 | ReadID_非零哑字节 | 预置 rx=[0xFF,0x93,0x41,0x00] → 仍返回 `0x934100`（b0 必丢） | doc/03 §5 |
| C12 | ReadID_模块ID透传 | rx=[0x00,0x93,0x41,0x12] → `0x934112` | doc/03 §5 |
| C13 | SetOrientation_写寄存器 | TX=[0x36][0x60] | doc/03 §5 |
| C14 | 上电序列_复位时序 | gpio：RST 先高 5ms → 低 ≥10ms → 高；此后 delay 累计 ≥120ms 才开始首命令 | doc/03 §6 行 1 |
| C15 | 上电序列_命令全序 | TX 命令序（含参数、延时插入点）与 doc/03 §6 表逐条一致：`0x01(+120ms) 0x11(+120ms) 0x3A:55 0x36:<推导值> 0x35:00 0xC0:23 0xC1:10 0xC5:3E 28 0xC7:86 0xB1:00 18 0xB6:08 82 27 0xF2:00 0x26:01 0x29(+20ms)`；命令总数与参数长度逐一核对 | doc/03 §6 |
| C16 | 上电序列_竖屏MADCTL行 | 竖屏实例上电序列中 0x36 参数 = 0x00 | doc/03 §4.2 |
| C17 | 上电幂等 | 连续两次上电 → transcript 序列完全相同（可重入） | doc/03 §5 |

### D. TestLcdDriverSpiIli9341 — LcdDriver\<SPI, ILI9341, M, N\>

引脚：CS=&csPort 等；stub 句柄 h1。

| # | 用例 | 断言要点 | 依据 |
|---|---|---|---|
| D1 | 范式二_构造即上电 | `lcd_.emplace(...)` 后 transcript 含完整上电序列（= C15 序列）；构造返回后 CS=SET（空闲高） | doc/04 §3 |
| D2 | 范式二_构造后即可用 | emplace 后直接 `pushFrame` 无中间 init 调用，transcript 正常 | doc/04 §3 |
| D3 | 范式一_init成功 | 默认构造 → `init(...)==true`；init 后 CS 空闲高 | doc/04 §3/§5 |
| D4 | 范式一_未init访问 | 不测（未定义行为，doc/04 §3 明示） | doc/04 §3 |
| D5 | 双范式等价 | 范式一 init 后与范式二构造后的 transcript 相等（同参数同序列） | doc/04 §5 |
| D6 | PushFrame_单事务 | CS 写入恰为 `[低, 高]` 各一次，且 3 次 bus.send 全部夹在两次 CS 写之间（事务内无 CS 抖动） | doc/04 §4 |
| D7 | FillScreen_单事务 | 同上，CS 一次低/一次高；像素数 153600 | doc/04 §4/§5 |
| D8 | ReadID_单事务端到端 | 预置 rx → 返回 `0x934100`；CS 一次低/高 | doc/04 §5 |
| D9 | SetOrientation_单事务 | TX=[0x36][值]；CS 一次低/高 | doc/04 §5 |
| D10 | 宽高 | `width()/height()` == M/N（240/320 与 320/240 两实例） | doc/04 §5 |
| D11 | 连续操作序列 | init → readID → fillScreen → pushFrame 依序执行，每操作一个事务，无串扰 | doc/04 §4/§5 |
| D12 | 重新上电 | 范式二：`reset()` + `emplace(...)` 后仍可用，transcript 序列从头重放 | doc/04 §3 |

### E. 边界与回归（散布于上表，单列清单）

- E1 字节序回归：pushFrame 像素逐字节透传（防误加字节交换）——C5。
- E2 分块边界回归：帧长 153600 → 恰 3 次且每次 ≤65535——C6；小分辨率 1 次——C7。
- E3 事务原子性回归：所有公开操作 CS 一次低/高——D6–D9。
- E4 上电序列漂移回归：全序对照表（C15）——实现与实测锁定值改动时必须同步本用例与 doc/03 §6/§8。

## 5. 与 spec 的对应

每个用例的"依据"列指向 doc/02–04 具体小节；**spec 数值变更（如横屏 MADCTL 锁定）必须同步更新用例**（A5、C16 首当其冲）并登记修订记录——即 doc/README「约定」中"改协议数据须登记并重新实测"的测试侧镜像。

## 6. 实施顺序（代码阶段）

1. 落伞头与各层头骨架（`include/lcdriv*.hpp`，先过编译）→ 同步落 `tests/support/hal_stub`（已完成）。
2. 每实现一个 public 函数即补对应用例（B/C/D 按 spec 小节推进），`ctest` 全绿再继续。
3. A 组编译期用例最先落（已完成，TestCompileTime 全绿）。
4. 接入仓库 CI（现有 scripts/），主机侧 `lcdriv_ut` 与 tidy 并列；硬件实测通过后锁定横屏 MADCTL，更新 A5/C16 与 doc/03。

## 7. 实施时需回填 spec 的小澄清

- doc/02 Bus.send `n==0` 无操作：已写入 doc/02 §4（B4 断言，勿重复实现）。
- HAL 符号面（库实际调用的全部 HAL 符号 = stub 需实现的全部）：`HAL_SPI_Transmit / HAL_SPI_Receive / HAL_GPIO_WritePin / HAL_Delay`——即库对 HAL 的依赖面收敛为这 4 个符号。

## 8. 版本与分支策略（发布后）

- 主分支持续演进；**实现 + 硬件调试完成后，将当前实现版本单独拉发布分支**（脱离主干，仅 bugfix 演进）。
- raycaster 以发布分支为依赖：FetchContent `GIT_TAG = 发布分支名或标签`（版本锁定，见根 README「集成」）。
- UT 用例随发布分支冻结：协议相关用例（C/D/E 组）只允许因 bugfix 而改，并同步登记 doc/03 §8 修订记录。
