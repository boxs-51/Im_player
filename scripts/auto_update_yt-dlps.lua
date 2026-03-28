-- auto_update_yt-dlp.lua
-- Full stable version (Windows-safe + fallback + log rotation)

local utils = require 'mp.utils'
local opt = require 'mp.options'
local log_initialized = false
-- === Cấu hình ===
local options = {
    auto_update = true,
    log_file = "log/yt-dlp-update.log",
    max_log_size = 1024 * 1024 * 5,
}
opt.read_options(options, "auto_update_yt-dlp")

-- === Đường dẫn ===

local is_windows = package.config:sub(1,1) == "\\"
local is_mac = not is_windows and (os.getenv("OSTYPE") == "darwin" or os.getenv("OSTYPE") == "macos")
local is_linux = not is_windows and not is_mac

local sep = package.config:sub(1,1)

local function join(a, b)
    if a:sub(-1) == sep then
        return a .. b
    end
    return a .. sep .. b
end

local app_dir = mp.find_config_file(".") or utils.getcwd()
local ytdlp_exe = join(app_dir, "yt-dlp.exe")
local exe_dir = mp.get_property("working-directory") or utils.getcwd()
local log_dir = join(script_dir, "log")
local log_path = join(log_dir, "yt-dlp-update.log")

-- === Helpers ===

local function ensure_log_dir_safe()
    if utils.file_info(log_dir) then return true end

    local res
    if is_windows then
        -- Sử dụng PowerShell để tạo thư mục (ổn định hơn cmd trên một số máy)
        res = utils.subprocess({
            args = {"powershell", "-NoProfile", "-Command", "New-Item", "-ItemType", "Directory", "-Force", "-Path", log_dir},
            cancellable = false
        })
    else
        res = utils.subprocess({
            args = {"mkdir", "-p", log_dir},
            cancellable = false
        })
    end

    if not res or res.status ~= 0 then
        -- Nếu vẫn lỗi, thử ghi log thẳng ra thư mục chứa script (không cần folder log)
        log_path = script_dir .. sep .. "yt-dlp-update.log"
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

local function log(msg)
    if not ensure_log_dir_safe() then
        mp.msg.error("Cannot create log directory")
        return
    end

    trim_log()

    -- Chỉ lần đầu thì ghi đè (reset file)
    local mode = "a"
    if not log_initialized then
        mode = "w"
        log_initialized = true
        f:write("\n===== NEW SESSION =====\n")
    end

    local f = io.open(log_path, mode)
    if f then
        f:write(os.date("[%Y-%m-%d %H:%M:%S] ") .. msg .. "\n")
        f:close()
    else
        mp.msg.error("Cannot open log file: " .. log_path)
    end
end

local function send_status_node(status, current, latest, extra)
    local node = {
        status = status,
        current_version = current or "unknown",
        latest_version = latest or "unknown",
        extra_info = extra or ""
    }
    local json_str = utils.format_json(node)
    mp.command_native({
        "script-message",
        "yt-dlp-status-node", json_str
    })
    log("Status Sent: " .. status .. " | Current: " .. node.current_version .. " | Latest: " .. node.latest_version)
end

local function osd(msg)
    mp.osd_message("[yt-dlp] " .. msg, 5)
end

local function normalize_version(v)
    if not v then return nil end
    local cleaned = v:gsub("[^0-9%.]", "")
    if cleaned == "" then return nil end
    return cleaned
end

local function run_and_log(args)
    log("Run: " .. table.concat(args, " "))
    local res = utils.subprocess({ args = args, cancellable = false })
    if res.status ~= 0 then
        log("Error: " .. (res.stderr or "unknown"))
    end
    return res
end

