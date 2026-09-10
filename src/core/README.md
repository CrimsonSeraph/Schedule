# core（核心层）

## 职责

存放与界面无关的纯业务 / 数据逻辑。**本模块不依赖任何 GUI 技术**（QWidget / QML），仅使用 `Qt6::Core`，保证核心逻辑可在无显示环境下单元测试与复用。

## 依赖

- Qt 6.9.3（组件：`Core`）
- 无其他项目内部依赖

## 产物

- `MyCore`（静态库）
- 公开头文件目录：`include/`（引用方式：`#include "core/DataEngine.h"`）

## 当前内容

| 类           | 说明                                                |
| ------------ | --------------------------------------------------- |
| `DataEngine` | 核心数据引擎；`get_version()` 返回应用版本 `"1.0.0"` |

## 构建

本模块由根 `CMakeLists.txt` 通过 `add_subdirectory(src/core)` 引入，无需单独构建。

## 相关文档

- [根 README](../README.md) — 架构总览与构建指南
- [engine](../engine/README.md) — 依赖本层的 QML 桥接层
- [ui](../ui/README.md) / [app](../app/README.md) — 上层界面与应用入口
