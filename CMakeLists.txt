cmake_minimum_required(VERSION 3.20)

project(ScheduleApp
	VERSION 1.0.0
	DESCRIPTION "跨平台课表应用 Schedule"
	LANGUAGES CXX
)

# 全局编译选项
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
	set_property(
		CACHE CMAKE_BUILD_TYPE
		PROPERTY
			STRINGS
			Debug
			Release
			RelWithDebInfo
			MinSizeRel
	)
endif()

# 通用选项
option(BUILD_TESTS "构建单元测试" OFF)

# 启用 CTest：必须在所有 add_test() 之前调用，故放在顶层
if(BUILD_TESTS)
	enable_testing()
endif()

# Qt 依赖（后续模块按需补充组件）
# Gui 必须在这里显式请求：Android 打包链路要求 Qt6::Gui 的插件包
# （如 Qt6::QAndroidIntegrationPlugin）在 qt_add_executable 定稿前完成注册；
# 只在子目录里通过 Quick 间接依赖 Gui，会导致 androiddeployqt 生成部署设置时报
# "No target Qt6::QAndroidIntegrationPlugin"。
find_package(Qt6 6.9.3 REQUIRED COMPONENTS Core Gui Qml Quick QuickDialogs2 Sql)

# Qt 推荐的项目级初始化（启用 AUTOMOC/AUTOUIC/AUTORCC，采纳 Qt 策略默认值）
qt_standard_project_setup()

# ---------------------------------------------------------- 内嵌浏览器抓取（可选）
# 用途：让用户在应用内打开教务系统页面，并直接从**当前页面**抓取课表。
# 隐私：只用内嵌 Web 组件承载用户自己的登录会话，应用**不读取、不保存**任何 Cookie 或密码。
#
# 外观上是三选一，实际由**编译期**决定（QML 侧通过 schedule.importExport.webBrowserBackend 读取）：
#   webview   → Qt WebView，复用系统原生 Web 组件（Android WebView / WKWebView 等），体积小
#   webengine → Qt WebEngine，自带 Chromium
#   none      → 不链接任何 Web 组件，界面只保留“系统浏览器打开 + 文件导入”的兜底路径
#
# 注意：`qtwebview` 是 Qt 的**独立模块**，在线安装器的默认勾选与 CI 的 install-qt-action
# 都不一定包含它；套件里没有该模块时这里会静默落到 none，Android / iOS 又没有 WebEngine 可退，
# 最终 APK 的「从教务导入」就只能走系统浏览器。CI 用 install-qt-action 的
# `modules: qtwebview` 显式安装；手工装 Qt 请在 Maintenance Tool 里勾上 “Qt WebView”。
option(SCHEDULE_ENABLE_EMBEDDED_BROWSER "启用内嵌浏览器抓取课表（关闭则回退系统浏览器 + 文件导入）" ON)

set(SCHEDULE_BROWSER_BACKEND "none")

if(SCHEDULE_ENABLE_EMBEDDED_BROWSER)
	# 桌面平台上 Qt WebView 只是外壳，真正渲染的是 <Qt>/plugins/webview 下的后端插件
	# （Windows 官方包为 qtwebview_webengine，Linux 为 qtwebview_qtwebengine）。
	# 官方 MinGW 套件**不附带**该插件：照常启用的话程序启动后只会打印
	# “No WebView plug-in found!”，WebView 完全不可用。因此必须在配置期就识别并排除，
	# 让它直接落到 WebEngine 或 none。Android / iOS 的后端由平台自身提供，无需插件目录。
	if(ANDROID OR IOS)
		set(SCHEDULE_HAS_WEBVIEW_BACKEND TRUE)
	else()
		set(SCHEDULE_HAS_WEBVIEW_BACKEND FALSE)
		if(Qt6_DIR)
			# Qt6_DIR 形如 <Qt>/lib/cmake/Qt6，回退三级即安装前缀
			get_filename_component(_schedule_qt_prefix "${Qt6_DIR}/../../.." ABSOLUTE)
			set(_schedule_webview_plugin_dir "${_schedule_qt_prefix}/plugins/webview")
			if(EXISTS "${_schedule_webview_plugin_dir}")
				file(GLOB _schedule_webview_backends "${_schedule_webview_plugin_dir}/*")
				if(_schedule_webview_backends)
					set(SCHEDULE_HAS_WEBVIEW_BACKEND TRUE)
				endif()
			endif()
		endif()
	endif()

	# 只探测 WebView：抓取走 runJavaScript 拉取模型（见 BrowserCaptureScript.h），
	# 不使用 QWebChannel；Qt6::WebView 自身也只依赖 Core / Gui，把 WebChannel 当作
	# 启用条件只会多一个“模块没装齐就静默降级”的失败点。
	find_package(Qt6 6.9.3 QUIET COMPONENTS WebView)
	if(Qt6WebView_FOUND AND SCHEDULE_HAS_WEBVIEW_BACKEND)
		set(SCHEDULE_BROWSER_BACKEND "webview")
	else()
		find_package(Qt6 6.9.3 QUIET COMPONENTS WebEngineQuick)
		if(Qt6WebEngineQuick_FOUND)
			set(SCHEDULE_BROWSER_BACKEND "webengine")
		endif()
	endif()
endif()

if(SCHEDULE_BROWSER_BACKEND STREQUAL "none")
	message(STATUS "内嵌浏览器：未启用（回退到系统浏览器 + 文件导入）")
	if(SCHEDULE_ENABLE_EMBEDDED_BROWSER)
		message(STATUS "  原因：当前 Qt 套件里找不到 WebView 模块，也没有 WebEngine；安装 qtwebview（或 qtwebengine）后重新配置即可启用")
	endif()
else()
	message(STATUS "内嵌浏览器后端：${SCHEDULE_BROWSER_BACKEND}")
endif()

# 子模块（按阶段逐步启用）
# 阶段 1: 核心层，纯 C++，仅依赖 Qt6::Core
add_subdirectory(src/core)
# 阶段 2: 基础设施层，SQLite 持久化与导入导出，依赖 core
add_subdirectory(src/data)
# 阶段 3: 引擎层，QML 绑定对象，依赖 core / data
add_subdirectory(src/engine)
# 阶段 4: UI 层，QML 模块，依赖 engine
add_subdirectory(src/ui)
# 阶段 5: 应用入口，可执行程序
add_subdirectory(src/app)
