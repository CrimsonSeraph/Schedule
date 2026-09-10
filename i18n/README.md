# i18n（国际化）

## 职责

存放 Qt Linguist 翻译源文件（`.ts`），并通过 `qt_add_translations()` 在构建期生成 `.qm` 并嵌入可执行程序；运行时按系统语言加载。

## 约定

- **源语言是简体中文**：QML 与 C++ 中的 `qsTr()` / `tr()` 直接写中文原文；
- 因此**不需要** `schedule_zh_CN.ts`——中文用户直接看到源字符串；
- 其它语言按 `schedule_<locale>.ts` 命名，例如 `schedule_en.ts`（英文）。

## 目录结构

```text
i18n/
├── README.md
└── schedule_en.ts        # 英文翻译源（初始为空，由 lupdate 生成条目）
```

## 工作流

```bash
# 1) 扫描源码中的 qsTr()/tr()，更新 .ts（新增条目为 unfinished）
cmake --build --preset windows-msvc-debug --target update_translations

# 2) 用 Qt Linguist 打开 .ts 并翻译
linguist i18n/schedule_en.ts

# 3) 重新构建：qt_add_translations 会自动调用 lrelease 生成 .qm 并嵌入资源
cmake --build --preset windows-msvc-debug

# 4) 验证：切换系统语言或在启动时指定语言
#    （Windows 上可通过“设置 → 时间和语言 → 语言”添加英文并置顶）
```

## 运行时加载

`src/app/main.cpp` 在创建桥接对象之前安装翻译器：

```cpp
QTranslator translator;
if (translator.load(QLocale(), QStringLiteral("schedule"), QStringLiteral("_"), QStringLiteral(":/i18n"))) {
    gui_app.installTranslator(&translator);
}
```

- `.qm` 由 `qt_add_translations(Schedule ... RESOURCE_PREFIX "/i18n")` 嵌入，路径为 `:/i18n/schedule_<locale>.qm`；
- 找不到对应语言的 `.qm` 时自动回退到源字符串（简体中文），不会出现空白界面。

## 新增语言

1. 在 `src/app/CMakeLists.txt` 的 `qt_add_translations(... TS_FILES ...)` 中追加 `i18n/schedule_<locale>.ts`；
2. 执行 `update_translations` 生成条目；
3. 翻译并提交。

## 注意事项

- **QML 中的 `qsTr()` 需要被 lupdate 扫描到**：QML 文件必须出现在 `qt6_add_qml_module()` 的 `QML_FILES` 列表中，否则条目不会被收集；
- 界面中大量文本是**运行期拼接**的（例如 `qTr("第 %1 周").arg(week)`），翻译时请保留 `%1` 等占位符；
- 日期 / 时间格式目前固定为 `yyyy-MM-dd` / `HH:mm`（课表场景下更易读），不随语言变化，这是有意为之。

## 相关文档

- [根 README](../README.md)
- [打包与发布](../docs/PACKAGING.md)
- [app 模块](../src/app/README.md)
