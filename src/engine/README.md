# engine（引擎 / QML 桥接层）

## 职责
把 C++ 侧的服务以 QObject 形式暴露给 QML（属性 / 槽 / 信号），
是 UI 层与 core 层之间的桥梁。

## 依赖
- `MyCore`（核心层静态库）
- Qt 6.9.3（组件：`Core`、`Qml`，需要 `AUTOMOC` 生成 Q_OBJECT 元数据）

## 产物
- `MyEngine`（静态库，PUBLIC 链接 MyCore / Qt6::Core / Qt6::Qml）
- 公开头文件目录：`include/`（引用方式：`#include "engine/AppBridge.h"`）

## 当前内容
| 类型 | 说明 |
|------|------|
| `AppBridge` | 桥接对象；属性 `version`（来自 `DataEngine::getVersion()`）、`Q_INVOKABLE testButtonClicked()`（qDebug 输出测试信息）、信号 `testSignal(msg)` |

## 注册到 QML
在可执行程序入口（`src/app/main.cpp`）中、加载 QML 之前执行：

```cpp
qmlRegisterType<myapp::AppBridge>("MyApp", 1, 0, "AppBridge");
```

随后 QML 侧即可：

```qml
import MyApp 1.0

AppBridge {
    id: bridge
}

Text { text: bridge.version }
Button {
    text: qsTr("测试")
    onClicked: bridge.testButtonClicked()
}
```

## 构建
本模块由根 `CMakeLists.txt` 通过 `add_subdirectory(src/engine)` 引入。
