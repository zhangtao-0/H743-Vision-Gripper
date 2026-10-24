@echo off
chcp 65001 >nul
cd /d "%~dp0"
if not exist ".venv\Scripts\python.exe" (
 echo 未找到预览环境，请按项目零基础手册创建.venv。
 pause
 exit /b 1
)
".venv\Scripts\python.exe" "start_viewer.py" %*
if errorlevel 1 pause
