#!/usr/bin/env bash
# cargo run 的 macOS runner：
# 1) 把二进制装进临时 .app（TCC 定位服务只稳定认 Bundle）
# 2) 嵌入 AppIcon.icns（程序坞图标）
# 3) ad-hoc 签名并绑定 Info.plist
set -euo pipefail

BIN="${1:?}"
shift

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PLIST_SRC="$ROOT/assets/macos_info.plist"
ICNS_SRC="$ROOT/assets/app.icns"
APP_DIR="$(dirname "$BIN")/BLE Modbus 工具.app"
CONTENTS="$APP_DIR/Contents"
MACOS_DIR="$CONTENTS/MacOS"
RES_DIR="$CONTENTS/Resources"
APP_BIN="$MACOS_DIR/ble_gui"

mkdir -p "$MACOS_DIR" "$RES_DIR"
cp -f "$BIN" "$APP_BIN"
chmod +x "$APP_BIN"

if [[ -f "$ICNS_SRC" ]]; then
  cp -f "$ICNS_SRC" "$RES_DIR/AppIcon.icns"
elif [[ -f "$ROOT/assets/app_icon.png" ]]; then
  # 无 icns 时至少放一张 png，部分环境可作回退
  cp -f "$ROOT/assets/app_icon.png" "$RES_DIR/AppIcon.png"
fi

if [[ -f "$PLIST_SRC" ]]; then
  cp -f "$PLIST_SRC" "$CONTENTS/Info.plist"
else
  cat > "$CONTENTS/Info.plist" <<'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>CFBundleExecutable</key>
	<string>ble_gui</string>
	<key>CFBundleIdentifier</key>
	<string>com.ble-gui.tool</string>
	<key>CFBundleName</key>
	<string>BLE Modbus 工具</string>
	<key>CFBundlePackageType</key>
	<string>APPL</string>
	<key>CFBundleIconFile</key>
	<string>AppIcon</string>
	<key>NSLocationWhenInUseUsageDescription</key>
	<string>需要定位权限以扫描附近 WiFi 名称（SSID），用于设备配网。</string>
</dict>
</plist>
EOF
fi

if command -v codesign >/dev/null 2>&1; then
  codesign --force --deep --sign - --identifier com.ble-gui.tool "$APP_DIR" >/dev/null 2>&1 || \
    codesign --force --sign - --identifier com.ble-gui.tool "$APP_BIN" >/dev/null 2>&1 || true
fi

exec "$APP_BIN" "$@"
