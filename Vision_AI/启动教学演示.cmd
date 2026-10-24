@echo off
if exist "%~dp0.venv\Scripts\python.exe" (
  "%~dp0.venv\Scripts\python.exe" "%~dp0tools\study.py" demo
) else (
  "%~dp0..\Host\vision_gripper\.venv\Scripts\python.exe" "%~dp0tools\study.py" demo
)
pause
