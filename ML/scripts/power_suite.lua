--[[---------------------------------------------------------------------------
5D3 Power Suite
Optimized live view scaling, power-through recording, and high-FPS presets
for Canon EOS 5D Mark III running Magic Lantern.

Enable autorun from: ML menu -> Scripts -> power_suite -> Autorun -> ON
---------------------------------------------------------------------------]]

require("config")
local ps = require("powersuite")

-- Runtime state
local state = {
    overlay_saved = {},
    overlay_active = { light = false, medium = false, heavy = false },
    lv_cycle = 0,
    lv_paused_by_us = false,
    power_task_running = false,
    last_notify = 0,
    preset_index = 1,
    last_fps_idx = 0,
    last_fps_optimize = 0,
    last_lv_pct = 0,
}

local function notify(msg, duration)
    duration = duration or 2500
    local now = dryos.ms_clock
    if now - state.last_notify > 800 then
        display.notify_box(msg, duration)
        state.last_notify = now
    end
end

local function power_through_active()
    if power_menu.value ~= "ON" then return false end
    if power_menu.submenu["Always Active"].value == "ON" then return true end
    if power_menu.submenu["While Recording"].value == "ON" and movie.recording then return true end
    return false
end

local function apply_liveview_resolution()
    local idx = liveview_menu.submenu["Resolution"].value
    local pct = ps.resolution_pct_from_index(idx)

    if liveview_menu.submenu["Auto Optimize"].value == "OFF" then
        return pct
    end

    if pct ~= state.last_lv_pct then
        ps.apply_overlay_load(pct, state.overlay_saved, state.overlay_active)
        if liveview_menu.submenu["Scale Crop YRES"].value == "ON" then
            ps.try_scale_crop_yres(pct)
        end
        state.last_lv_pct = pct
    end

    local ratio = ps.lv_pause_ratio(pct)
    if ratio > 0 and lv.enabled and not movie.recording and not menu.visible then
        state.lv_cycle = state.lv_cycle + 1
        if state.lv_cycle % (ratio + 1) == 0 then
            if lv.running and not lv.paused then
                lv.pause()
                state.lv_paused_by_us = true
            end
        elseif state.lv_paused_by_us and lv.paused then
            lv.resume()
            state.lv_paused_by_us = false
        end
    elseif state.lv_paused_by_us and lv.paused and not movie.recording then
        lv.resume()
        state.lv_paused_by_us = false
    end

    return pct
end

local function apply_fps_preset(force)
    if fps_menu.value ~= "ON" then
        if state.last_fps_idx ~= 0 then
            ps.disable_fps_override()
            state.last_fps_idx = 0
        end
        return
    end

    local idx = fps_menu.submenu["Target FPS"].value
    local opt = fps_menu.submenu["Optimize For"].value
    if not force and idx == state.last_fps_idx and opt == state.last_fps_optimize then
        return
    end

    local preset = ps.FPS_PRESETS[idx]
    if preset == nil then return end

    local optimize = fps_menu.submenu["Optimize For"].choices[opt]
    local ok, err = ps.apply_fps_override(preset.value, optimize)
    if not ok then
        notify(err or "FPS override failed", 3000)
        return
    end

    state.last_fps_idx = idx
    state.last_fps_optimize = opt

    if force and tonumber(preset.value) >= 96 then
        notify("Experimental " .. preset.value .. "fps - expect short bursts", 3500)
    end
end

