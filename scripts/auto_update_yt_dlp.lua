local utils = require 'mp.utils'
local opt = require 'mp.options'
local msg = require 'mp.msg' -- Dùng mp.msg để log ra console của mpv

-- ================= CONFIG =================
local options = {
    auto_update = true,
    log_file = "log/yt-dlp-update.log",
    max_log_size = 1024 * 1024 * 5,
    retry_count = 3,
    timeout = 30,
}
opt.read_options(options, "auto_update_yt-dlp")

-- ================= PLATFORM =================
local is_windows = package.config:sub(1,1) == "\\"
local sep = package.config:sub(1,1)

local function join(a,b)
    if not a or not b then return "" end
    if a:sub(-1) == sep then return a..b end
    return a..sep..b
end

local exe_path = mp.get_property("mpv-executable-path") or ""
local exe_dir = exe_path:match("^(.*)[\\/]") or mp.get_script_directory()
local script_dir = mp.get_script_directory() or "."

local ytdlp_name = is_windows and "yt-dlp.exe" or "yt-dlp"
local ytdlp_exe = join(exe_dir, ytdlp_name)

local log_dir = join(exe_dir, "log")
local log_path = join(log_dir, "yt-dlp-update.log")

-- ================= LOG =================
local log_initialized = false

local function ensure_log_dir()
    if utils.file_info(log_dir) then return true end

    local res = utils.subprocess({
        args = is_windows and
            {"powershell","-Command","New-Item -ItemType Directory -Force -Path '"..log_dir.."'"}
            or {"mkdir","-p",log_dir},
        timeout = options.timeout
    })

    if not res or res.status ~= 0 then
        log_dir = script_dir
        log_path = join(script_dir, "yt-dlp-update.log")
        return false
    end
    return true
end

local function trim_log()
    local info = utils.file_info(log_path)
    if info and info.size > options.max_log_size then
        os.remove(log_path)
    end
end

local function log(text, is_error)
    -- In ra mpv console
    if is_error then
        msg.error(text)
    else
        msg.info(text)
    end

    ensure_log_dir()
    trim_log()

    local mode = log_initialized and "a" or "w"
    log_initialized = true

    local f, err = io.open(log_path, mode)
    if f then
        if mode == "w" then
            f:write("\n===== NEW SESSION =====\n")
        end
        local prefix = is_error and "[ERROR] " or "[INFO] "
        f:write(os.date("[%Y-%m-%d %H:%M:%S] ")..prefix..text.."\n")
        f:close()
    else
        msg.warn("Cannot write to log file: "..tostring(err))
    end
end

-- ================= UTILS =================
local function run(args)
    log("Run: "..table.concat(args," "))
    local res = utils.subprocess({
        args = args,
        timeout = options.timeout,
        cancellable = false
    })
    
    -- Xử lý an toàn khi res bị nil
    if not res then
        log("Error: Subprocess returned nil (Timeout or Executable not found)", true)
        return { status = -1, stdout = "", stderr = "Subprocess nil" }
    end

    if res.status ~= 0 then
        log("Error (Status "..tostring(res.status).."): "..(res.stderr or "unknown error"), true)
    end
    return res
end

-- ================= VERSION =================
local function normalize(v)
    if not v then return nil end
    return v:match("[%d%.]+")
end

local function vtable(v)
    local t = {}
    if not v then return t end
    for n in v:gmatch("%d+") do
        table.insert(t, tonumber(n))
    end
    return t
end

