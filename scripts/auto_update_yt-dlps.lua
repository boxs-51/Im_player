-- auto_update_yt-dlp.lua
-- Tối ưu: Chỉ dùng GitHub Release EXE + Gửi status JSON cho giao diện ImGui

local utils = require 'mp.utils'
local opt = require 'mp.options'

-- === Cấu hình ===
local options = {
    auto_update = true,
    log_file = "log/yt-dlp-update.log",
    max_log_size = 1024 * 1024 * 5,
}
opt.read_options(options, "auto_update_yt-dlp")

-- === Đường dẫn ===
local app_dir = utils.getcwd() 
local ytdlp_exe = app_dir .. "/yt-dlp.exe"
local script_dir = mp.get_script_directory() or "D:/MPVPLAYER/imgui_player/scriptS"
local log_path = script_dir .. "/" .. options.log_file

-- === Helpers ===
local function log(msg)
    local f = io.open(log_path, "a")
    if f then
        f:write(os.date("[%Y-%m-%d %H:%M:%S] ") .. msg .. "\n")
        f:close()
    end
end

-- Gửi trạng thái JSON qua client-message cho giao diện ImGui
local function send_status_node(status, current, latest, extra)
    local node = {
        status = status,             -- "latest", "updated", "outdated", "error", "downloading"
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
    return (v or ""):gsub("[^0-9%.]", "")
end

local function run_and_log(args)
    log("Run: " .. table.concat(args, " "))
    return utils.subprocess({ args = args, cancellable = false })
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
    local res = run_and_log({"curl", "-s", "https://api.github.com/repos/yt-dlp/yt-dlp/releases/latest"})
    if res.status == 0 and res.stdout then
        return normalize_version(res.stdout:match('"tag_name":%s*"v?([%d%.]+)"'))
    end
    return nil
end

local function download_ytdlp(local_v, remote_v)
    local url = "https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp.exe"
    
    send_status_node("downloading", local_v, remote_v, "Downloading from GitHub...")
    osd("Downloading latest yt-dlp.exe...")
    
    local res = run_and_log({"curl", "-L", "-o", ytdlp_exe, url})
    if res.status ~= 0 then
        res = run_and_log({"wget", "-O", ytdlp_exe, url})
    end

    if res.status == 0 and utils.file_info(ytdlp_exe) then
        return true
    end
    return false
end

-- === Main Task ===
local function perform_update_logic()
    log("=== Check Start (GitHub Only) ===")
    
    local local_ver = get_local_version()
    local remote_ver = get_github_version()

    -- Trường hợp 1: Không có file exe (Tải mới)
    if not local_ver then
        if download_ytdlp("none", remote_ver) then
            local new_v = get_local_version()
            send_status_node("updated", new_v, remote_ver, "Fresh install success")
            osd("Installed: " .. (new_v or "Done"))
        else
            send_status_node("error", "none", remote_ver, "Failed to download EXE")
            osd("Install failed!")
        end

    -- Trường hợp 2: Có file, nhưng không lấy được bản mới từ GitHub (Lỗi mạng)
    elseif not remote_ver then
        send_status_node("error", local_ver, "unknown", "Network error or GitHub API limit")
        log("Could not fetch remote version.")

    -- Trường hợp 3: Đã là bản mới nhất
    elseif local_ver == remote_ver then
        send_status_node("latest", local_ver, remote_ver, "Up to date")
        log("Already latest.")

    -- Trường hợp 4: Có bản mới
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

    -- Luôn đảm bảo MPV dùng đúng path này
    if utils.file_info(ytdlp_exe) then
        mp.set_property("ytdl_path", ytdlp_exe)
    end
    log("=== Check End ===")
end

-- Chạy sau 1s khi MPV mở
mp.add_timeout(1, perform_update_logic)

-- Lắng nghe gọi thủ công từ giao diện
mp.register_script_message("check_update", perform_update_logic)