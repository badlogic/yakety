@echo off
REM Windows batch script to start WSL and set up SSH port forwarding
REM Run this script as Administrator

echo === WSL SSH Startup Script ===
echo.

REM Start WSL in the background
echo Starting WSL...
wsl --exec echo "WSL is running" > nul 2>&1

REM Wait a moment for WSL to fully start
timeout /t 2 /nobreak > nul

REM Get WSL IP address
echo Getting WSL IP address...
for /f "tokens=1" %%i in ('wsl hostname -I') do set WSL_IP=%%i

if "%WSL_IP%"=="" (
    echo ERROR: Could not get WSL IP address. Is WSL running?
    pause
    exit /b 1
)

echo WSL IP: %WSL_IP%
echo.

REM Check if running as administrator
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo ERROR: This script must be run as Administrator!
    echo Right-click the script and select "Run as administrator"
    pause
    exit /b 1
)

REM Remove all existing port forwarding rules
echo Removing existing port forwarding rules...
netsh interface portproxy reset

REM Add port forwarding for SSH (port 22)
echo Setting up port forwarding for SSH (port 22)...
netsh interface portproxy add v4tov4 listenport=22 listenaddress=0.0.0.0 connectport=22 connectaddress=%WSL_IP%

REM Create firewall rule if it doesn't exist
echo Checking firewall rules...
netsh advfirewall firewall show rule name="WSL SSH Port 22" >nul 2>&1
if %errorLevel% neq 0 (
    echo Creating firewall rule for port 22...
    netsh advfirewall firewall add rule name="WSL SSH Port 22" dir=in action=allow protocol=TCP localport=22
) else (
    echo Firewall rule already exists
)

REM Show current port forwarding rules
echo.
echo Current port forwarding rules:
netsh interface portproxy show v4tov4

REM Get Windows IP address
echo.
echo Your Windows host IP addresses:
echo ================================
for /f "tokens=2 delims=:" %%i in ('ipconfig ^| findstr /i "IPv4"') do echo %%i

echo.
echo === Setup Complete! ===
echo To connect from a remote machine:
echo ssh username@[windows-ip-address]
echo.
pause