# lcdriv UT 测试框架（Spec）

> **状态：框架定稿。** 本文是测试**框架**规范——分层原则、mock 边界、目录/命名/CMake/红线门禁。**不含用例设计**：各 suite 的用例在实现步逐批设计并经 review 后补充。风格约定：gtest（FetchContent v1.17.0）+ `lcdriv_add_ut` 辅助 + `gtest_discover_tests`。

## 1. 分层原则（框架级）

**每层各自的 UT 测自己与自己的边界，互动处用对方的 mock；Driver 为组合根，不做单测，其测试场即全真实三层的 MT。**

| 测试场 | 被测对象 | 互动处用什么 | 覆盖范围 |
|---|---|---|---|
| **Bus UT** | `Bus<BusType::SPI>` | hal_stub（HAL 替身） | send/read 行为与边界 |
| **Controller UT** | `Controller<ControllerType::ILI9341, M, N>` | **MockBus** + hal_stub（GPIO/Delay） | 协议方法行为与边界（不含上电序列） |
| **MT** | `LcdDriver<SPI, ILI9341, M, N>`（真实三层） | 无（全部真实；HAL 仍为 hal_stub） | Driver 场（生命周期/CS/门面）+ 上电序列 + 端到端协议真值 |

**规则**：
- **Driver 不做独立 UT**：组合根无自有逻辑可隔离（装配接线/CS 事务/门面转发），效果全部在 hal_stub transcript 上可观测，MT 用真实三层覆盖。若将来 Driver 长出真逻辑（DMA 编排、双缓冲、错误路径），再补独立 UT。
- **bringUp（上电序列）不归任何层 UT**：Controller 私有、装配期由 Driver 调用（Controller spec §5），无公开触发路径——序列断言全部归 MT。
- **库头不自包含（include 顺序契约）**：凡分析库头（clang-tidy/编译）必须经"先 HAL 替身、后库"的 TU。

## 2. mock 语义

| mock | 位置 | 提供 |
|---|---|---|
| `hal_stub` | `support/hal_stub.{hpp,cpp}` | HAL 类型替身 + 4 个 HAL 函数（TX 记录 / read 预置回填 / GPIO 写 / 延时）+ `hal::g_transcript` 观测点 |
| `MockBus` | `support/mock_bus.hpp`（header-only，规划中） | duck-type 总线：`send` 记录字节流与调用次数、`read` 按预置回填；Controller 的 Bus 参数是 `template<typename B>`，MockBus 零成本替换 |

HAL 符号面（stub 需实现的全部）：`HAL_SPI_Transmit / HAL_SPI_Receive / HAL_GPIO_WritePin / HAL_Delay`。

## 3. 目录与命名规范

```
tests/
├── CMakeLists.txt
├── TestCompileTime.cpp        # 编译期契约（已落地，见 §6）
├── RestrictedCompile.cpp      # lcdriv_restricted 红线编译门禁（已落地，见 §5）
├── Bus/                       # Bus 层 UT；新总线 = 加 TestBusXxx.cpp
│   └── TestBusSPI.cpp         # 规划中（对应 Bus spec）
├── Controller/                # Controller 层 UT；新控制器 = 加 TestControllerXxx.cpp
│   └── TestControllerILI9341.cpp  # 规划中（对应 Controller spec）
├── Mt/                        # MT；新 (bus,controller) 搭配 = 加 TestLcdDriver<Bus><Ctrl>.cpp
│   └── TestLcdDriverSpiIli9341.cpp  # 规划中（对应 Driver spec）
└── support/
    ├── hal_stub.{hpp,cpp}
    └── mock_bus.hpp           # 规划中
```

**命名规则**：文件与 gtest suite 名 = `TestXxx`；测试文件与算法文档一一对应——`Bus spec ↔ Bus/TestBusSPI`、`Controller spec ↔ Controller/TestControllerILI9341`、`Driver spec ↔ Mt/TestLcdDriverSpiIli9341`；扩展一个搭配加一个新文件，旧文件不动。用例名 = **英文 camelCase 标识符**（如 `sendZeroLengthNoOp`），直接写进 `TEST_F` 便于检索定位。

**UT 类风格**（参照 FlowHub）：每个 suite 一个 fixture 类（`class TestBusSPI : public ::testing::Test`），公共夹具（句柄、reset 等）放 `protected` 并在 `SetUp()` 初始化；用例用 `TEST_F(TestBusSPI, 用例名)`，共享夹具不重复初始化。

