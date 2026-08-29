@echo off
:: 加载配置
call "%~dp0config.bat"

:: 初始化ESP-IDF
cd /d %PROJECT_DIR%
call %IDF_PATH%\export.bat

:: 参数覆盖
set MODEL=%1
set PORT=%2
if "%PORT%"=="" set PORT=%DEFAULT_PORT%

:: 构建目录映射
if "%MODEL%"=="" (
    set BUILD_DIR=build
) else (
    set BUILD_DIR=build\%MODEL%
)

:: 烧录
cd %PROJECT_DIR%\%BUILD_DIR%
if errorlevel 1 (
    echo FileNotFoundError : %PROJECT_DIR%\%BUILD_DIR%
    exit /b 1
)

python -m esptool --chip esp32s3 -p %PORT% -b 460800 --before default_reset --after hard_reset write_flash "@flash_args"