-- === Download (FULL FALLBACK) ===
local function download_file(url, out)
    -- ===== Windows =====
    if is_windows then
        -- 1. curl (Win10+ thường có)
        local res = run_and_log({"curl", "-L", "-o", out, url})
        if res.status == 0 then return true end

        -- 2. PowerShell (fallback mạnh nhất Windows)
        res = run_and_log({
            "powershell", "-Command",
            "Invoke-WebRequest -Uri '" .. url .. "' -OutFile '" .. out .. "'"
        })
        if res.status == 0 then return true end

        -- 3. bitsadmin (Windows cũ)
        res = run_and_log({
            "bitsadmin", "/transfer", "yt", url, out
        })
        return res.status == 0
    end

    -- ===== Linux =====
    if is_linux then
        -- 1. curl
        local res = run_and_log({"curl", "-L", "-o", out, url})
        if res.status == 0 then return true end

        -- 2. wget
        res = run_and_log({"wget", "-O", out, url})
        if res.status == 0 then return true end

        -- 3. busybox wget (máy minimal)
        res = run_and_log({"busybox", "wget", "-O", out, url})
        return res.status == 0
    end

    -- ===== macOS =====
    if is_mac then
        -- macOS luôn có curl
        local res = run_and_log({"curl", "-L", "-o", out, url})
        if res.status == 0 then return true end

        -- fallback wget nếu user có brew
        res = run_and_log({"wget", "-O", out, url})
        return res.status == 0
    end

    return false
end

-- === Core Functions ===

local function get_local_version()
    if utils.file_info(ytdlp_exe) then
        local res = run_and_log({ytdlp_exe, "--version"})
        if res.status == 0 and res.stdout then
            return normalize_version(res.stdout)
        end
    end
    return nil
end

local function get_github_version()
    local api = "https://api.github.com/repos/yt-dlp/yt-dlp/releases/latest"

    local function parse(json)
        if not json then return nil end
        local ver = json:match('"tag_name":%s*"v?([%d%.]+)"')
        return normalize_version(ver)
    end

    -- Windows
    if is_windows then
        local res = run_and_log({"curl", "-s", api})
        if res.status == 0 then
            local v = parse(res.stdout)
            if v then return v end
        end

        -- PowerShell fallback
        res = run_and_log({
            "powershell", "-Command",
            "(Invoke-WebRequest -Uri '" .. api .. "').Content"
        })
        if res.status == 0 then
            return parse(res.stdout)
        end
    end

    -- Linux / macOS
    local res = run_and_log({"curl", "-s", api})
    if res.status == 0 then
        local v = parse(res.stdout)
        if v then return v end
    end

    res = run_and_log({"wget", "-qO-", api})
    if res.status == 0 then
        return parse(res.stdout)
    end

    log("GitHub API failed all fallbacks")
    return nil
end

local function download_ytdlp(local_v, remote_v)
    local url = "https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp.exe"
    local tmp = ytdlp_exe .. ".tmp"

    send_status_node("downloading", local_v, remote_v, "Downloading from GitHub...")
    osd("Downloading latest yt-dlp.exe...")

    if not download_file(url, tmp) then
        return false
    end

    if utils.file_info(tmp) then
        os.remove(ytdlp_exe)
        os.rename(tmp, ytdlp_exe)
        return true
    end

    return false
end

-- === Main Task ===

local function perform_update_logic()
    log("=== Check Start (GitHub Only) ===")

    local local_ver = get_local_version()
    local remote_ver = get_github_version()

    if not local_ver then
        if download_ytdlp("none", remote_ver) then
            local new_v = get_local_version()
            send_status_node("updated", new_v, remote_ver, "Fresh install success")
            osd("Installed: " .. (new_v or "Done"))
        else
            send_status_node("error", "none", remote_ver, "Failed to download EXE")
            osd("Install failed!")
        end

    elseif not remote_ver then
        send_status_node("error", local_ver, "unknown", "Network error or GitHub API limit")

    elseif local_ver == remote_ver then
        send_status_node("latest", local_ver, remote_ver, "Up to date")

    else
        if options.auto_update then
            if download_ytdlp(local_ver, remote_ver) then
                send_status_node("updated", remote_ver, remote_ver, "Update success")
                osd("Updated to " .. remote_ver)
            else
                send_status_node("error", local_ver, remote_ver, "Update download failed")
                osd("Update failed")
            end
        else
            send_status_node("outdated", local_ver, remote_ver, "Auto-update disabled")
            osd("New version available: " .. remote_ver)
        end
    end

    if utils.file_info(ytdlp_exe) then
        mp.set_property("ytdl_path", ytdlp_exe)
    end

    log("=== Check End ===")
end

-- === Init ===
ensure_log_dir_safe()
mp.add_timeout(1, perform_update_logic)
mp.register_script_message("check_update", perform_update_logic)