## 4. CMake 规范

```cmake
function(lcdriv_add_ut TEST_NAME)
    cmake_parse_arguments(UT "" "" "SOURCES" ${ARGN})
    add_executable(${TEST_NAME} ${UT_SOURCES})
    target_include_directories(${TEST_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/include)
    target_link_libraries(${TEST_NAME} PRIVATE GTest::gtest_main lcdriv)
    add_dependencies(lcdriv_ut ${TEST_NAME})
    include(GoogleTest)
    gtest_discover_tests(${TEST_NAME} PROPERTIES LABELS "lcdriv")
endfunction()
```

- 行为类 suite 一律带 `support/hal_stub.cpp`；纯编译期 suite（TestCompileTime）不带。
- suite 经 `lcdriv_add_ut` 自动挂入聚合目标 `lcdriv_ut`（CI 构建入口）。
- 测试 target **不加** 红线 flag（gtest 需要异常）；红线合规由 `lcdriv_restricted` 验证。
- **include 顺序**：测试 TU 先 `support/hal_stub.hpp`（Controller UT 再加 `support/mock_bus.hpp`）后 `lcdriv.hpp`；**两者之间用空行分成两个 include 块**——clang-format 只做块内排序，否则会把 `lcdriv.hpp` 排到 hal_stub 前破坏 include 顺序契约。

## 5. 红线编译门禁（lcdriv_restricted，已落地）

`tests/RestrictedCompile.cpp` + target `lcdriv_restricted`：以固件同款红线 flag 编译整个伞头（hal_stub 可见下的模板实例化）：

```
-fno-exceptions -fno-rtti -fno-threadsafe-statics -Wall -Wextra -Werror
```

**目的**：gtest 测试用默认 flag（允许异常/RTTI），真机固件才用红线 flag——若无此目标，主机 CI 永远验证不到"库在红线编译选项下可编译"，红线违规（用了异常/RTTI/线程安全静态）直到烧固件才暴露。此目标随 `lcdriv_ut` 构建，把编译期红线合规前移到每次 CI。

## 6. 已落地 suite 清册

| suite / 目标 | 类型 | 状态 | 内容 |
|---|---|---|---|
| `TestCompileTime` | gtest | **已落地** | 编译期契约：IsSupported 白名单、M/N 自由实例化与 width/height、`kBytes`、`kMadctlDefault` 推导值（见 §7 断言清单） |
| `lcdriv_restricted` | 编译门禁（非 gtest） | **已落地** | 红线 flag + `-Werror` 编译整个伞头（见 §5） |
| `TestBusSPI` / `TestControllerILI9341` / `TestLcdDriverSpiIli9341` | gtest | 规划中 | 用例在对应实现步设计并经 review 后补充 |

## 7. TestCompileTime 已实现断言清单（供 review）

- `static_assert`：`IsSupported<SPI,ILI9341>` 为真；I2C+ILI9341 / SPI+ST7789 / I2C+ST7789 为假（A1）。
- `static_assert`：`kBytes == M*N*2`（240×320、320×240 → 153600；240×100 → 48000）（A3）。
- `static_assert`：`kMadctlDefault` 竖屏 240×320 → 0x00；横屏 320×240 → 0x60（候选，**硬件实测锁定后更新**）（A5）。
- 运行时 `TEST`：240×320 与 320×240 可默认构造，`width()/height()` = M/N（A2）；常量可作普通值使用。
- **未代码化（规划在 A 组但尚未写入文件）**：不支持组合编译失败（A6）、M/N≤0 守卫（A7）、不可拷贝（A8）——这三类为"编译失败"型断言，库侧守卫已实现（static_assert/delete），测试侧负向编译验证待定形式（CMake try_compile 或注释保留人工验证）。

## 8. 版本与分支策略（发布后）

- 主分支持续演进；实现 + 硬件调试完成后，将当前实现版本单独拉发布分支（脱离主干，仅 bugfix 演进）。
- 消费方（依赖本库的工程）以发布分支/标签作为依赖版本：FetchContent `GIT_TAG = 发布分支名或标签`（见根 README「集成」）。
- 用例随发布分支冻结；协议相关数据变更（横屏 MADCTL 锁定）必须同步相关断言与 Controller spec §4.2/§8。
