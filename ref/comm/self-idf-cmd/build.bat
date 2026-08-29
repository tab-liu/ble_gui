@echo off
:: 加载配置
call "%~dp0config.bat"

:: 初始化ESP-IDF
cd /d %PROJECT_DIR%
call %IDF_PATH%\export.bat

:: 参数覆盖
set MODEL=%1

:: 构建目录映射
if "%MODEL%"=="" (
    idf.py build
    exit /b 1
) 

set BUILD_DIR=build/%MODEL%
echo Cmd: idf.py -B %BUILD_DIR% -D SDKCONFIG=./sdkconfig_%MODEL% build
idf.py -B %BUILD_DIR% -D SDKCONFIG=./sdkconfig_%MODEL% build