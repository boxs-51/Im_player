@echo off
setlocal enabledelayedexpansion

REM ============================================
REM 🚀 FFmpeg Auto Downloader & Extractor
REM Tác giả: ChatGPT (GPT-5)
REM Mục tiêu: Tải & giải nén FFmpeg vào thư mục local
REM ============================================

:: Thư mục đích
set "FFMPEG_DIR=%cd%\ffmpeg"

:: URL tải về (bản Windows chính thức từ gyan.dev)
set "FFMPEG_URL=https://www.gyan.dev/ffmpeg/builds/ffmpeg-release-essentials.zip"
set "FFMPEG_ZIP=ffmpeg.zip"

echo ============================================
echo 🧩 Đang tải FFmpeg từ:
echo %FFMPEG_URL%
echo ============================================

:: Nếu có sẵn ffmpeg.exe thì bỏ qua tải
if exist "%FFMPEG_DIR%\bin\ffmpeg.exe" (
    echo ✅ FFmpeg đã tồn tại tại: %FFMPEG_DIR%\bin
    goto :done
)

:: Tải file zip
powershell -Command "Invoke-WebRequest -Uri '%FFMPEG_URL%' -OutFile '%FFMPEG_ZIP%'" || (
    echo ❌ Không thể tải FFmpeg. Kiểm tra mạng hoặc URL.
    exit /b 1
)

:: Giải nén
echo 🧰 Đang giải nén...
powershell -Command "Expand-Archive -Force '%FFMPEG_ZIP%' '%FFMPEG_DIR%_tmp'" || (
    echo ❌ Giải nén thất bại.
    exit /b 1
)

:: Di chuyển thư mục con (do FFmpeg có subfolder tên dài)
for /d %%d in ("%FFMPEG_DIR%_tmp\ffmpeg-*") do (
    move "%%d" "%FFMPEG_DIR%" >nul
)

:: Dọn dẹp
rmdir /s /q "%FFMPEG_DIR%_tmp"
del "%FFMPEG_ZIP%"

echo ✅ FFmpeg đã được tải và giải nén vào:
echo     %FFMPEG_DIR%

:: Thêm vào PATH tạm thời cho session hiện tại
set "PATH=%FFMPEG_DIR%\bin;%PATH%"
echo 🧠 PATH đã được cập nhật tạm thời.

:done
echo.
echo 🔍 Kiểm tra phiên bản FFmpeg:
ffmpeg -version
echo.
echo ✅ Hoàn tất!
pause
