@echo off
setlocal

REM Lấy thư mục hiện tại (thư mục chứa file .bat)
set "CURRENT_DIR=%~dp0"

REM Tìm thư mục libs trong thư mục hiện tại
set "LIBS_PATH=%CURRENT_DIR%libs"

REM Kiểm tra xem thư mục libs có tồn tại không
if not exist "%LIBS_PATH%" (
    echo ❌ Khong tim thay thu muc "libs" trong: %CURRENT_DIR%
    pause
    exit /b
)

echo 🔍 Tim thay thu muc libs: %LIBS_PATH%

REM Kiểm tra xem PATH của user da co libs chua
for /f "tokens=2* delims=  " %%A in ('reg query "HKCU\Environment" /v Path 2^>nul') do set "UserPath=%%B"

echo PATH hien tai cua user:
echo %UserPath%

echo.
echo 🔧 Dang kiem tra va them vao PATH neu chua co...

echo %UserPath% | find /i "%LIBS_PATH%" >nul
if %errorlevel%==0 (
    echo ✅ Duong dan libs da ton tai trong PATH.
) else (
    echo ➕ Dang them "%LIBS_PATH%" vao PATH...
    setx PATH "%UserPath%;%LIBS_PATH%"
    echo ✅ Da them thanh cong!
)

echo.
pause
endlocal
