local utils = require 'mp.utils'
local opt = require 'mp.options'

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
    if a:sub(-1) == sep then return a..b end
    return a..sep..b
end

local exe_path = mp.get_property("mpv-executable-path")
local exe_dir = exe_path:match("^(.*)[\\/]")
local script_dir = mp.get_script_directory()

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

local function log(msg)
    ensure_log_dir()
    trim_log()

    local mode = log_initialized and "a" or "w"
    log_initialized = true

    local f = io.open(log_path, mode)
    if f then
        if mode == "w" then
            f:write("\n===== NEW SESSION =====\n")
        end
        f:write(os.date("[%Y-%m-%d %H:%M:%S] ")..msg.."\n")
        f:close()
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
    if not res or res.status ~= 0 then
        log("Error: "..(res and res.stderr or "nil"))
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
    for i=1, options.retry_count do
        log("Download attempt "..i)

        local res = run(is_windows and
            {"curl","-L","-o",out,url} or
            {"curl","-L","-o",out,url}
        )

        if res.status == 0 and valid_file(out) then
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
        log("Rename failed: "..tostring(err))
        os.rename(backup, target)
        return false
    end

    os.remove(backup)
    return true
end

-- ================= VERSION FETCH =================
local function get_local()
    if not utils.file_info(ytdlp_exe) then return nil end
    local res = run({ytdlp_exe,"--version"})
    return normalize(res.stdout)
end

local function get_remote()
    local api = "https://api.github.com/repos/yt-dlp/yt-dlp/releases/latest"
    local res = run({"curl","-s",api})

    if res.status ~= 0 then return nil end

    log("GitHub raw: "..(res.stdout or "nil"))

    return normalize(res.stdout:match('"tag_name"%s*:%s*"v?([%d%.]+)"'))
end

-- ================= MAIN =================
local function update()
    log("=== CHECK START ===")

    local local_v = get_local()
    local remote_v = get_remote()

    if not remote_v then
        log("Cannot fetch remote version")
        return
    end

    if not local_v then
        log("No local version → fresh install")
    elseif compare_version(local_v, remote_v) >= 0 then
        log("Already latest: "..local_v)
        return
    end

    local url = "https://github.com/yt-dlp/yt-dlp/releases/latest/download/"..ytdlp_name
    local tmp = ytdlp_exe..".tmp"

    if not download(url, tmp) then
        log("Download failed")
        return
    end

    if not safe_replace(tmp, ytdlp_exe) then
        log("Update failed (rollback done)")
        return
    end

    if not is_windows then
        run({"chmod","+x",ytdlp_exe})
    end

    log("Updated to "..remote_v)

    mp.set_property("ytdl_path", ytdlp_exe)

    log("=== CHECK END ===")
end

-- ================= INIT =================
mp.add_timeout(1, update)
mp.register_script_message("check_update", update)