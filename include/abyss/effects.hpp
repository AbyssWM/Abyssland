// The tables behind WindowEffects / LayerEffects / DeviceOpts: X(field, "rule name", type).
// The rule name is what the rule engine knows the effect as (same names as in `abyssctl` output and the old Lua API).
#pragma once

// bool effects take true/false, string effects the same text the rule used to take, e.g. size = "800 600"
// or "monitor_w*0.5 monitor_h*0.5", opacity = "0.9 0.8", border_color = "0xff80b0d8 0xffffffff 45deg".
#define ABYSS_WINDOW_EFFECTS(X)                                                                                                                                                    \
    X(floating, "float", bool)                                                                                                                                                     \
    X(tile, "tile", bool)                                                                                                                                                          \
    X(fullscreen, "fullscreen", bool)                                                                                                                                              \
    X(maximize, "maximize", bool)                                                                                                                                                  \
    X(center, "center", bool)                                                                                                                                                      \
    X(pseudo, "pseudo", bool)                                                                                                                                                      \
    X(pin, "pin", bool)                                                                                                                                                            \
    X(size, "size", std::string)                                                                                                                                                   \
    X(move, "move", std::string)                                                                                                                                                   \
    X(minSize, "min_size", std::string)                                                                                                                                            \
    X(maxSize, "max_size", std::string)                                                                                                                                            \
    X(monitor, "monitor", std::string)                                                                                                                                             \
    X(workspace, "workspace", std::string)                                                                                                                                         \
    X(group, "group", std::string)                                                                                                                                                 \
    X(opacity, "opacity", std::string)                                                                                                                                             \
    X(borderColor, "border_color", std::string)                                                                                                                                    \
    X(borderSize, "border_size", int64_t)                                                                                                                                          \
    X(rounding, "rounding", int64_t)                                                                                                                                               \
    X(roundingPower, "rounding_power", double)                                                                                                                                     \
    X(animation, "animation", std::string)                                                                                                                                         \
    X(tag, "tag", std::string)                                                                                                                                                     \
    X(content, "content", std::string)                                                                                                                                             \
    X(idleInhibit, "idle_inhibit", std::string)                                                                                                                                    \
    X(fullscreenState, "fullscreen_state", std::string)                                                                                                                            \
    X(suppressEvent, "suppress_event", std::string)                                                                                                                                \
    X(tonemap, "tonemap", std::string)                                                                                                                                             \
    X(scrollingWidth, "scrolling_width", double)                                                                                                                                   \
    X(scrollMouse, "scroll_mouse", double)                                                                                                                                         \
    X(scrollTouchpad, "scroll_touchpad", double)                                                                                                                                   \
    X(noCloseFor, "no_close_for", int64_t)                                                                                                                                         \
    X(noInitialFocus, "no_initial_focus", bool)                                                                                                                                    \
    X(noFocus, "no_focus", bool)                                                                                                                                                   \
    X(noFollowMouse, "no_follow_mouse", bool)                                                                                                                                      \
    X(stayFocused, "stay_focused", bool)                                                                                                                                           \
    X(focusOnActivate, "focus_on_activate", bool)                                                                                                                                  \
    X(allowsInput, "allows_input", bool)                                                                                                                                           \
    X(persistentSize, "persistent_size", bool)                                                                                                                                     \
    X(keepAspectRatio, "keep_aspect_ratio", bool)                                                                                                                                  \
    X(noMaxSize, "no_max_size", bool)                                                                                                                                              \
    X(decorate, "decorate", bool)                                                                                                                                                  \
    X(dimAround, "dim_around", bool)                                                                                                                                               \
    X(noDim, "no_dim", bool)                                                                                                                                                       \
    X(noAnim, "no_anim", bool)                                                                                                                                                     \
    X(noBlur, "no_blur", bool)                                                                                                                                                     \
    X(noShadow, "no_shadow", bool)                                                                                                                                                 \
    X(opaque, "opaque", bool)                                                                                                                                                      \
    X(xray, "xray", bool)                                                                                                                                                          \
    X(nearestNeighbor, "nearest_neighbor", bool)                                                                                                                                   \
    X(forceRgbx, "force_rgbx", bool)                                                                                                                                               \
    X(syncFullscreen, "sync_fullscreen", bool)                                                                                                                                     \
    X(immediate, "immediate", bool)                                                                                                                                                \
    X(renderUnfocused, "render_unfocused", bool)                                                                                                                                   \
    X(noScreenShare, "no_screen_share", bool)                                                                                                                                      \
    X(noShortcutsInhibit, "no_shortcuts_inhibit", bool)                                                                                                                            \
    X(noVrr, "no_vrr", bool)                                                                                                                                                       \
    X(noAutoHdr, "no_auto_hdr", bool)                                                                                                                                              \
    X(confinePointer, "confine_pointer", bool)

