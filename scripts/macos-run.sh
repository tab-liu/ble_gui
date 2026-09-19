#!/usr/bin/env bash
# cargo run 的 macOS runner：
# 1) 把二进制装进临时 .app（TCC 定位服务只稳定认 Bundle）
# 2) ad-hoc 签名并绑定 Info.plist
# 否则系统不弹定位窗，定位列表里也不会出现「BLE Modbus 工具」。
set -euo pipefail

BIN="${1:?}"
shift

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PLIST_SRC="$ROOT/assets/macos_info.plist"
APP_DIR="$(dirname "$BIN")/BLE Modbus 工具.app"
CONTENTS="$APP_DIR/Contents"
MACOS_DIR="$CONTENTS/MacOS"
APP_BIN="$MACOS_DIR/ble_gui"

mkdir -p "$MACOS_DIR"
# 复制可执行文件（保持与 cargo 产物同步）
cp -f "$BIN" "$APP_BIN"
chmod +x "$APP_BIN"

# 完整 Info.plist（Bundle 用）
cat > "$CONTENTS/Info.plist" <<'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>CFBundleDevelopmentRegion</key>
	<string>zh-Hans</string>
	<key>CFBundleExecutable</key>
	<string>ble_gui</string>
	<key>CFBundleIdentifier</key>
	<string>com.ble-gui.tool</string>
	<key>CFBundleInfoDictionaryVersion</key>
	<string>6.0</string>
	<key>CFBundleName</key>
	<string>BLE Modbus 工具</string>
	<key>CFBundleDisplayName</key>
	<string>BLE Modbus 工具</string>
	<key>CFBundlePackageType</key>
	<string>APPL</string>
	<key>CFBundleShortVersionString</key>
	<string>0.1.0</string>
	<key>CFBundleVersion</key>
	<string>1</string>
	<key>LSMinimumSystemVersion</key>
	<string>11.0</string>
	<key>NSHighResolutionCapable</key>
	<true/>
	<key>NSLocationWhenInUseUsageDescription</key>
	<string>需要定位权限以扫描附近 WiFi 名称（SSID），用于设备配网。</string>
	<key>NSLocationUsageDescription</key>
	<string>需要定位权限以扫描附近 WiFi 名称（SSID），用于设备配网。</string>
</dict>
</plist>
EOF

# 同步一份到仓库模板（若存在）
if [[ -f "$PLIST_SRC" ]]; then
  :
fi

if command -v codesign >/dev/null 2>&1; then
  codesign --force --deep --sign - --identifier com.ble-gui.tool "$APP_DIR" >/dev/null 2>&1 || \
    codesign --force --sign - --identifier com.ble-gui.tool "$APP_BIN" >/dev/null 2>&1 || true
fi

exec "$APP_BIN" "$@"
