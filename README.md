# lcdriv

裸机 LCD 驱动库，C++ 实现（禁用堆分配、异常、RTTI、虚表）。只做单帧传输，不带字体、不带 GFX。
## 目标

- **干净的驱动**：只做 `init` / `pushFrame` / `fillScreen` / `readID`
- **解耦设计**：传输（Bus）和控制器（Controller）正交，模板参数独立
- **编译期确定**：Bus、Controller、分辨率、像素格式均在编译期固定
- **零开销**：无堆、无异常、无 RTTI、无虚表

## 仓库结构

```
include/lcdriv.hpp          伞头（消费方 include 入口）
include/lcdriv/*.hpp        按层拆分：core / bus / controller / lcdDriver / panelMgr（文件名为小驼峰，类名为大驼峰）
tests/                      gtest 单元测试（hal_stub 为主机侧 HAL 替身）
doc/                        正式文档（spec）—— 索引见 doc/README.md
```

## 当前支持

- SPI + ILI9341（RGB565；方向与逻辑分辨率见 Controller spec §4（`doc/code/controller/ili9341.md`））

## 构建与测试

```bash
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```

gtest 经 FetchContent 拉取（v1.17.0）；离线/内网可用
`-DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=<本地 googletest 源码>` 覆盖。

## 集成（消费方如何引入，无需复制粘贴）

lcdriv 是 **header-only 模板库**：引入 = 让 `include/` 目录可见（不编译、不链接、无库产物）。

**方式 1（推荐，CMake FetchContent，版本可钉）**：

```cmake
# lcdriv 侧提供 INTERFACE 目标（仓库根 CMakeLists.txt，仅 3 行核心）：
#   add_library(lcdriv INTERFACE)
#   add_library(lcdriv::lcdriv ALIAS lcdriv)
#   target_include_directories(lcdriv INTERFACE include/)

# 消费方 CMakeLists.txt：
include(FetchContent)
FetchContent_Declare(lcdriv
    GIT_REPOSITORY https://github.com/yun-qilong/lcdriv.git
    GIT_TAG        main # 第一个驱动完成后将会拉取独立分支，到时将tag钉到对应分支
)
FetchContent_MakeAvailable(lcdriv)
# ... target_link_libraries(<固件目标> lcdriv)
# INTERFACE 目标只传播 include 目录与用法要求，不会编译/链接任何 lcdriv 代码
```

**方式 2（最小，裸 include path）**：把 `lcdriv/include` 加进消费方 include 路径即可（联调最快，但路径写死、不锁版本）。

**include 顺序契约**（使用 lcdriv 的每个 TU）：

```cpp
#include "main.h"       // 工程头（STM32 HAL 类型来源；家族由工程决定）
#include "lcdriv.hpp"   // 库不 include 任何 HAL/家族头（避免锁死系列）
```

## 单元测试

- gtest v1.17.0（FetchContent），测试框架规范见 `doc/test/architecture.md`（分层 UT + MT；`lcdriv_restricted` 红线编译门禁随 CI 构建）；测试通过 `tests/support/hal_stub`（主机侧 HAL 替身）在**不改动库代码**的前提下编译运行真实库逻辑。

## 版本与分支策略

- 主分支持续演进；**实现并硬件调试完成后，将当前实现版本单独拉发布分支**（脱离主干，仅 bugfix 演进）。
- 消费方依赖该发布分支：FetchContent `GIT_TAG` 指向发布分支/标签（版本锁定，见上方「集成」）。
- 协议相关数据（初始化序列、MADCTL 实测值）变更须硬件重新实测，并同步 Controller spec（`doc/code/controller/ili9341.md`）与对应测试。

## 设计文档

正式文档（spec）在 `doc/`：`doc/README.md` 索引 → `code/architecture.md`（代码架构）→ `code/{bus,controller,driver}/*.md`（算法 spec）→ `test/architecture.md`（测试框架）→ `test/{bus,controller,driver}/*.md`（case design）。实现与维护以 `doc/` 为准；代码中不写注释，需要解释的内容一律见 doc/。
