# 免费签名与未签名产物说明

本项目只在 GitHub Release 发布，不上架任何应用商店。本文说明哪些签名可以免费完成、如何在 Windows 11 上创建，以及用户安装时可能遇到的系统警告。

## 一、免费签名能力

| 平台    | 免费方案             | 效果                                                |
| ------- | -------------------- | --------------------------------------------------- |
| Android | 自签名 keystore      | APK 可正常安装；Android 要求 APK 必须签名           |
| Linux   | GPG 分离签名（可选） | 提供 `.asc` 校验签名；不强制                        |
| Windows | 自签名 PFX           | CI 会签名，但 Windows 仍提示“未知发布者 / 不受信任” |
| macOS   | 无免费分发签名       | 产物未签名、未公证，Gatekeeper 会拦截               |
| iOS     | 无免费分发签名       | 只能出未签名 `.xcarchive`，普通用户无法安装         |

> Apple 的 Developer ID、Apple Distribution、Provisioning Profile、公证和 App Store Connect API Key 都依赖付费 Apple Developer Program。免费 Apple ID 只能本机 7 天调试，不能用于 CI 分发。

## 二、GitHub Secrets

免费方案需要配置以下 secrets：

| Secret                      | 说明                               |
| --------------------------- | ---------------------------------- |
| `ANDROID_KEYSTORE_BASE64`   | Android release keystore 的 Base64 |
| `ANDROID_KEYSTORE_PASSWORD` | keystore 口令                      |
| `ANDROID_KEY_ALIAS`         | 签名别名                           |
| `ANDROID_KEY_PASSWORD`      | 别名口令                           |
| `LINUX_GPG_PRIVATE_KEY`     | ASCII-armored GPG 私钥             |
| `LINUX_GPG_PASSPHRASE`      | GPG 私钥口令                       |
| `WINDOWS_PFX_BASE64`        | 自签名 PFX 的 Base64               |
| `WINDOWS_PFX_PASSWORD`      | PFX 口令                           |

Apple 相关 secrets 不配置，CI 会自动跳过签名/公证并输出 warning。

## 三、Windows 安装说明

Windows 产物可能是：

- 未签名 `Schedule.exe`
- 使用自签名证书签名的 `Schedule.exe`
- 自签名证书签名的安装包

无论哪种，只要没有购买受信任代码签名证书，Windows SmartScreen 都可能提示：

- “Windows 已保护你的电脑”
- “未知发布者”
- “此应用可能不安全”

这不是病毒，而是因为证书不受 Windows 信任。

### 用户如何继续安装

1. 如果出现“Windows 已保护你的电脑”：
    - 点击“更多信息”
    - 点击“仍要运行”

2. 如果出现“未知发布者”：
    - 点击“更多信息”后仍可运行
    - 或右键 `Schedule.exe` → 属性 → 常规 → 勾选“解除锁定” → 确定

3. 如果 SmartScreen 仍然阻止：
    - 仅建议在测试环境临时关闭 SmartScreen
    - 不要长期关闭系统安全功能

### 如何彻底消除警告

只有购买受信任的 OV / EV 代码签名证书，并用它重新签名，才能让 SmartScreen 不再提示。自签名证书无法消除警告。

## 四、macOS 安装说明

macOS 产物未签名、未公证时，Gatekeeper 会提示：

- “无法打开，因为 Apple 无法检查其是否包含恶意软件”
- “无法验证开发者”
- “已损坏，无法打开”

用户可尝试：

1. 右键点击 `Schedule.app` → 打开 → 仍要打开
2. 如果系统不允许，进入：
    - 系统设置 → 隐私与安全性 → 仍要打开
3. 高级用户可执行：

```bash
xattr -dr com.apple.quarantine /path/to/Schedule.app
```

macOS 15 Sequoia 及更新版本已移除 Ctrl+点击快捷绕过，必须去“系统设置 → 隐私与安全性”手动放行。

## 五、Linux 安装说明

- AppImage：

```bash
chmod +x Schedule-*.AppImage
./Schedule-*.AppImage
```

如果提示 FUSE 错误：

```bash
./Schedule-*.AppImage --appimage-extract-and-run
```

或安装 `libfuse2` / `libfuse2t64`。

- deb：

```bash
sudo apt install ./Schedule-*.deb
```

## 六、Android 安装说明

- 必须使用已签名 APK；未签名 APK 无法安装。
- 首次安装需允许“安装未知应用”：
    - 设置 → 安全 → 安装未知应用 → 选择文件管理器 / 浏览器 → 允许
- 如果使用自签名 keystore，APK 可以正常安装。
- 数据库在应用私有目录，卸载会删除，请先使用应用内“导出”功能备份。

## 七、iOS 说明

- 未签名 `.xcarchive` 无法安装。
- 没有付费 Apple Developer Program 时，不建议发布 iOS 二进制。
- 建议只发布源码，或让用户自行用 Xcode 本机调试。

## 八、安全提醒

- 所有私钥、keystore、PFX、GPG 私钥只放在 GitHub Secrets。
- 不要提交到仓库，不要打印到日志。
- 本地生成后，建议删除临时 `.jks`、`.pfx`、`.asc` 文件，或放到加密磁盘。
- 自签名证书仅用于测试和开源分发说明，不适合商业软件。

---

## 七、最后提醒

1. **Android 必须配置 keystore**，否则 CI 只出未签名 APK，用户无法安装。
2. **Windows 自签名 PFX 不会消除 SmartScreen 警告**。
3. **Linux GPG 可选**，不配就只提供 `sha256sum`。
4. **macOS / iOS 不要配 Apple secrets**，让 CI 跳过签名/公证，否则会因缺少 secrets 在 `sign=true` 时失败。
5. `release-all.yml` 默认 `sign=false`，只要对应 secrets 齐全就会签名；不想让某个平台签名，就不要配置该平台的 secrets。