#define ABYSS_LAYER_EFFECTS(X)                                                                                                                                                     \
    X(blur, "blur", bool)                                                                                                                                                          \
    X(blurPopups, "blur_popups", bool)                                                                                                                                             \
    X(ignoreAlpha, "ignore_alpha", double)                                                                                                                                         \
    X(noAnim, "no_anim", bool)                                                                                                                                                     \
    X(animation, "animation", std::string)                                                                                                                                         \
    X(dimAround, "dim_around", bool)                                                                                                                                               \
    X(xray, "xray", bool)                                                                                                                                                          \
    X(order, "order", int64_t)                                                                                                                                                     \
    X(aboveLock, "above_lock", int64_t)                                                                                                                                            \
    X(noScreenShare, "no_screen_share", bool)

// per-device overrides: X(field, "config key", type); the key is the last part of the option name (input:touchpad:natural_scroll -> natural_scroll)
#define ABYSS_DEVICE_FIELDS(X)                                                                                                                                                     \
    X(sensitivity, "sensitivity", double)                                                                                                                                          \
    X(accelProfile, "accel_profile", std::string)                                                                                                                                  \
    X(rotation, "rotation", int64_t)                                                                                                                                               \
    X(kbFile, "kb_file", std::string)                                                                                                                                              \
    X(kbLayout, "kb_layout", std::string)                                                                                                                                          \
    X(kbVariant, "kb_variant", std::string)                                                                                                                                        \
    X(kbOptions, "kb_options", std::string)                                                                                                                                        \
    X(kbRules, "kb_rules", std::string)                                                                                                                                            \
    X(kbModel, "kb_model", std::string)                                                                                                                                            \
    X(repeatRate, "repeat_rate", int64_t)                                                                                                                                          \
    X(repeatDelay, "repeat_delay", int64_t)                                                                                                                                        \
    X(naturalScroll, "natural_scroll", bool)                                                                                                                                       \
    X(tapButtonMap, "tap_button_map", std::string)                                                                                                                                 \
    X(numlockByDefault, "numlock_by_default", bool)                                                                                                                                \
    X(resolveBindsBySym, "resolve_binds_by_sym", bool)                                                                                                                             \
    X(disableWhileTyping, "disable_while_typing", bool)                                                                                                                            \
    X(clickfingerBehavior, "clickfinger_behavior", bool)                                                                                                                           \
    X(middleButtonEmulation, "middle_button_emulation", bool)                                                                                                                      \
    X(tapToClick, "tap_to_click", bool)                                                                                                                                            \
    X(tapAndDrag, "tap_and_drag", bool)                                                                                                                                            \
    X(dragLock, "drag_lock", int64_t)                                                                                                                                              \
    X(leftHanded, "left_handed", bool)                                                                                                                                             \
    X(scrollMethod, "scroll_method", std::string)                                                                                                                                  \
    X(scrollButton, "scroll_button", int64_t)                                                                                                                                      \
    X(scrollButtonLock, "scroll_button_lock", bool)                                                                                                                                \
    X(scrollPoints, "scroll_points", std::string)                                                                                                                                  \
    X(scrollFactor, "scroll_factor", double)                                                                                                                                       \
    X(transform, "transform", int64_t)                                                                                                                                             \
    X(output, "output", std::string)                                                                                                                                               \
    X(enabled, "enabled", bool)                                                                                                                                                    \
    X(regionPosition, "region_position", Vec2)                                                                                                                                     \
    X(absoluteRegionPosition, "absolute_region_position", bool)                                                                                                                    \
    X(regionSize, "region_size", Vec2)                                                                                                                                             \
    X(relativeInput, "relative_input", bool)                                                                                                                                       \
    X(activeAreaPosition, "active_area_position", Vec2)                                                                                                                            \
    X(activeAreaSize, "active_area_size", Vec2)                                                                                                                                    \
    X(eraserButtonMode, "eraser_button_mode", int64_t)                                                                                                                             \
    X(eraserButtonOverride, "eraser_button_override", int64_t)                                                                                                                     \
    X(pressureRangeMin, "pressure_range_min", double)                                                                                                                              \
    X(pressureRangeMax, "pressure_range_max", double)                                                                                                                              \
    X(flipX, "flip_x", bool)                                                                                                                                                       \
    X(flipY, "flip_y", bool)                                                                                                                                                       \
    X(drag3fg, "drag_3fg", int64_t)                                                                                                                                                \
    X(shareStates, "share_states", int64_t)                                                                                                                                        \
    X(releasePressedOnClose, "release_pressed_on_close", bool)                                                                                                                     \
    X(keybinds, "keybinds", bool)                                                                                                                                                  \
    X(tags, "tags", std::string)
