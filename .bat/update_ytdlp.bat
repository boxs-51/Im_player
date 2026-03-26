@echo off
setlocal

set YTDLP=yt-dlp.exe
set URL=https://github.com/yt-dlp/yt-dlp-nightly-builds/releases/latest/download/yt-dlp.exe


echo Checking yt-dlp...

echo Downloading latest yt-dlp...
curl -L -o "%YTDLP%.new" "%URL%"
if exist "%YTDLP%.new" (
    del /f /q "%YTDLP%"
    move /y "%YTDLP%.new" "%YTDLP%"
    exit /b 0
)

echo Update failed
exit /b 1