local function compare_version(a,b)
    local ta, tb = vtable(a), vtable(b)
    for i=1, math.max(#ta,#tb) do
        local x = ta[i] or 0
        local y = tb[i] or 0
        if x < y then return -1 end
        if x > y then return 1 end
    end
    return 0
end

-- ================= DOWNLOAD =================
local function valid_file(path)
    local info = utils.file_info(path)
    return info and info.size > 100000
end

local function download(url, out)
    for i = 1, options.retry_count do
        log("Download attempt " .. i .. "/" .. options.retry_count)

        -- Thử Curl với User-Agent
        local res = run({"curl", "-L", "-H", "User-Agent: mpv-script", "-o", out, url})

        -- Nếu Curl thất bại, thử dùng PowerShell Bật TLS 1.2
        if not res or res.status ~= 0 or not valid_file(out) then
            log("Curl download failed. Retrying download with PowerShell...")
            local ps_dl = string.format(
                "[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; " ..
                "Invoke-WebRequest -Uri '%s' -OutFile '%s' -Headers @{'User-Agent'='Mozilla/5.0'}",
                url, out
            )
            res = run({"powershell", "-NoProfile", "-Command", ps_dl})
        end

        if valid_file(out) then
            return true
        end
    end
    return false
end

-- ================= SAFE UPDATE =================
local function safe_replace(tmp, target)
    local backup = target..".old"

    os.remove(backup)
    os.rename(target, backup)

    local ok, err = os.rename(tmp, target)
    if not ok then
        log("Rename failed: "..tostring(err), true)
        os.rename(backup, target) -- Rollback
        return false
    end

    os.remove(backup)
    return true
end

-- ================= VERSION FETCH =================
local function get_local()
    if not utils.file_info(ytdlp_exe) then return nil end
    local res = run({ytdlp_exe, "--version"})
    return normalize(res.stdout)
end

local function get_remote()
    local api = "https://api.github.com/repos/yt-dlp/yt-dlp/releases/latest"
    
    -- Cách 1: Thử Curl kèm User-Agent Header
    local res = run({"curl", "-s", "-L", "-H", "User-Agent: mpv-script", api})

    -- Cách 2: Nếu Curl lỗi, Fallback sang PowerShell (Bật ép TLS 1.2 + User-Agent)
    if not res or res.status ~= 0 or not res.stdout or res.stdout == "" then
        log("curl failed. Retrying with PowerShell (TLS 1.2 forced)...")
        
        local ps_cmd = string.format(
            "[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; " ..
            "$ProgressPreference='SilentlyContinue'; " ..
            "try { $r = Invoke-RestMethod -Uri '%s' -Headers @{'User-Agent'='mpv-script'}; Write-Output $r.tag_name } catch { Write-Error $_ }",
            api
        )
        res = run({"powershell", "-NoProfile", "-Command", ps_cmd})
    end

    -- Cách 3: Nếu cả API đều bị chặn, cào thẳng tag_name từ trang GitHub Release HTML (Fallback cuối)
    if not res or res.status ~= 0 or not res.stdout or res.stdout == "" then
        log("API fetch failed. Fallback to scraping GitHub HTML page...")
        local html_url = "https://github.com/yt-dlp/yt-dlp/releases/latest"
        local ps_html_cmd = string.format(
            "[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; " ..
            "(Invoke-WebRequest -Uri '%s' -Headers @{'User-Agent'='Mozilla/5.0'}).BaseResponse.ResponseUri.AbsoluteUri",
            html_url
        )
        res = run({"powershell", "-NoProfile", "-Command", ps_html_cmd})
    end

    if not res or res.status ~= 0 or not res.stdout then 
        log("CRITICAL: All fetch methods failed (Network/TLS/Firewall issue)", true)
        return nil 
    end

    local tag = res.stdout:match('"tag_name"%s*:%s*"v?([%d%.]+)"') or normalize(res.stdout)
    if not tag then
        log("Failed to parse version tag from response: "..tostring(res.stdout), true)
    end
    return normalize(tag)
end

-- ================= MAIN LOGIC =================
local function update_logic()
    log("=== CHECK START ===")

    if not options.auto_update then
        log("Auto update is disabled in options.")
        return
    end

    local local_v = get_local()
    local remote_v = get_remote()

    if not remote_v then
        log("Cannot fetch remote version. Aborting update.", true)
        return
    end

    if not local_v then
        log("No local version found -> fresh install")
    elseif compare_version(local_v, remote_v) >= 0 then
        log("Already on latest version: "..local_v)
        return
    end

    log("New version detected: "..remote_v.." (Current: "..(local_v or "None")..")")

    local url = "https://github.com/yt-dlp/yt-dlp/releases/latest/download/"..ytdlp_name
    local tmp = ytdlp_exe..".tmp"

    if not download(url, tmp) then
        log("Download failed after "..options.retry_count.." attempts", true)
        return
    end

    if not safe_replace(tmp, ytdlp_exe) then
        log("Update failed during file replacement (rollback applied)", true)
        return
    end

    if not is_windows then
        run({"chmod", "+x", ytdlp_exe})
    end

    log("Successfully updated yt-dlp to "..remote_v)

    mp.set_property("ytdl_path", ytdlp_exe)

    log("=== CHECK END ===")
end

-- ================= EXCEPTION HANDLER (TRY-CATCH) =================
local function safe_update()
    -- xpcall đóng vai trò như try-catch trong các ngôn ngữ khác
    local status, err = xpcall(update_logic, debug.traceback)
    
    if not status then
        -- Nếu có lỗi xảy ra (Runtime Error), đoạn này sẽ hứng lỗi và Stack Trace
        log("\n----------------------------------------", true)
        log("CRITICAL EXCEPTION / RUNTIME ERROR CAUGHT:", true)
        log(tostring(err), true)
        log("----------------------------------------\n", true)
    end
end

-- ================= INIT =================
mp.add_timeout(1, safe_update)
mp.register_script_message("check_update_yt-dlp", safe_update)