local function apply_performance_preset()
    local preset = ps.PERF_PRESETS[state.preset_index]
    if preset == nil then return end

    local res_idx = ps.resolution_index_from_pct(preset.resolution_pct)
    liveview_menu.submenu["Resolution"].value = res_idx
    liveview_menu.submenu["Auto Optimize"].value = "ON"
    liveview_menu.submenu["Scale Crop YRES"].value = "ON"

    fps_menu.value = "ON"
    for i, p in ipairs(ps.FPS_PRESETS) do
        if p.value == preset.fps then
            fps_menu.submenu["Target FPS"].value = i
            break
        end
    end

    for i, choice in ipairs(fps_menu.submenu["Optimize For"].choices) do
        if choice == preset.optimize then
            fps_menu.submenu["Optimize For"].value = i
            break
        end
    end

    apply_liveview_resolution()
    apply_fps_preset(true)

    if preset.global_draw then
        ps.set_global_draw(preset.global_draw)
    end

    if preset.overlays == false then
        for _, feature in ipairs(ps.OVERLAY_FEATURES) do
            ps.set_overlay(feature, "OFF")
        end
    end

    notify("Applied: " .. preset.name, 3000)
end

local function disable_powersave_menus()
    if ps.menu_exists("Powersave in LiveView", "Enable power saving") then
        ps.safe_menu_set("Powersave in LiveView", "Enable power saving", "OFF")
    end
    if ps.menu_exists("Powersave in LiveView", "Dim display") then
        ps.safe_menu_set("Powersave in LiveView", "Dim display", "OFF")
    end
    if ps.menu_exists("Powersave in LiveView", "Turn off LCD and LV") then
        ps.safe_menu_set("Powersave in LiveView", "Turn off LCD and LV", "OFF")
    end
end

local function power_through_tick()
    if not power_through_active() then return end

    disable_powersave_menus()

    if power_menu.submenu["Keep LiveView Active"].value == "ON" then
        if lv.enabled and lv.paused and not state.lv_paused_by_us then
            lv.resume()
        end
        if lv.enabled == false and (movie.recording or camera.mode == MODE.MOVIE) then
            lv.start()
        end
    end
end

function power_suite_background()
    state.power_task_running = true
    while true do
        power_through_tick()
        if liveview_menu.value == "ON" then
            apply_liveview_resolution()
        end
        if fps_menu.value == "ON" then
            apply_fps_preset(false)
        end
        task.yield(400)
    end
end

-- Menu: Live View
liveview_menu = menu.new
{
    name = "Live View",
    help = "Downscale preview load to free CPU for recording",
    help2 = "Dial resolution % to reduce overlay and preview cost",
    choices = { "OFF", "ON" },
    value = "ON",
    submenu =
    {
        {
            name = "Resolution",
            help = "Preview processing scale (recording resolution unchanged)",
            choices = ps.RESOLUTION_CHOICES,
            value = 6,
        },
        {
            name = "Auto Optimize",
            help = "Auto-disable overlays and throttle preview by %",
            choices = { "OFF", "ON" },
            value = "ON",
        },
        {
            name = "Scale Crop YRES",
            help = "Also scale Crop mode Target YRES when available",
            choices = { "OFF", "ON" },
            value = "ON",
        },
    },
}

-- Menu: Power Through
power_menu = menu.new
{
    name = "Power Through",
    help = "Override auto power-off and thermal shutdown behavior",
    help2 = "Use with caution - remove battery if camera gets too hot",
    choices = { "OFF", "ON" },
    value = "OFF",
    submenu =
    {
        {
            name = "While Recording",
            help = "Keep camera alive during MLV/raw recording",
            choices = { "OFF", "ON" },
            value = "ON",
        },
        {
            name = "Always Active",
            help = "Prevent shutdown even when idle (advanced)",
            choices = { "OFF", "ON" },
            value = "OFF",
            warning = function(this)
                if this.value == "ON" then
                    return "Battery drain + heat risk. Pull battery if too hot."
                end
            end,
        },
        {
            name = "Keep LiveView Active",
            help = "Resume LiveView if Canon/ML tries to pause it",
            choices = { "OFF", "ON" },
            value = "ON",
        },
    },
}

