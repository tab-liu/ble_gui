@echo off
REM 如果需要，可把下面的 python 路径改为你的 python.exe 的绝对路径
SET PY=python
"%PY%" "%~dp0udp_receiver.py" %*
echo.
pause