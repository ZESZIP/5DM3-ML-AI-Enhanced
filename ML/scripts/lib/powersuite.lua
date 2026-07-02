--[[---------------------------------------------------------------------------
Power Suite library for Canon 5D Mark III / Magic Lantern
Shared helpers for live view scaling, FPS presets, and bandwidth math.
---------------------------------------------------------------------------]]

local M = {}

M.RESOLUTION_CHOICES = { "25%", "33%", "50%", "67%", "75%", "100%" }
M.RESOLUTION_VALUES  = { 25, 33, 50, 67, 75, 100 }

M.FPS_PRESETS = {
    { label = "23.976", value = "23.976", note = "Cinema" },
    { label = "24",     value = "24",     note = "Film" },
    { label = "25",     value = "25",     note = "PAL" },
    { label = "30",     value = "30",     note = "NTSC" },
    { label = "48",     value = "48",     note = "2x 24p" },
    { label = "50",     value = "50",     note = "PAL high" },
    { label = "60",     value = "60",     note = "720p max stable" },
    { label = "72",     value = "72",     note = "Overcrank" },
    { label = "96",     value = "96",     note = "Experimental" },
    { label = "120",    value = "120",    note = "Max attempt" },
}

M.FPS_CHOICES = {}
for i, p in ipairs(M.FPS_PRESETS) do
    M.FPS_CHOICES[i] = p.label
end

M.OVERLAY_FEATURES = {
    "Zebras",
    "Focus Peak",
    "Magic Zoom",
    "Histogram",
    "Waveform",
    "False Color",
}

M.PERF_PRESETS = {
    {
        name = "4K Raw Stable",
        help = "12-bit, low ISO, overlays off, 24fps target",
        resolution_pct = 50,
        fps = "24",
        optimize = "Exact FPS",
        overlays = false,
        global_draw = "OFF",
        bit_hint = 12,
    },
    {
        name = "1080p Smooth",
        help = "Balanced preview + recording, 25fps",
        resolution_pct = 75,
        fps = "25",
        optimize = "High FPS",
        overlays = true,
        global_draw = "ON",
        bit_hint = 14,
    },
    {
        name = "Slo-Mo 60",
        help = "60fps crop mode preset, minimal preview load",
        resolution_pct = 50,
        fps = "60",
        optimize = "High FPS",
        overlays = false,
        global_draw = "OFF",
        bit_hint = 12,
    },
    {
        name = "Ultra 120",
        help = "Push FPS override to 120 (experimental, short bursts)",
        resolution_pct = 25,
        fps = "120",
        optimize = "High FPS",
        overlays = false,
        global_draw = "OFF",
        bit_hint = 10,
    },
}

function M.resolution_pct_from_index(idx)
    return M.RESOLUTION_VALUES[idx] or 100
end

function M.resolution_index_from_pct(pct)
    local best_idx, best_diff = 6, 999
    for i, v in ipairs(M.RESOLUTION_VALUES) do
        local d = math.abs(v - pct)
        if d < best_diff then
            best_diff = d
            best_idx = i
        end
    end
    return best_idx
end

function M.safe_menu_get(...)
    local ok, value = pcall(menu.get, ...)
    if ok then return value end
    return nil
end

function M.safe_menu_set(...)
    local ok, result = pcall(menu.set, ...)
    return ok and result
end

function M.menu_exists(...)
    return M.safe_menu_get(...) ~= nil
end

function M.set_overlay(feature, state)
    if M.menu_exists("Overlay", feature) then
        return M.safe_menu_set("Overlay", feature, state)
    end
    return false
end

function M.set_global_draw(state)
    if M.menu_exists("Overlay", "Global Draw") then
        return M.safe_menu_set("Overlay", "Global Draw", state)
    end
    return false
end

function M.capture_overlay_state(store)
    for _, feature in ipairs(M.OVERLAY_FEATURES) do
        local v = M.safe_menu_get("Overlay", feature)
        if v ~= nil then
            store[feature] = v
        end
    end
    local gdr = M.safe_menu_get("Overlay", "Global Draw")
    if gdr ~= nil then
        store["Global Draw"] = gdr
    end
end

function M.restore_overlay_state(store)
    for k, v in pairs(store) do
        if k == "Global Draw" then
            M.set_global_draw(v)
        else
            M.set_overlay(k, v)
        end
    end
end

function M.apply_overlay_load(pct, saved, active)
    if pct >= 100 then
        if next(saved) ~= nil then
            M.restore_overlay_state(saved)
        end
        active.light = false
        active.medium = false
        active.heavy = false
        return
    end

    if next(saved) == nil then
        M.capture_overlay_state(saved)
    end

    active.light = pct <= 75
    active.medium = pct <= 50
    active.heavy = pct <= 33

    if active.heavy then
        M.set_global_draw("OFF")
        for _, feature in ipairs(M.OVERLAY_FEATURES) do
            M.set_overlay(feature, "OFF")
        end
    elseif active.medium then
        M.set_global_draw("OFF")
        for _, feature in ipairs(M.OVERLAY_FEATURES) do
            M.set_overlay(feature, "OFF")
        end
    elseif active.light then
        M.set_overlay("Zebras", "OFF")
        M.set_overlay("Focus Peak", "OFF")
        M.set_overlay("False Color", "OFF")
    end
end

function M.try_scale_crop_yres(pct)
    if not M.menu_exists("Crop mode", "Target YRES") then
        return false
    end
    local current = M.safe_menu_get("Crop mode", "Target YRES", 0)
    if current == nil or current == 0 then
        return false
    end
    local scaled = math.max(400, math.floor(current * pct / 100))
    return M.safe_menu_set("Crop mode", "Target YRES", scaled)
end

function M.apply_fps_override(fps_value, optimize_for)
    if not M.menu_exists("Movie", "FPS override") then
        return false, "FPS override not available"
    end

    M.safe_menu_set("FPS override", "Desired FPS", fps_value)
    if optimize_for and M.menu_exists("FPS override", "Optimize for") then
        M.safe_menu_set("FPS override", "Optimize for", optimize_for)
    end
    M.safe_menu_set("Movie", "FPS override", "ON")
    return true
end

function M.disable_fps_override()
    if M.menu_exists("Movie", "FPS override") then
        M.safe_menu_set("Movie", "FPS override", "OFF")
    end
end

function M.read_actual_fps()
    local actual = M.safe_menu_get("FPS override", "Actual FPS")
    if actual ~= nil then return actual end
    return M.safe_menu_get("Movie", "FPS override")
end

function M.estimate_bandwidth_mbps(width, height, fps, bit_depth, compression)
    width = width or 1920
    height = height or 1080
    fps = fps or 24
    bit_depth = bit_depth or 14
    compression = compression or 1.6
    return (width * height * fps * bit_depth) / (8 * compression * 1024 * 1024)
end

function M.parse_fps_number(text)
    if text == nil then return 24 end
    local n = tonumber(tostring(text):match("([%d%.]+)"))
    return n or 24
end

function M.lv_pause_ratio(pct)
    if pct <= 25 then return 3 end
    if pct <= 33 then return 2 end
    if pct <= 50 then return 0 end
    return 0
end

function M.format_pct_bar(pct)
    local filled = math.floor(pct / 10)
    local bar = string.rep("|", filled) .. string.rep(".", 10 - filled)
    return string.format("%s %d%%", bar, pct)
end

return M