-- Menu: High FPS
fps_menu = menu.new
{
    name = "High FPS",
    help = "Extended frame rates via ML FPS override",
    help2 = "60fps stable in crop modes; 120fps is experimental",
    choices = { "OFF", "ON" },
    value = "OFF",
    submenu =
    {
        {
            name = "Target FPS",
            help = "Desired recording frame rate",
            choices = ps.FPS_CHOICES,
            value = 7,
            warning = function(this)
                local preset = ps.FPS_PRESETS[this.value]
                if preset and tonumber(preset.value) >= 96 then
                    return preset.value .. "fps may be unstable on 5D3"
                end
            end,
        },
        {
            name = "Optimize For",
            help = "Exact FPS for cinema rates, High FPS for overcrank",
            choices = { "Exact FPS", "High FPS" },
            value = 2,
        },
    },
}

-- Menu: Performance Presets
perf_menu = menu.new
{
    name = "Perf Presets",
    help = "One-tap optimized profiles for raw video",
    submenu =
    {
        {
            name = "Preset",
            help = "Select a performance profile",
            choices = { "4K Raw Stable", "1080p Smooth", "Slo-Mo 60", "Ultra 120" },
            value = 1,
            select = function(this, delta)
                this.value = this.value + delta
                if this.value < 1 then this.value = 4 end
                if this.value > 4 then this.value = 1 end
                state.preset_index = this.value
            end,
        },
        {
            name = "Apply Preset",
            help = "Apply selected profile to Live View + FPS settings",
            select = function(this)
                state.preset_index = perf_menu.submenu["Preset"].value
                task.create(apply_performance_preset)
            end,
        },
    },
}

-- Parent menu entry under Movie (children attach via parent field below)
menu.new
{
    parent = "Movie",
    name = "5D3 Power Suite",
    help = "2026 optimized workflow for 5D Mark III raw video",
    help2 = "Live view scaling, power-through, and high FPS",
}

liveview_menu.parent = "5D3 Power Suite"
power_menu.parent = "5D3 Power Suite"
fps_menu.parent = "5D3 Power Suite"
perf_menu.parent = "5D3 Power Suite"

-- On-screen HUD
lv.info
{
    name = "Power Suite HUD",
    value = "",
    priority = 120,
    preferred_position = -50,
    update = function(this)
        if liveview_menu.value ~= "ON" and power_menu.value ~= "ON" and fps_menu.value ~= "ON" then
            this.value = ""
            return
        end

        local pct = ps.resolution_pct_from_index(liveview_menu.submenu["Resolution"].value)
        local fps_text = "OFF"
        if fps_menu.value == "ON" then
            local actual = ps.read_actual_fps()
            fps_text = actual or ps.FPS_PRESETS[fps_menu.submenu["Target FPS"].value].label
        end

        local pwr = power_through_active() and "PWR+" or "PWR-"
        local rec = movie.recording and "REC" or "STBY"
        local bw = ps.estimate_bandwidth_mbps(1920, 1080, ps.parse_fps_number(fps_text), 12, 1.7)

        this.background = movie.recording and COLOR.RED or COLOR.DARK_GRAY
        this.foreground = COLOR.WHITE
        this.value = string.format("%s %s | LV %s | %sfps | ~%.0fMB/s", rec, pwr, pct .. "%", fps_text, bw)
    end,
}

-- Quick toggle: SET in movie mode toggles power-through while recording
local prev_keypress = event.keypress
event.keypress = function(key)
    if key == KEY.INFO and camera.mode == MODE.MOVIE and menu.visible == false then
        if power_menu.value == "OFF" then
            power_menu.value = "ON"
            notify("Power Through ON", 1500)
        else
            power_menu.value = "OFF"
            notify("Power Through OFF", 1500)
        end
    end
    if prev_keypress then
        return prev_keypress(key)
    end
end

-- Persist settings
config.create_from_menu(liveview_menu)
config.create_from_menu(power_menu)
config.create_from_menu(fps_menu)

-- Start background optimizer (disable_powersave=true on task)
task.create(power_suite_background, nil, nil, true)

-- Startup hint
notify("5D3 Power Suite ready", 2000)
