#include <medusa/medusa_window.hpp>
#include <medusa/medusa_theme.hpp>
#include <medusa/medusa_production_scene.hpp>
#include <medusa/medusa_project.hpp>
#include <windowsx.h>
#include <commdlg.h>
#include <dwmapi.h>
#include <shlobj.h>
#include <algorithm>
#include <iostream>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>
#include <exception>
#include <new>

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

namespace medusa {

static const wchar_t* kWindowClassName = L"MedusaPlatinum_WindowClass";
static const wchar_t* kWindowTitle = L"Medusa Platinum - 3D Cinematic Animation & Production Renderer";

static uint32_t free_camera_key_mask(WPARAM key) {
    switch (key) {
        case 'W': return 1u << 0;
        case 'S': return 1u << 1;
        case 'A': return 1u << 2;
        case 'D': return 1u << 3;
        case 'Q': return 1u << 4;
        case 'E': return 1u << 5;
        default: return 0;
    }
}

static bool sequence_outputs_exist(const std::wstring& prefix, int startFrame, int endFrame, bool exr) {
    const std::filesystem::path outputPrefix(prefix);
    std::filesystem::path directory = outputPrefix.parent_path();
    std::error_code ec;
    if (directory.empty()) directory = std::filesystem::current_path(ec);
    if (ec) return false;
    const std::wstring filenamePrefix = outputPrefix.filename().wstring() + L"_";
    const std::wstring extension = exr ? L".exr" : L".png";
    for (std::filesystem::directory_iterator it(directory, ec), end; !ec && it != end; it.increment(ec)) {
        const std::wstring name = it->path().filename().wstring();
        if (name.size() <= filenamePrefix.size() + extension.size() ||
            name.compare(0, filenamePrefix.size(), filenamePrefix) != 0 ||
            name.compare(name.size() - extension.size(), extension.size(), extension) != 0) continue;
        const std::wstring digits = name.substr(filenamePrefix.size(), name.size() - filenamePrefix.size() - extension.size());
        if (!std::all_of(digits.begin(), digits.end(), [](wchar_t value) { return value >= L'0' && value <= L'9'; })) continue;
        try {
            const long long frame = std::stoll(digits);
            if (frame >= startFrame && frame <= endFrame) return true;
        } catch (...) {
        }
    }
    return false;
}

static std::string wide_to_utf8(const std::wstring& value) {
    if (value.empty()) return {};
    const int bytes = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (bytes <= 1) return {};
    std::string result(static_cast<size_t>(bytes), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, result.data(), bytes, nullptr, nullptr);
    result.resize(static_cast<size_t>(bytes - 1));
    return result;
}

static std::wstring utf8_to_wide(const std::string& value) {
    if (value.empty()) return {};
    const int chars = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
    if (chars <= 1) return {};
    std::wstring result(static_cast<size_t>(chars), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, result.data(), chars);
    result.resize(static_cast<size_t>(chars - 1));
    return result;
}

Window::Window() = default;

Window::~Window() {
    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
}

bool Window::create(const std::wstring &title, int width, int height) {
    HINSTANCE hInstance = GetModuleHandleW(nullptr);

    // Register Window Class
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc = Window::window_proc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr; // Direct2D handles rendering, no GDI background erase
    wc.lpszClassName = kWindowClassName;

    RegisterClassExW(&wc);

    // Calculate window rectangle for desired client area
    RECT rect = { 0, 0, width, height };
    DWORD style = WS_OVERLAPPEDWINDOW;
    AdjustWindowRectEx(&rect, style, FALSE, 0);

    int window_width = rect.right - rect.left;
    int window_height = rect.bottom - rect.top;

    // Center within visible work area of primary monitor
    int pos_x = CW_USEDEFAULT;
    int pos_y = CW_USEDEFAULT;
    RECT work_area;
    if (SystemParametersInfoW(SPI_GETWORKAREA, 0, &work_area, 0)) {
        int max_w = work_area.right - work_area.left;
        int max_h = work_area.bottom - work_area.top;
        if (window_width > max_w) window_width = max_w - 40;
        if (window_height > max_h) window_height = max_h - 40;
        pos_x = work_area.left + (max_w - window_width) / 2;
        pos_y = work_area.top + (max_h - window_height) / 2;
    }

    hwnd_ = CreateWindowExW(
        0,
        kWindowClassName,
        title.c_str(),
        style,
        pos_x,
        pos_y,
        window_width,
        window_height,
        nullptr,
        nullptr,
        hInstance,
        this
    );

    if (!hwnd_) {
        std::cerr << "[Medusa Window] Failed to create HWND. Error: " << GetLastError() << std::endl;
        return false;
    }

    RAWINPUTDEVICE mouseInput = {};
    mouseInput.usUsagePage = 0x01; // Generic Desktop Controls
    mouseInput.usUsage = 0x02;     // Mouse
    mouseInput.dwFlags = 0;
    mouseInput.hwndTarget = hwnd_;
    raw_mouse_registered_ = RegisterRawInputDevices(&mouseInput, 1, sizeof(mouseInput)) != FALSE;
    if (!raw_mouse_registered_)
        std::cerr << "[Medusa Window] Raw mouse input unavailable; free-look will use regular mouse motion." << std::endl;

    // Enable Immersive Dark Mode for Win32 Title Bar
    BOOL dark_mode = TRUE;
    DwmSetWindowAttribute(hwnd_, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark_mode, sizeof(dark_mode));
    // Fallback for older Windows 10 versions (1903/1909)
    DwmSetWindowAttribute(hwnd_, 19, &dark_mode, sizeof(dark_mode));

    // Initialize Graphics and Editor
    RECT client_rect;
    GetClientRect(hwnd_, &client_rect);
    client_width_ = client_rect.right - client_rect.left;
    client_height_ = client_rect.bottom - client_rect.top;

    if (!gfx_.init(hwnd_, client_width_, client_height_)) {
        std::cerr << "[Medusa Window] Graphics::init failed: " << gfx_.last_init_error() << std::endl;
        return false;
    }

    editor_ = std::make_unique<MedusaEditor>();
    ensure_medusa_document_identity(editor_->get_state());
    saved_project_snapshot_ = medusa_project_snapshot(editor_->get_state());
    set_project_title();

    float dip_w = static_cast<float>(client_width_) / gfx_.get_dpi_scale();
    float dip_h = static_cast<float>(client_height_) / gfx_.get_dpi_scale();
    editor_->rebuild_layout(dip_w, dip_h);
    const Rect render_bounds = editor_->get_render_viewport_bounds();
    gfx_.resize_realtime_viewport(
        static_cast<uint32_t>(std::max(1.0f, render_bounds.width * gfx_.get_dpi_scale())),
        static_cast<uint32_t>(std::max(1.0f, render_bounds.height * gfx_.get_dpi_scale())));

    std::string startupSceneError;
    if (load_production_asset(kBuiltinCubeScenePath, startupSceneError)) {
        auto& state = editor_->get_state();
        state.select_scene_node(0);
        state.frame_selected();
        state.camera_pilot_active = false;
        state.enter_viewport_camera();
        saved_project_snapshot_ = medusa_project_snapshot(state);
    } else {
        std::cerr << "[Medusa Window] Could not create the starter scene: "
                  << startupSceneError << std::endl;
    }

    recovery_timer_active_ = SetTimer(hwnd_, 5, 10000, nullptr) != 0;
    maybe_restore_recovery();

    return true;
}

void Window::show(int cmd_show) {
    if (hwnd_) {
        ShowWindow(hwnd_, cmd_show);
        UpdateWindow(hwnd_);
        SetForegroundWindow(hwnd_);
        SetFocus(hwnd_);
    }
}

void Window::request_redraw() {
    if (hwnd_) {
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
}

void Window::render_frame() {
    const bool offlineRender = sequence_render_active_ || export_current_on_complete_;
    gfx_.set_production_render_active(offlineRender);
    sync_production_controls();
    if (editor_) {
        const auto& state = editor_->get_state();
        gfx_.set_viewport_shading_mode(state.viewport_shading);
        gfx_.set_realtime_grid_enabled(state.show_grid);
        gfx_.set_realtime_modal_overlay(state.render_setup_open);
        gfx_.set_realtime_grid_enabled(state.show_grid);
        if (offlineRender) {
            KillTimer(hwnd_, 8);
        } else {
            if (production_timer_active_) {
                KillTimer(hwnd_, 1);
                production_timer_active_ = false;
            }
            if (gfx_.realtime_view_available() && gfx_.realtime_convergence_pending())
                SetTimer(hwnd_, 8, 16, nullptr);
            else
                KillTimer(hwnd_, 8);
            if (gfx_.realtime_render_dirty() || state.is_playing)
                gfx_.render_realtime_view();
        }
    }
    gfx_.begin_draw();
    gfx_.clear(Theme::kChassisBg);

    if (editor_) {
        editor_->draw(gfx_);
    }

    gfx_.end_draw();

    if (!screenshot_path_.empty()) {
        gfx_.save_screenshot(screenshot_path_);
        screenshot_path_.clear();
    }

    gfx_.present();
}

void Window::trace_production_frame() {
    if (!sequence_render_active_ && !export_current_on_complete_) return;
    gfx_.trace_production_frame();
}

void Window::start_production_timer() {
    if (!hwnd_ || production_timer_active_ || gfx_.production_render_complete() || is_sizing_ ||
        (!sequence_render_active_ && !export_current_on_complete_)) return;
    production_timer_active_ = SetTimer(hwnd_, 1, 33, nullptr) != 0;
    gfx_.set_production_render_active(production_timer_active_);
}

void Window::dispatch_editor_command() {
    if (!editor_) return;
    const std::string command = editor_->take_pending_command();
    if (command.empty()) {
        if (editor_->get_state().is_playing) start_animation_timer();
        else if (animation_timer_active_) {
            KillTimer(hwnd_, 3);
            animation_timer_active_ = false;
            animation_timer_interval_ = 0;
        }
        if (!defer_project_dirty_) update_project_dirty();
        return;
    }
    if (command == "viewport_pick") {
        auto& state = editor_->get_state();
        uint32_t nodeIndex = 0;
        float distance = 0.0f;
        if (gfx_.pick_production_node(state.pending_pick_origin, state.pending_pick_direction,
                                      nodeIndex, distance)) {
            state.select_scene_node(nodeIndex);
        } else {
            state.selected_scene_node = -1;
        }
        request_redraw();
    } else if (command == "new_project") {
        if (confirm_project_change()) {
            std::wstring projectPath;
            if (choose_project_path(true, projectPath, L"Create Medusa Project")) {
                EditorState fresh;
                const auto& current = editor_->get_state();
                fresh.ui_toolshelf_width = current.ui_toolshelf_width;
                fresh.ui_right_deck_width = current.ui_right_deck_width;
                fresh.ui_outliner_ratio = current.ui_outliner_ratio;
                fresh.ui_timeline_height = current.ui_timeline_height;
                fresh.asset_library_root = current.asset_library_root;
                fresh.render_output_path = current.render_output_path;
                fresh.render_output_width = current.render_output_width;
                fresh.render_output_height = current.render_output_height;
                fresh.render_output_exr = current.render_output_exr;
                fresh.render_frame_mode = current.render_frame_mode;
                fresh.render_custom_start = current.render_custom_start;
                fresh.render_custom_end = current.render_custom_end;
                const std::filesystem::path projectFile(projectPath);
                fresh.project_path = wide_to_utf8(projectPath);
                fresh.scene_name = wide_to_utf8(projectFile.stem().wstring());
                ensure_medusa_document_identity(fresh);
                std::string error;
                if (save_medusa_project(fresh, fresh.project_path, error)) {
                    gfx_.clear_production_scene();
                    render_view_valid_ = false;
                    render_settings_valid_ = false;
                    clear_history();
                    editor_->get_state() = std::move(fresh);
                    discard_changes_confirmed_ = false;
                    saved_project_snapshot_ = medusa_project_snapshot(editor_->get_state());
                    set_project_title();
                    request_redraw();
                } else {
                    MessageBoxA(hwnd_, error.c_str(), "Medusa Platinum - Project Error", MB_ICONERROR | MB_OK);
                }
            }
        }
    } else if (command == "open_project") {
        std::wstring projectPath;
        if (choose_project_path(false, projectPath, L"Open Medusa Project")) {
            const std::string utf8Path = wide_to_utf8(projectPath);
            EditorState candidate;
            std::string error;
            if (load_medusa_project(utf8Path, candidate, error)) {
                if (!candidate.asset_path.empty() && !is_builtin_production_scene(candidate.asset_path) &&
                    !std::filesystem::exists(std::filesystem::u8path(candidate.asset_path))) {
                    error = "The project's linked asset could not be found:\n" + candidate.asset_path;
                } else if (confirm_project_change() && !open_project(utf8Path, error)) {
                    // open_project supplies a specific error when restoration fails.
                }
            }
            if (!error.empty())
                MessageBoxA(hwnd_, error.c_str(), "Medusa Platinum - Project Error", MB_ICONERROR | MB_OK);
        }
    } else if (command == "undo") {
        apply_history_step(false);
    } else if (command == "redo") {
        apply_history_step(true);
    } else if (command == "save_project") {
        save_project(false);
    } else if (command == "save_project_as") {
        save_project(true);
    } else if (command == "reset_layout") {
        editor_->reset_layout();
        request_redraw();
    } else if (command.rfind("add_primitive_", 0) == 0 || command.rfind("add_light_", 0) == 0 ||
               command == "add_camera" || command == "add_volume") {
        std::string objectType;
        if (command.rfind("add_primitive_", 0) == 0) objectType = "primitive:" + command.substr(14);
        else if (command.rfind("add_light_", 0) == 0) objectType = "light:" + command.substr(10);
        else objectType = command == "add_camera" ? "camera" : "volume";
        std::string error;
        if (!add_scene_object(objectType, error) && !error.empty())
            MessageBoxA(hwnd_, error.c_str(), "Medusa Platinum - Add Object", MB_ICONERROR | MB_OK);
    } else if (command == "frame_selected") {
        editor_->get_state().frame_selected();
        sync_production_controls();
        request_redraw();
    } else if (command == "evaluate_scene_animation") {
        auto& state = editor_->get_state();
        std::string error;
        size_t keyCursor = 0;
        for (size_t i = 0; i < state.scene_nodes.size(); ++i) {
            while (keyCursor < state.node_keyframes.size() && state.node_keyframes[keyCursor].node_index < static_cast<int>(i)) ++keyCursor;
            const auto type = state.scene_nodes[i].type;
            if (type != zegfx::ProductionNodeType::Mesh && type != zegfx::ProductionNodeType::Group) continue;
            if (keyCursor >= state.node_keyframes.size() || state.node_keyframes[keyCursor].node_index != static_cast<int>(i)) continue;
            const auto& node = state.scene_nodes[i];
            const float position[3] = {node.worldTransform[12], node.worldTransform[13], node.worldTransform[14]};
            if (!gfx_.set_production_node_translation(static_cast<uint32_t>(i), position, error)) break;
            std::vector<zegfx::ProductionSceneNode> updatedNodes;
            if (!gfx_.set_production_node_rotation_scale(static_cast<uint32_t>(i), node.rotationOffset,
                                                          node.scaleFactor, updatedNodes, error)) break;
            state.scene_nodes = std::move(updatedNodes);
        }
        if (!error.empty()) MessageBoxA(hwnd_, error.c_str(), "Medusa Platinum - Animation", MB_ICONERROR | MB_OK);
        else start_production_timer();
        sync_production_controls();
        request_redraw();
    } else if (command == "scene_node_state") {
        auto& state = editor_->get_state();
        if (state.pending_scene_node >= 0 &&
            static_cast<size_t>(state.pending_scene_node) < state.scene_nodes.size()) {
            const size_t index = static_cast<size_t>(state.pending_scene_node);
            std::string error;
            const bool applied = gfx_.set_production_node_state(
                static_cast<uint32_t>(index), state.scene_nodes[index].visible,
                state.scene_nodes[index].renderable, error);
            if (!applied) {
                state.scene_nodes[index].visible = state.pending_previous_visible;
                state.scene_nodes[index].renderable = state.pending_previous_renderable;
                if (!error.empty()) MessageBoxA(hwnd_, error.c_str(), "Medusa Platinum", MB_ICONINFORMATION | MB_OK);
            } else {
                start_production_timer();
            }
            state.pending_scene_node = -1;
            request_redraw();
        }
    } else if (command == "scene_node_light_energy") {
        auto& state = editor_->get_state();
        if (state.pending_scene_node >= 0 &&
            static_cast<size_t>(state.pending_scene_node) < state.scene_nodes.size()) {
            const size_t index = static_cast<size_t>(state.pending_scene_node);
            std::string error;
            if (gfx_.has_production_scene() &&
                !gfx_.set_production_node_light_energy(static_cast<uint32_t>(index),
                    state.scene_nodes[index].lightEnergy, error)) {
                state.revert_scene_node_light_energy(static_cast<uint32_t>(index));
                if (!error.empty()) MessageBoxA(hwnd_, error.c_str(), "Medusa Platinum", MB_ICONINFORMATION | MB_OK);
            } else if (gfx_.has_production_scene()) {
                start_production_timer();
            }
            state.pending_scene_node = -1;
            request_redraw();
        }
    } else if (command == "scene_node_translation") {
        auto& state = editor_->get_state();
        if (state.pending_scene_node >= 0 &&
            static_cast<size_t>(state.pending_scene_node) < state.scene_nodes.size()) {
            const size_t index = static_cast<size_t>(state.pending_scene_node);
            const float position[3] = {state.scene_nodes[index].worldTransform[12],
                                        state.scene_nodes[index].worldTransform[13],
                                        state.scene_nodes[index].worldTransform[14]};
            std::string error;
            if (gfx_.has_production_scene() &&
                !gfx_.set_production_node_translation(static_cast<uint32_t>(index), position, error)) {
                state.revert_scene_node_translation(static_cast<uint32_t>(index));
                if (!error.empty()) MessageBoxA(hwnd_, error.c_str(), "Medusa Platinum", MB_ICONINFORMATION | MB_OK);
            } else if (gfx_.has_production_scene()) {
                start_production_timer();
            }
            state.pending_scene_node = -1;
            request_redraw();
        }
    } else if (command == "scene_node_rotation_scale") {
        auto& state = editor_->get_state();
        if (state.pending_scene_node >= 0 &&
            static_cast<size_t>(state.pending_scene_node) < state.scene_nodes.size()) {
            const size_t index = static_cast<size_t>(state.pending_scene_node);
            const auto& node = state.scene_nodes[index];
            std::vector<zegfx::ProductionSceneNode> updatedNodes;
            std::string error;
            if (gfx_.has_production_scene() && !gfx_.set_production_node_rotation_scale(static_cast<uint32_t>(index),
                    node.rotationOffset, node.scaleFactor, updatedNodes, error)) {
                state.revert_scene_node_rotation_scale(static_cast<uint32_t>(index));
                if (!error.empty()) MessageBoxA(hwnd_, error.c_str(), "Medusa Platinum", MB_ICONINFORMATION | MB_OK);
            } else if (gfx_.has_production_scene()) {
                state.scene_nodes = std::move(updatedNodes);
                start_production_timer();
            }
            state.pending_scene_node = -1;
            request_redraw();
        }
    } else if (command == "exit") {
        PostMessageW(hwnd_, WM_CLOSE, 0, 0);
    } else if (command == "choose_asset_library") {
        BROWSEINFOW dialog = {};
        dialog.hwndOwner = hwnd_;
        dialog.lpszTitle = L"Choose Asset Library Folder";
        dialog.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
        LPITEMIDLIST selected = SHBrowseForFolderW(&dialog);
        if (selected) {
            wchar_t path[MAX_PATH] = {};
            if (SHGetPathFromIDListW(selected, path)) {
                editor_->set_asset_library_root(wide_to_utf8(path));
                request_redraw();
            }
            CoTaskMemFree(selected);
        }
    } else if (command == "load_library_asset") {
        const std::string path = editor_->get_state().asset_library_selected_path;
        if (!path.empty() && confirm_project_change()) {
            std::string error;
            if (!load_production_asset(path, error))
                MessageBoxA(hwnd_, error.c_str(), "Medusa Platinum - Asset Load Error", MB_ICONERROR | MB_OK);
        }
    } else if (command == "render") {
        sequence_render_active_ = false;
        export_current_on_complete_ = false;
        editor_->get_state().viewport_shading = ViewportShadingMode::Rendered;
        request_redraw();
    } else if (command == "render_current") {
        if (!gfx_.has_production_scene()) {
            MessageBoxW(hwnd_, L"Open a GLB or GLTF asset before rendering.",
                        L"Medusa Platinum", MB_ICONINFORMATION | MB_OK);
        } else {
            sequence_render_active_ = false;
            std::wstring path;
            if (choose_render_output(path, L"Render Current Frame")) {
                editor_->get_state().render_output_path = wide_to_utf8(path);
                editor_->get_state().render_output_exr = std::filesystem::path(path).extension() == L".exr";
                current_export_path_ = path;
                current_export_exr_ = std::filesystem::path(path).extension() == L".exr";
                const auto& renderState = editor_->get_state();
                if (!gfx_.resize_production_output(
                        static_cast<uint32_t>(renderState.render_output_width),
                        static_cast<uint32_t>(renderState.render_output_height))) {
                    MessageBoxW(hwnd_, L"The renderer could not allocate the selected output resolution.",
                                L"Medusa Platinum", MB_ICONERROR | MB_OK);
                    return;
                }
                export_current_on_complete_ = true;
                render_view_valid_ = false;
                sync_production_controls();
                gfx_.restart_production_render();
                start_production_timer();
                request_redraw();
            }
        }
    } else if (command == "render_output_browse") {
        auto& state = editor_->get_state();
        std::wstring path;
        if (choose_render_output(path, L"Choose Render Output", false)) {
            state.render_output_path = wide_to_utf8(path);
            state.render_output_exr = std::filesystem::path(path).extension() == L".exr";
        }
        state.render_setup_open = true;
        request_redraw();
    } else if (command == "render_sequence") {
        if (!gfx_.has_production_scene()) {
            MessageBoxW(hwnd_, L"Open a GLB or GLTF asset before rendering a sequence.",
                        L"Medusa Platinum", MB_ICONINFORMATION | MB_OK);
            editor_->get_state().render_setup_open = true;
            request_redraw();
        } else {
            auto& state = editor_->get_state();
            std::wstring path;
            if (state.render_output_path.empty()) {
                if (!choose_render_output(path, L"Render Sequence: choose the first frame name", false)) {
                    state.render_setup_open = true;
                    request_redraw();
                    return;
                }
                state.render_output_exr = std::filesystem::path(path).extension() == L".exr";
            } else {
                path = std::filesystem::u8path(state.render_output_path).wstring();
            }
            std::filesystem::path base(path);
            base.replace_extension(state.render_output_exr ? L".exr" : L".png");
            path = base.wstring();
            state.render_output_path = wide_to_utf8(path);
            export_current_on_complete_ = false;
            sequence_export_exr_ = state.render_output_exr;
            base.replace_extension();
            sequence_prefix_ = base.wstring();
            if (state.render_frame_mode == EditorState::RenderFrameMode::CurrentFrame) {
                sequence_frame_ = std::max(1, state.current_frame);
                sequence_end_frame_ = sequence_frame_;
            } else if (state.render_frame_mode == EditorState::RenderFrameMode::CustomRange) {
                sequence_frame_ = std::max(1, state.render_custom_start);
                sequence_end_frame_ = std::max(sequence_frame_, state.render_custom_end);
            } else {
                sequence_frame_ = std::max(1, state.start_frame);
                sequence_end_frame_ = std::max(sequence_frame_, state.end_frame);
            }
            std::error_code outputError;
            const std::filesystem::path outputDirectory = base.has_parent_path()
                ? base.parent_path() : std::filesystem::current_path(outputError);
            if (outputError || !std::filesystem::exists(outputDirectory, outputError) ||
                !std::filesystem::is_directory(outputDirectory, outputError) || outputError) {
                MessageBoxW(hwnd_, L"The selected output folder is no longer available. Choose another folder.",
                            L"Medusa Platinum - Output Folder", MB_ICONERROR | MB_OK);
                state.render_output_path.clear();
                state.render_setup_open = true;
                request_redraw();
                return;
            }
            if (sequence_outputs_exist(sequence_prefix_, sequence_frame_, sequence_end_frame_, sequence_export_exr_) &&
                MessageBoxW(hwnd_, L"Matching frames already exist in this folder. Overwrite those frames?",
                            L"Medusa Platinum - Existing Render Frames", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES) {
                state.render_setup_open = true;
                request_redraw();
                return;
            }
            if (!gfx_.resize_production_output(static_cast<uint32_t>(state.render_output_width),
                                               static_cast<uint32_t>(state.render_output_height))) {
                MessageBoxW(hwnd_, L"The renderer could not allocate the selected output resolution.",
                            L"Medusa Platinum", MB_ICONERROR | MB_OK);
                state.render_setup_open = true;
                request_redraw();
                return;
            }
            sequence_output_size_active_ = true;
            state.current_frame = sequence_frame_;
            state.evaluate_keyframes();
            sequence_render_active_ = true;
            render_view_valid_ = false;
            dispatch_editor_command();
            sync_production_controls();
            gfx_.restart_production_render();
            start_production_timer();
            request_redraw();
        }
    } else if (command == "open_asset") {
        std::vector<wchar_t> path(32768, L'\0');
        OPENFILENAMEW dialog = {};
        dialog.lStructSize = sizeof(dialog);
        dialog.hwndOwner = hwnd_;
        dialog.lpstrFilter = L"3D assets (*.glb;*.gltf;*.obj;*.fbx)\0*.glb;*.gltf;*.obj;*.fbx\0All files (*.*)\0*.*\0";
        dialog.lpstrFile = path.data();
        dialog.nMaxFile = static_cast<DWORD>(path.size());
        dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
        dialog.lpstrTitle = L"Open Production Asset";
        if (GetOpenFileNameW(&dialog)) {
            if (confirm_project_change()) {
                std::string error;
                if (!load_production_asset(wide_to_utf8(path.data()), error))
                    MessageBoxA(hwnd_, error.c_str(), "Medusa Platinum - Asset Load Error", MB_ICONERROR | MB_OK);
            }
        }
    }

    if (editor_) {
        if (editor_->get_state().is_playing) start_animation_timer();
        else if (animation_timer_active_) {
            KillTimer(hwnd_, 3);
            animation_timer_active_ = false;
            animation_timer_interval_ = 0;
        }
    }
    if (!defer_project_dirty_) update_project_dirty();
}

bool Window::load_production_asset(const std::string& path, std::string& error,
                                   const std::string& stableAssetId) {
    try {
        zegfx::ProductionScene scene;
        if (!load_production_scene(path, scene, error)) return false;
        const std::string assetId = stableAssetId.empty() ? create_medusa_stable_id() : stableAssetId;
        const size_t sceneNodeCount = scene.nodes.size();
        const size_t triangleCount = scene.triangles.size();
        const size_t materialCount = scene.materials.size();
        zegfx::ProductionScene editorScene;
        editorScene.nodes = scene.nodes;
        editorScene.triangles = scene.triangles;
        editorScene.materials = scene.materials;
        editorScene.sourceName = scene.sourceName;
        editorScene.activeCameraNode = scene.activeCameraNode;
        editorScene.keyLightNode = scene.keyLightNode;
        std::copy(scene.cameraPosition, scene.cameraPosition + 3, editorScene.cameraPosition);
        std::copy(scene.cameraTarget, scene.cameraTarget + 3, editorScene.cameraTarget);
        std::copy(scene.cameraUp, scene.cameraUp + 3, editorScene.cameraUp);
        editorScene.verticalFovDegrees = scene.verticalFovDegrees;
        std::copy(scene.keyLightPosition, scene.keyLightPosition + 3, editorScene.keyLightPosition);
        std::copy(scene.keyLightColor, scene.keyLightColor + 3, editorScene.keyLightColor);
        std::vector<EditorState::SceneNodeBounds> nodeBounds(scene.nodes.size());
        std::vector<float> boundsMin(scene.nodes.size() * 3, std::numeric_limits<float>::infinity());
        std::vector<float> boundsMax(scene.nodes.size() * 3, -std::numeric_limits<float>::infinity());
        for (const auto& triangle : scene.triangles) {
            if (triangle.nodeIndex < 0 || static_cast<size_t>(triangle.nodeIndex) >= scene.nodes.size()) continue;
            const size_t nodeIndex = static_cast<size_t>(triangle.nodeIndex);
            const auto& transform = scene.nodes[nodeIndex].worldTransform;
            for (const auto& vertex : triangle.vertices) {
                const float worldPosition[3] = {
                    transform[0] * vertex.position[0] + transform[4] * vertex.position[1] +
                        transform[8] * vertex.position[2] + transform[12],
                    transform[1] * vertex.position[0] + transform[5] * vertex.position[1] +
                        transform[9] * vertex.position[2] + transform[13],
                    transform[2] * vertex.position[0] + transform[6] * vertex.position[1] +
                        transform[10] * vertex.position[2] + transform[14]
                };
                for (int axis = 0; axis < 3; ++axis) {
                    const size_t i = nodeIndex * 3 + static_cast<size_t>(axis);
                    boundsMin[i] = std::min(boundsMin[i], worldPosition[axis]);
                    boundsMax[i] = std::max(boundsMax[i], worldPosition[axis]);
                }
            }
        }
        for (size_t nodeIndex = 0; nodeIndex < scene.nodes.size(); ++nodeIndex) {
            auto& bounds = nodeBounds[nodeIndex];
            bool valid = true;
            float center[3]{};
            float radiusSquared = 0.0f;
            for (int axis = 0; axis < 3; ++axis) {
                const size_t i = nodeIndex * 3 + static_cast<size_t>(axis);
                valid = valid && std::isfinite(boundsMin[i]) && std::isfinite(boundsMax[i]);
                center[axis] = (boundsMin[i] + boundsMax[i]) * 0.5f;
                const float halfExtent = (boundsMax[i] - boundsMin[i]) * 0.5f;
                radiusSquared += halfExtent * halfExtent;
                bounds.centerOffset[axis] = center[axis] - scene.nodes[nodeIndex].worldTransform[12 + axis];
            }
            bounds.valid = valid;
            bounds.radius = valid ? std::sqrt(radiusSquared) : 0.0f;
        }
        render_view_valid_ = false;
        if (!gfx_.submit_production_scene(std::move(scene), error)) return false;
        if (editor_) {
            editor_->initialize_render_controls(editorScene, nodeBounds);
            std::error_code pathError;
            if (is_builtin_production_scene(path)) {
                editor_->get_state().asset_path = path;
            } else {
                const auto absolutePath = std::filesystem::absolute(std::filesystem::u8path(path), pathError);
                editor_->get_state().asset_path = pathError ? path : absolutePath.lexically_normal().u8string();
            }
            auto& state = editor_->get_state();
            state.scene_node_ids.resize(sceneNodeCount);
            state.scene_node_asset_ids.resize(sceneNodeCount);
            for (size_t i = 0; i < sceneNodeCount; ++i) {
                state.scene_node_ids[i] = assetId + "/node/" + std::to_string(i);
                state.scene_node_asset_ids[i] = assetId;
            }
            ProjectAssetReference asset;
            asset.id = assetId;
            asset.path = state.asset_path;
            asset.display_name = is_builtin_production_scene(path)
                ? "Starter Cube" : std::filesystem::u8path(path).stem().u8string();
            if (!is_builtin_production_scene(path)) {
                std::error_code metadataError;
                asset.source_size = std::filesystem::file_size(std::filesystem::u8path(path), metadataError);
                if (metadataError) asset.source_size = 0;
                metadataError.clear();
                const auto modified = std::filesystem::last_write_time(std::filesystem::u8path(path), metadataError);
                if (!metadataError) asset.source_modified_ticks = modified.time_since_epoch().count();
            }
            state.asset_references.assign(1, std::move(asset));
            state.material_references.clear();
            state.material_references.reserve(materialCount);
            for (size_t i = 0; i < materialCount; ++i)
                state.material_references.push_back({assetId + "/material/" + std::to_string(i),
                                                     assetId, static_cast<uint32_t>(i)});
            ensure_medusa_document_identity(state);
        }
        std::cerr << "[Medusa] Submitted " << triangleCount << " triangles to ZeGFX ProductionDCC." << std::endl;
        start_production_timer();
        request_redraw();
        return true;
    } catch (const std::bad_alloc&) {
        error = "Out of memory.";
    } catch (const std::exception& exception) {
        error = std::string("Asset import failed: ") + exception.what();
    } catch (...) {
        error = "Import failed.";
    }
    return false;
}

bool Window::build_scene_with_procedural_objects(zegfx::ProductionScene& scene, const EditorState& state,
                                                size_t firstExtraNode, std::string& error) const {
    if (state.scene_nodes.size() < firstExtraNode) {
        error = "The saved scene hierarchy does not match its source asset.";
        return false;
    }
    scene.nodes = state.scene_nodes;
    constexpr const char* marker = "medusa-object-primitive-";
    for (size_t i = firstExtraNode; i < state.scene_nodes.size(); ++i) {
        if (i >= state.scene_node_ids.size()) continue;
        const std::string& id = state.scene_node_ids[i];
        if (id.rfind(marker, 0) != 0) continue;
        const size_t shapeStart = std::char_traits<char>::length(marker);
        const size_t shapeEnd = id.find('-', shapeStart);
        if (shapeEnd == std::string::npos) {
            error = "A saved primitive object has an invalid identity.";
            return false;
        }
        const std::string shape = id.substr(shapeStart, shapeEnd - shapeStart);
        if (!append_builtin_primitive_geometry(scene, shape, static_cast<uint32_t>(i), error)) return false;
    }
    error.clear();
    return true;
}

bool Window::add_scene_object(const std::string& objectType, std::string& error) {
    if (!editor_) { error = "The editor is not ready."; return false; }
    auto& state = editor_->get_state();
    ensure_medusa_document_identity(state);

    zegfx::ProductionScene scene;
    size_t firstExtraNode = 0;
    if (!state.asset_path.empty()) {
        if (!load_production_scene(state.asset_path, scene, error)) return false;
        firstExtraNode = scene.nodes.size();
    } else {
        scene.sourceName = state.scene_name.empty() ? "Scene" : state.scene_name;
    }
    if (!build_scene_with_procedural_objects(scene, state, firstExtraNode, error)) return false;

    const auto uniqueName = [&state](const std::string& base) {
        const auto exists = [&state](const std::string& name) {
            return std::any_of(state.scene_nodes.begin(), state.scene_nodes.end(), [&name](const auto& node) {
                return node.name == name;
            });
        };
        if (!exists(base)) return base;
        for (uint32_t suffix = 2; suffix < 1000000; ++suffix) {
            const std::string candidate = base + "." + (suffix < 10 ? "00" : suffix < 100 ? "0" : "") +
                                          std::to_string(suffix);
            if (!exists(candidate)) return candidate;
        }
        return base + "." + std::to_string(state.scene_nodes.size() + 1);
    };

    uint32_t nodeIndex = static_cast<uint32_t>(state.scene_nodes.size());
    std::string name;
    std::string identityKind;
    if (objectType.rfind("primitive:", 0) == 0) {
        const std::string shape = objectType.substr(10);
        const char* shapeName = shape == "plane" ? "Plane" : shape == "cube" ? "Cube" :
                                shape == "circle" ? "Circle" : shape == "cylinder" ? "Cylinder" : nullptr;
        if (!shapeName) { error = "The requested mesh shape is not supported."; return false; }
        name = uniqueName(shapeName);
        if (!append_builtin_primitive_node(scene, shape, name, nodeIndex, error)) return false;
        identityKind = "primitive-" + shape;
    } else {
        zegfx::ProductionSceneNode node;
        float forward[3]{};
        const float radians = 0.017453292519943295f;
        const float pitch = state.camera_rotation[0] * radians;
        const float yaw = state.camera_rotation[1] * radians;
        const float cp = std::cos(pitch), sp = std::sin(pitch);
        const float sy = std::sin(yaw), cy = std::cos(yaw);
        forward[0] = sy * cp; forward[1] = sp; forward[2] = -cy * cp;
        float up[3] = {-sy * sp, cp, cy * sp};
        float right[3] = {cy, 0.0f, sy};
        const float roll = state.camera_rotation[2] * radians;
        const float cr = std::cos(roll), sr = std::sin(roll);
        for (int axis = 0; axis < 3; ++axis) {
            const float rolled = up[axis] * cr + right[axis] * sr;
            right[axis] = right[axis] * cr - up[axis] * sr;
            up[axis] = rolled;
        }

        if (objectType.rfind("light:", 0) == 0) {
            const std::string kind = objectType.substr(6);
            const char* baseName = kind == "point" ? "Point Light" : kind == "sun" ? "Sun Light" :
                                   kind == "spot" ? "Spot Light" : kind == "area" ? "Area Light" : nullptr;
            if (!baseName) { error = "The requested light type is not supported."; return false; }
            name = uniqueName(baseName);
            node.type = zegfx::ProductionNodeType::Light;
            node.lightType = kind == "sun" ? zegfx::LightType::Directional :
                             kind == "spot" ? zegfx::LightType::Spot : zegfx::LightType::Point;
            node.areaLight = kind == "area";
            node.lightEnergy = kind == "sun" ? 3.0f : 6.0f;
            node.lightSpotAngleDegrees = 45.0f;
            node.lightAreaSize = 2.0f;
            identityKind = "light-" + kind;
            for (int axis = 0; axis < 3; ++axis) {
                node.worldTransform[12 + axis] = state.camera_position[axis] + forward[axis] * 2.0f;
                node.worldTransform[axis] = right[axis];
                node.worldTransform[4 + axis] = up[axis];
                node.worldTransform[8 + axis] = -forward[axis];
            }
        } else if (objectType == "camera") {
            name = uniqueName("Camera");
            node.type = zegfx::ProductionNodeType::Camera;
            identityKind = "camera";
            for (int axis = 0; axis < 3; ++axis) {
                node.worldTransform[12 + axis] = state.camera_position[axis];
                node.worldTransform[axis] = right[axis];
                node.worldTransform[4 + axis] = up[axis];
                node.worldTransform[8 + axis] = -forward[axis];
            }
        } else if (objectType == "volume") {
            name = uniqueName("Volume");
            node.type = zegfx::ProductionNodeType::Group;
            identityKind = "volume";
        } else {
            error = "The requested scene object is not supported.";
            return false;
        }
        node.name = name;
        scene.nodes.push_back(node);
    }

    if (nodeIndex >= scene.nodes.size()) { error = "Could not append the new scene object."; return false; }
    state.scene_nodes.push_back(scene.nodes[nodeIndex]);
    state.scene_node_ids.push_back("medusa-object-" + identityKind + "-" + create_medusa_stable_id());
    state.scene_node_asset_ids.emplace_back();
    state.scene_node_bounds.emplace_back();
    state.rebuild_scene_hierarchy();
    ++state.scene_generation;

    if (state.scene_nodes[nodeIndex].type == zegfx::ProductionNodeType::Light) {
        const auto& light = state.scene_nodes[nodeIndex];
        state.key_light_node = static_cast<int>(nodeIndex);
        state.key_light_x = light.worldTransform[12];
        state.key_light_y = light.worldTransform[13];
        state.key_light_z = light.worldTransform[14];
        state.key_light_energy = light.lightEnergy;
    } else if (state.scene_nodes[nodeIndex].type == zegfx::ProductionNodeType::Camera) {
        state.active_camera_node = static_cast<int>(nodeIndex);
        state.active_camera = name;
    }
    if (state.active_workspace != "Shot") state.select_scene_node(nodeIndex);
    else if (state.scene_nodes[nodeIndex].type == zegfx::ProductionNodeType::Camera) state.select_active_camera();

    if (!scene.triangles.empty()) {
        state.scene_triangles = scene.triangles;
        state.scene_materials = scene.materials;
        if (!gfx_.submit_production_scene(std::move(scene), error)) return false;
        render_view_valid_ = false;
        sync_production_controls();
        start_production_timer();
    }
    error.clear();
    request_redraw();
    update_project_dirty();
    return true;
}

bool Window::choose_render_output(std::wstring& path, const wchar_t* title, bool confirmOverwrite) {
    std::wstring initialName = L"medusa_frame.png";
    bool useExr = false;
    if (editor_) {
        const auto& state = editor_->get_state();
        useExr = state.render_output_exr;
        if (!state.render_output_path.empty()) {
            const std::wstring savedPath = std::filesystem::u8path(state.render_output_path).wstring();
            if (savedPath.size() < MAX_PATH) initialName = savedPath;
        }
    }
    if (initialName == L"medusa_frame.png" && useExr) initialName = L"medusa_frame.exr";
    wchar_t file[MAX_PATH] = {};
    std::copy(initialName.begin(), initialName.end(), file);
    file[initialName.size()] = L'\0';
    OPENFILENAMEW dialog = {};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = hwnd_;
    dialog.lpstrFilter = L"PNG image (*.png)\0*.png\0OpenEXR image (*.exr)\0*.exr\0All files (*.*)\0*.*\0";
    dialog.lpstrFile = file;
    dialog.nMaxFile = MAX_PATH;
    dialog.nFilterIndex = useExr ? 2 : 1;
    dialog.lpstrDefExt = useExr ? L"exr" : L"png";
    dialog.lpstrTitle = title;
    dialog.Flags = OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (confirmOverwrite) dialog.Flags |= OFN_OVERWRITEPROMPT;
    if (!GetSaveFileNameW(&dialog)) return false;
    path = file;
    if (dialog.nFilterIndex == 1 || dialog.nFilterIndex == 2) {
        std::filesystem::path normalized(path);
        normalized.replace_extension(dialog.nFilterIndex == 1 ? L".png" : L".exr");
        path = normalized.wstring();
    }
    return true;
}

void Window::restore_interactive_render_size() {
    if (!sequence_output_size_active_ || !editor_) return;
    const Rect renderBounds = editor_->get_render_viewport_bounds();
    gfx_.resize_realtime_viewport(
        static_cast<uint32_t>(std::max(1.0f, renderBounds.width * gfx_.get_dpi_scale())),
        static_cast<uint32_t>(std::max(1.0f, renderBounds.height * gfx_.get_dpi_scale())));
    sequence_output_size_active_ = false;
}

bool Window::choose_project_path(bool save, std::wstring& path, const wchar_t* title) {
    std::vector<wchar_t> file(32768, L'\0');
    if (save && editor_ && !editor_->get_state().project_path.empty()) {
        const std::wstring current = std::filesystem::u8path(editor_->get_state().project_path).wstring();
        const size_t count = std::min(current.size(), file.size() - 1);
        std::copy(current.begin(), current.begin() + static_cast<std::ptrdiff_t>(count), file.begin());
    } else if (save) {
        const wchar_t defaultName[] = L"Untitled.medusa";
        std::copy(std::begin(defaultName), std::end(defaultName), file.begin());
    }

    OPENFILENAMEW dialog = {};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = hwnd_;
    dialog.lpstrFilter = L"Medusa project (*.medusa)\0*.medusa\0All files (*.*)\0*.*\0";
    dialog.lpstrFile = file.data();
    dialog.nMaxFile = static_cast<DWORD>(file.size());
    dialog.lpstrDefExt = L"medusa";
    dialog.lpstrTitle = title;
    dialog.Flags = save
        ? OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR
        : OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    const BOOL chosen = save ? GetSaveFileNameW(&dialog) : GetOpenFileNameW(&dialog);
    if (!chosen) return false;
    path.assign(file.data());
    return true;
}

bool Window::save_project(bool saveAs) {
    if (!editor_) return false;
    auto& state = editor_->get_state();
    ensure_medusa_document_identity(state);
    std::wstring path;
    if (!saveAs && !state.project_path.empty())
        path = std::filesystem::u8path(state.project_path).wstring();
    else if (!choose_project_path(true, path, L"Save Medusa Project"))
        return false;

    const std::string utf8Path = wide_to_utf8(path);
    const std::string previousPath = state.project_path;
    state.project_path = utf8Path;
    std::string error;
    if (!save_medusa_project(state, utf8Path, error)) {
        state.project_path = previousPath;
        MessageBoxA(hwnd_, error.c_str(), "Medusa Platinum - Save Project", MB_ICONERROR | MB_OK);
        return false;
    }
    state.project_dirty = false;
    discard_changes_confirmed_ = false;
    saved_project_snapshot_ = medusa_project_snapshot(state);
    const std::string recovery = recovery_file_path();
    if (!recovery.empty()) {
        std::error_code ec;
        std::filesystem::remove(std::filesystem::u8path(recovery), ec);
    }
    set_project_title();
    request_redraw();
    return true;
}

bool Window::confirm_project_change() {
    discard_changes_confirmed_ = false;
    if (!editor_ || !editor_->get_state().project_dirty) return true;
    const int answer = MessageBoxW(hwnd_,
        L"This project has unsaved changes. Save them before continuing?",
        L"Medusa Platinum - Unsaved Project", MB_YESNOCANCEL | MB_ICONWARNING);
    if (answer == IDCANCEL) return false;
    if (answer == IDNO) { discard_changes_confirmed_ = true; return true; }
    return save_project(false);
}

bool Window::apply_project_node_overrides(
    const std::vector<zegfx::ProductionSceneNode>& imported,
    const std::vector<zegfx::ProductionSceneNode>& saved,
    const std::vector<std::string>& importedIds,
    const std::vector<std::string>& savedIds,
    std::string& error) {
    if (imported.size() != saved.size() || importedIds.size() != imported.size() ||
        savedIds.size() != saved.size()) {
        error = "The project's saved scene hierarchy no longer matches its linked asset.";
        return false;
    }
    for (size_t i = 0; i < saved.size(); ++i) {
        if (importedIds[i] != savedIds[i] || imported[i].name != saved[i].name || imported[i].type != saved[i].type ||
            imported[i].parentIndex != saved[i].parentIndex) {
            error = "The project's saved scene hierarchy no longer matches its linked asset.";
            return false;
        }
    }

    for (size_t i = 0; i < saved.size(); ++i) {
        if (imported[i].visible == saved[i].visible && imported[i].renderable == saved[i].renderable) continue;
        if (!gfx_.set_production_node_state(static_cast<uint32_t>(i), saved[i].visible,
                                             saved[i].renderable, error)) return false;
    }

    std::vector<size_t> order(saved.size());
    for (size_t i = 0; i < order.size(); ++i) order[i] = i;
    auto depth = [&saved](size_t index) {
        size_t result = 0;
        int parent = saved[index].parentIndex;
        while (parent >= 0 && static_cast<size_t>(parent) < saved.size() && result < saved.size()) {
            ++result;
            parent = saved[static_cast<size_t>(parent)].parentIndex;
        }
        return result;
    };
    std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) { return depth(a) < depth(b); });

    for (size_t i : order) {
        const auto type = saved[i].type;
        if (type != zegfx::ProductionNodeType::Mesh && type != zegfx::ProductionNodeType::Group) continue;
        bool moved = false;
        for (int axis = 0; axis < 3; ++axis)
            moved = moved || imported[i].worldTransform[12 + axis] != saved[i].worldTransform[12 + axis];
        if (moved) {
            const float position[3] = {saved[i].worldTransform[12], saved[i].worldTransform[13], saved[i].worldTransform[14]};
            if (!gfx_.set_production_node_translation(static_cast<uint32_t>(i), position, error)) return false;
        }
        bool rotatedOrScaled = false;
        for (int axis = 0; axis < 3; ++axis)
            rotatedOrScaled = rotatedOrScaled || imported[i].rotationOffset[axis] != saved[i].rotationOffset[axis] ||
                imported[i].scaleFactor[axis] != saved[i].scaleFactor[axis];
        if (rotatedOrScaled) {
            std::vector<zegfx::ProductionSceneNode> updated;
            if (!gfx_.set_production_node_rotation_scale(static_cast<uint32_t>(i),
                    saved[i].rotationOffset, saved[i].scaleFactor, updated, error)) return false;
        }
    }
    error.clear();
    return true;
}

bool Window::open_project(const std::string& path, std::string& error) {
    EditorState candidate;
    if (!load_medusa_project(path, candidate, error)) return false;
    return apply_project_state(std::move(candidate), error);
}

bool Window::apply_project_state(EditorState candidate, std::string& error, bool establishSavedBaseline) {
    if (!candidate.asset_path.empty()) {
        std::error_code ec;
        if (!is_builtin_production_scene(candidate.asset_path) &&
            (!std::filesystem::exists(std::filesystem::u8path(candidate.asset_path), ec) || ec)) {
            error = "The project's linked asset could not be found:\n" + candidate.asset_path;
            return false;
        }
        const std::string stableAssetId = candidate.asset_references.empty()
            ? std::string() : candidate.asset_references.front().id;
        if (!load_production_asset(candidate.asset_path, error, stableAssetId)) return false;
        const EditorState importedState = editor_->get_state();
        const auto importedNodes = importedState.scene_nodes;
        bool proceduralAdditionsRestored = false;
        bool importedPrefixMatches = candidate.scene_nodes.size() >= importedNodes.size() &&
            importedState.scene_node_ids.size() == importedNodes.size() &&
            candidate.scene_node_ids.size() == candidate.scene_nodes.size();
        if (importedPrefixMatches) {
            for (size_t i = 0; i < importedNodes.size(); ++i) {
                if (importedState.scene_node_ids[i] != candidate.scene_node_ids[i] ||
                    importedNodes[i].name != candidate.scene_nodes[i].name ||
                    importedNodes[i].type != candidate.scene_nodes[i].type ||
                    importedNodes[i].parentIndex != candidate.scene_nodes[i].parentIndex) {
                    importedPrefixMatches = false;
                    break;
                }
            }
        }
        bool supportedAdditions = importedPrefixMatches;
        if (supportedAdditions) {
            for (size_t i = importedNodes.size(); i < candidate.scene_nodes.size(); ++i) {
                if (candidate.scene_node_ids[i].rfind("medusa-object-", 0) != 0) {
                    supportedAdditions = false;
                    break;
                }
            }
        }
        if (supportedAdditions && candidate.scene_nodes.size() > importedNodes.size()) {
            zegfx::ProductionScene composed;
            if (!load_production_scene(candidate.asset_path, composed, error) ||
                !build_scene_with_procedural_objects(composed, candidate, importedNodes.size(), error) ||
                !gfx_.submit_production_scene(std::move(composed), error)) return false;
            proceduralAdditionsRestored = true;
            render_view_valid_ = false;
        }
        if (!proceduralAdditionsRestored) candidate.key_light_node = importedState.key_light_node;
        if (!proceduralAdditionsRestored && candidate.key_light_node < 0) {
            candidate.key_light_x = importedState.key_light_x;
            candidate.key_light_y = importedState.key_light_y;
            candidate.key_light_z = importedState.key_light_z;
            candidate.key_light_energy = 0.0f;
        }
        candidate.scene_node_bounds = importedState.scene_node_bounds;
        if (candidate.material_references.empty())
            candidate.material_references = importedState.material_references;
        bool matches = candidate.scene_nodes.size() == importedNodes.size() || proceduralAdditionsRestored;
        matches = matches && importedState.scene_node_ids.size() == importedNodes.size() &&
            candidate.scene_node_ids.size() == candidate.scene_nodes.size();
        if (matches) {
            for (size_t i = 0; i < importedNodes.size(); ++i) {
                if (importedState.scene_node_ids[i] != candidate.scene_node_ids[i] ||
                    importedNodes[i].name != candidate.scene_nodes[i].name ||
                    importedNodes[i].type != candidate.scene_nodes[i].type ||
                    importedNodes[i].parentIndex != candidate.scene_nodes[i].parentIndex) {
                    matches = false;
                    break;
                }
            }
        }
        if (matches && !proceduralAdditionsRestored) {
            if (!apply_project_node_overrides(importedNodes, candidate.scene_nodes,
                    importedState.scene_node_ids, candidate.scene_node_ids, error)) return false;
        } else if (!matches) {
            candidate.scene_nodes = importedNodes;
            candidate.scene_node_ids = importedState.scene_node_ids;
            candidate.scene_node_asset_ids = importedState.scene_node_asset_ids;
            candidate.active_camera_node = importedState.active_camera_node;
            candidate.key_light_node = importedState.key_light_node;
            candidate.selected_scene_node = importedState.selected_scene_node;
            candidate.active_camera = importedState.active_camera;
            MessageBoxW(hwnd_,
                L"The linked asset's object hierarchy has changed. Its current hierarchy was loaded; saved per-object overrides were skipped.",
                L"Medusa Platinum - Project Update", MB_ICONINFORMATION | MB_OK);
        }
        candidate.asset_path = importedState.asset_path;
    } else {
        gfx_.clear_production_scene();
        candidate.scene_node_bounds.clear();
        const bool hasProceduralMesh = std::any_of(candidate.scene_node_ids.begin(), candidate.scene_node_ids.end(),
            [](const std::string& id) { return id.rfind("medusa-object-primitive-", 0) == 0; });
        if (hasProceduralMesh) {
            zegfx::ProductionScene composed;
            composed.sourceName = candidate.scene_name.empty() ? "Scene" : candidate.scene_name;
            if (!build_scene_with_procedural_objects(composed, candidate, 0, error) ||
                !gfx_.submit_production_scene(std::move(composed), error)) return false;
            render_view_valid_ = false;
        }
    }

    candidate.pending_command.clear();
    candidate.pending_scene_node = -1;
    candidate.asset_library_root = editor_->get_state().asset_library_root;
    candidate.render_output_path = editor_->get_state().render_output_path;
    candidate.asset_library_selected_path.clear();
    candidate.project_dirty = !establishSavedBaseline && candidate.project_dirty;
    candidate.rebuild_scene_hierarchy();
    editor_->get_state() = std::move(candidate);
    auto& loadedState = editor_->get_state();
    editor_->set_layout_settings(loadedState.ui_toolshelf_width, loadedState.ui_right_deck_width,
                                 loadedState.ui_outliner_ratio, loadedState.ui_timeline_height);
    render_view_valid_ = false;
    render_settings_valid_ = false;
    if (!editor_->get_state().keyframes.empty()) editor_->get_state().evaluate_keyframes();
    if (!editor_->get_state().pending_command.empty()) dispatch_editor_command();
    sync_production_controls();
    gfx_.restart_production_render();
    start_production_timer();
    if (establishSavedBaseline) saved_project_snapshot_ = medusa_project_snapshot(editor_->get_state());
    else update_project_dirty();
    if (establishSavedBaseline) clear_history();
    if (establishSavedBaseline) discard_changes_confirmed_ = false;
    set_project_title();
    request_redraw();
    error.clear();
    return true;
}

void Window::set_project_title() {
    if (!hwnd_) return;
    std::wstring title = kWindowTitle;
    if (editor_ && !editor_->get_state().project_path.empty()) {
        const std::wstring filename = std::filesystem::u8path(editor_->get_state().project_path).filename().wstring();
        title += L" - " + filename;
    }
    if (editor_ && editor_->get_state().project_dirty) title += L" *";
    SetWindowTextW(hwnd_, title.c_str());
}

void Window::update_project_dirty() {
    if (!editor_) return;
    auto& state = editor_->get_state();
    state.project_dirty = medusa_project_snapshot(state) != saved_project_snapshot_;
    if (state.project_dirty) discard_changes_confirmed_ = false;
    set_project_title();
}

void Window::begin_history_transaction() {
    if (!editor_ || !interaction_snapshot_.empty()) return;
    interaction_project_id_ = editor_->get_state().project_id;
    interaction_snapshot_ = medusa_project_snapshot(editor_->get_state());
}

void Window::commit_history_transaction() {
    if (suppress_history_commit_) {
        suppress_history_commit_ = false;
        interaction_snapshot_.clear();
        interaction_project_id_.clear();
        sync_history_flags();
        return;
    }
    if (!editor_ || interaction_snapshot_.empty()) return;
    std::string before = std::move(interaction_snapshot_);
    const std::string beforeProjectId = std::move(interaction_project_id_);
    interaction_snapshot_.clear();
    interaction_project_id_.clear();
    auto& state = editor_->get_state();
    if (beforeProjectId != state.project_id) {
        clear_history();
        return;
    }
    std::string after = medusa_project_snapshot(state);
    if (before == after) return;

    while (history_.size() > history_cursor_) {
        history_bytes_ -= history_.back().bytes;
        history_.pop_back();
    }
    constexpr size_t kHistoryBudget = 64u * 1024u * 1024u;
    constexpr size_t kHistoryCommandLimit = 64;
    const size_t bytes = before.size() + after.size();
    if (bytes > kHistoryBudget) {
        clear_history();
        sync_history_flags();
        return;
    }
    history_.push_back({std::move(before), std::move(after), bytes});
    history_bytes_ += bytes;
    history_cursor_ = history_.size();
    while (history_.size() > kHistoryCommandLimit || history_bytes_ > kHistoryBudget) {
        history_bytes_ -= history_.front().bytes;
        history_.pop_front();
        --history_cursor_;
    }
    sync_history_flags();
}

void Window::clear_history() {
    history_.clear();
    history_cursor_ = 0;
    history_bytes_ = 0;
    interaction_snapshot_.clear();
    interaction_project_id_.clear();
    suppress_history_commit_ = false;
    sync_history_flags();
}

void Window::sync_history_flags() {
    if (!editor_) return;
    auto& state = editor_->get_state();
    state.history_can_undo = history_cursor_ > 0;
    state.history_can_redo = history_cursor_ < history_.size();
}

bool Window::restore_project_snapshot(const std::string& snapshot, std::string& error) {
    if (!editor_) { error = "The editor is not ready to restore history."; return false; }
    EditorState candidate = editor_->get_state();
    if (!restore_medusa_project_snapshot(snapshot, candidate, error)) return false;
    return apply_project_state(std::move(candidate), error, false);
}

bool Window::apply_history_step(bool redo) {
    if ((!redo && history_cursor_ == 0) || (redo && history_cursor_ >= history_.size())) return false;
    const size_t index = redo ? history_cursor_ : history_cursor_ - 1;
    const std::string snapshot = redo ? history_[index].after : history_[index].before;
    std::string error;
    if (!restore_project_snapshot(snapshot, error)) {
        MessageBoxA(hwnd_, error.c_str(), "Medusa Platinum - Edit History", MB_ICONERROR | MB_OK);
        return false;
    }
    history_cursor_ = redo ? index + 1 : index;
    suppress_history_commit_ = true;
    sync_history_flags();
    request_redraw();
    return true;
}

std::string Window::recovery_file_path() const {
    PWSTR localAppData = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &localAppData)) ||
        !localAppData) return {};
    const std::filesystem::path path = std::filesystem::path(localAppData) /
        L"Medusa Platinum" / L"Recovery" / L"last-session.medusa-recovery";
    CoTaskMemFree(localAppData);
    return path.u8string();
}

void Window::save_recovery_snapshot() {
    if (!editor_ || !editor_->get_state().project_dirty) return;
    const std::string path = recovery_file_path();
    if (path.empty()) return;
    std::string error;
    if (!save_medusa_recovery(editor_->get_state(), path, editor_->get_state().project_path, error) && !error.empty())
        std::cerr << "[Medusa] Recovery snapshot could not be saved: " << error << std::endl;
}

void Window::maybe_restore_recovery() {
    const std::string recoveryPath = recovery_file_path();
    if (recoveryPath.empty()) return;
    std::error_code ec;
    if (!std::filesystem::exists(std::filesystem::u8path(recoveryPath), ec) || ec) return;

    EditorState recovered;
    std::string sourceProjectPath, error;
    if (!load_medusa_recovery(recoveryPath, recovered, sourceProjectPath, error)) {
        std::cerr << "[Medusa] Ignoring invalid recovery snapshot: " << error << std::endl;
        std::filesystem::remove(std::filesystem::u8path(recoveryPath), ec);
        return;
    }
    bool newerThanProject = sourceProjectPath.empty();
    if (!sourceProjectPath.empty()) {
        const auto projectPath = std::filesystem::u8path(sourceProjectPath);
        const auto recoveryTime = std::filesystem::last_write_time(std::filesystem::u8path(recoveryPath), ec);
        if (ec) return;
        if (!std::filesystem::exists(projectPath, ec) || ec) {
            newerThanProject = true;
        } else {
            const auto projectTime = std::filesystem::last_write_time(projectPath, ec);
            if (ec) return;
            newerThanProject = recoveryTime > projectTime;
        }
    }
    if (!newerThanProject) {
        std::filesystem::remove(std::filesystem::u8path(recoveryPath), ec);
        return;
    }

    std::wstring prompt = L"An unsaved Medusa recovery snapshot was found";
    if (!sourceProjectPath.empty()) prompt += L" for:\n" + utf8_to_wide(sourceProjectPath);
    prompt += L"\n\nRestore it?";
    if (MessageBoxW(hwnd_, prompt.c_str(), L"Medusa Platinum - Recovery", MB_YESNO | MB_ICONQUESTION) != IDYES) {
        std::filesystem::remove(std::filesystem::u8path(recoveryPath), ec);
        return;
    }

    if (!sourceProjectPath.empty() && std::filesystem::exists(std::filesystem::u8path(sourceProjectPath), ec) && !ec) {
        EditorState saved;
        if (load_medusa_project(sourceProjectPath, saved, error))
            saved_project_snapshot_ = medusa_project_snapshot(saved);
    }
    if (!apply_project_state(std::move(recovered), error, false)) {
        MessageBoxA(hwnd_, error.c_str(), "Medusa Platinum - Recovery Error", MB_ICONERROR | MB_OK);
        return;
    }
    editor_->get_state().project_dirty = true;
    update_project_dirty();
    clear_history();
}

bool Window::sync_production_controls() {
    if (!editor_) return false;
    auto& state = editor_->get_state();
    bool changed = false;

    zegfx::ProductionRenderSettings settings;
    settings.maxSamplesPerPixel = static_cast<uint32_t>(std::clamp(state.max_samples_per_pixel, 1, 4096));
    settings.clampFireflies = std::max(0.0f, state.firefly_clamp);
    auto sameSettings = [](const zegfx::ProductionRenderSettings& a, const zegfx::ProductionRenderSettings& b) {
        return a.maxSamplesPerPixel == b.maxSamplesPerPixel && a.clampFireflies == b.clampFireflies;
    };
    if (!render_settings_valid_ || !sameSettings(settings, last_render_settings_)) {
        std::string error;
        if (gfx_.update_production_settings(settings, error)) {
            last_render_settings_ = settings;
            render_settings_valid_ = true;
            changed = gfx_.has_production_scene();
        } else if (gfx_.has_production_scene()) {
            std::cerr << "[Medusa] Could not apply production settings: " << error << std::endl;
        }
    }

    if (!gfx_.has_production_scene()) return changed;

    const bool useSceneCamera = sequence_render_active_ || export_current_on_complete_;
    const bool useViewportCamera = state.viewport_navigation_active && !useSceneCamera;
    const float* cameraPosition = useViewportCamera ? state.viewport_position : state.camera_position;
    const float* cameraRotation = useViewportCamera ? state.viewport_rotation : state.camera_rotation;
    constexpr float radians = 0.017453292519943295f;
    const float pitch = cameraRotation[0] * radians;
    const float yaw = cameraRotation[1] * radians;
    const float roll = cameraRotation[2] * radians;
    const float cp = std::cos(pitch), sp = std::sin(pitch);
    const float sy = std::sin(yaw), cy = std::cos(yaw);
    float forward[3] = {sy * cp, sp, -cy * cp};
    float right[3] = {cy, 0.0f, sy};
    float up[3] = {-sy * sp, cp, cy * sp};
    const float cr = std::cos(roll), sr = std::sin(roll);
    float rolledUp[3] = {up[0] * cr + right[0] * sr,
                         up[1] * cr + right[1] * sr,
                         up[2] * cr + right[2] * sr};

    zegfx::ProductionViewSettings view;
    view.cameraPosition[0] = cameraPosition[0];
    view.cameraPosition[1] = cameraPosition[1];
    view.cameraPosition[2] = cameraPosition[2];
    const float targetDistance = std::max(0.1f, useViewportCamera ? state.viewport_distance : state.focus_distance);
    for (int i = 0; i < 3; ++i) {
        view.cameraTarget[i] = useViewportCamera ? state.viewport_target[i] : view.cameraPosition[i] + forward[i] * targetDistance;
        view.cameraUp[i] = rolledUp[i];
    }
    const float focal = std::clamp(state.camera_focal_length, 10.0f, 500.0f);
    view.verticalFovDegrees = 2.0f * std::atan(18.0f / focal) / radians;
    view.keyLightPosition[0] = state.key_light_x;
    view.keyLightPosition[1] = state.key_light_y;
    view.keyLightPosition[2] = state.key_light_z;
    const bool hasProceduralLight = std::any_of(state.scene_node_ids.begin(), state.scene_node_ids.end(),
        [](const std::string& id) { return id.rfind("medusa-object-light-", 0) == 0; });
    view.keyLightColor[0] = hasProceduralLight ? 0.0f : 3.5f * state.key_light_energy;
    view.keyLightColor[1] = hasProceduralLight ? 0.0f : 3.2f * state.key_light_energy;
    view.keyLightColor[2] = hasProceduralLight ? 0.0f : 2.8f * state.key_light_energy;
    auto sameView = [](const zegfx::ProductionViewSettings& a, const zegfx::ProductionViewSettings& b) {
        for (int i = 0; i < 3; ++i) {
            if (a.cameraPosition[i] != b.cameraPosition[i] || a.cameraTarget[i] != b.cameraTarget[i] ||
                a.cameraUp[i] != b.cameraUp[i] || a.keyLightPosition[i] != b.keyLightPosition[i] ||
                a.keyLightColor[i] != b.keyLightColor[i]) return false;
        }
        return a.verticalFovDegrees == b.verticalFovDegrees;
    };
    const bool viewportViewChanged = gfx_.set_realtime_view(view);
    if (viewportViewChanged && !useSceneCamera) {
        gfx_.set_realtime_interactive(true);
        KillTimer(hwnd_, 9);
        SetTimer(hwnd_, 9, 300, nullptr);
    }
    if (useSceneCamera && (!render_view_valid_ || !sameView(view, last_render_view_))) {
        std::string error;
        if (gfx_.update_production_view(view, error)) {
            last_render_view_ = view;
            render_view_valid_ = true;
            changed = true;
        } else {
            std::cerr << "[Medusa] Could not apply production view: " << error << std::endl;
        }
    }
    if (changed) start_production_timer();
    return changed;
}

void Window::start_animation_timer() {
    if (!hwnd_ || !editor_) return;
    const int fps = std::clamp(editor_->get_state().playback_fps, 1, 240);
    const UINT interval = static_cast<UINT>(std::max(1, 1000 / fps));
    if (animation_timer_active_ && animation_timer_interval_ == interval) return;
    animation_timer_active_ = SetTimer(hwnd_, 3, interval, nullptr) != 0;
    animation_timer_interval_ = animation_timer_active_ ? interval : 0;
}

void Window::on_production_frame_complete() {
    auto exportPath = [this](const std::wstring& widePath) {
        const int bytes = WideCharToMultiByte(CP_UTF8, 0, widePath.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (bytes <= 1) return std::string();
        std::string path(static_cast<size_t>(bytes), '\0');
        WideCharToMultiByte(CP_UTF8, 0, widePath.c_str(), -1, path.data(), bytes, nullptr, nullptr);
        path.resize(static_cast<size_t>(bytes - 1));
        return path;
    };

    if (export_current_on_complete_) {
        std::string error;
        const std::string path = exportPath(current_export_path_);
        const bool exported = !path.empty() && (current_export_exr_
            ? gfx_.export_production_exr(path, error)
            : gfx_.export_production_png(path, error));
        if (!exported)
            MessageBoxA(hwnd_, error.empty() ? "Could not export the rendered frame." : error.c_str(),
                        "Medusa Platinum - Export Error", MB_ICONERROR | MB_OK);
        export_current_on_complete_ = false;
        gfx_.set_production_render_active(sequence_render_active_);
        current_export_path_.clear();
        if (!sequence_render_active_) {
            render_view_valid_ = false;
            sync_production_controls();
            request_redraw();
        }
    }

    if (!sequence_render_active_) return;
    std::wostringstream filename;
    filename << sequence_prefix_ << L"_" << std::setw(4) << std::setfill(L'0') << sequence_frame_
             << (sequence_export_exr_ ? L".exr" : L".png");
    std::string error;
    const std::string path = exportPath(filename.str());
    const bool exported = !path.empty() && (sequence_export_exr_
        ? gfx_.export_production_exr(path, error)
        : gfx_.export_production_png(path, error));
    if (!exported) {
        sequence_render_active_ = false;
        gfx_.set_production_render_active(false);
        restore_interactive_render_size();
        render_view_valid_ = false;
        sync_production_controls();
        MessageBoxA(hwnd_, error.empty() ? "Could not export the rendered sequence frame." : error.c_str(),
                    "Medusa Platinum - Sequence Error", MB_ICONERROR | MB_OK);
        return;
    }

    if (sequence_frame_ >= sequence_end_frame_) {
        sequence_render_active_ = false;
        gfx_.set_production_render_active(false);
        restore_interactive_render_size();
        render_view_valid_ = false;
        sync_production_controls();
        MessageBoxW(hwnd_, L"The render sequence is complete.", L"Medusa Platinum", MB_ICONINFORMATION | MB_OK);
        return;
    }
    ++sequence_frame_;
    if (editor_) {
        editor_->get_state().current_frame = sequence_frame_;
        editor_->get_state().evaluate_keyframes();
        defer_project_dirty_ = true;
        dispatch_editor_command();
        defer_project_dirty_ = false;
    }
    sync_production_controls();
    gfx_.restart_production_render();
    start_production_timer();
    request_redraw();
}

void Window::on_paint() {
    PAINTSTRUCT ps;
    BeginPaint(hwnd_, &ps);
    render_frame();
    EndPaint(hwnd_, &ps);
}

void Window::on_resize(uint32_t width, uint32_t height) {
    if (width == 0 || height == 0) return;
    client_width_ = width;
    client_height_ = height;

    if (is_sizing_) {
        resize_pending_ = true;
        return;
    }

    apply_resize(width, height);
}

void Window::apply_resize(uint32_t width, uint32_t height) {
    gfx_.resize(width, height);
    if (editor_) {
        float dip_w = static_cast<float>(width) / gfx_.get_dpi_scale();
        float dip_h = static_cast<float>(height) / gfx_.get_dpi_scale();
        editor_->rebuild_layout(dip_w, dip_h);
        if (!sequence_output_size_active_) {
            const Rect render_bounds = editor_->get_render_viewport_bounds();
            gfx_.resize_realtime_viewport(
                static_cast<uint32_t>(std::max(1.0f, render_bounds.width * gfx_.get_dpi_scale())),
                static_cast<uint32_t>(std::max(1.0f, render_bounds.height * gfx_.get_dpi_scale())));
        }
    }

    start_production_timer();
    request_redraw();
}

void Window::on_mouse_move(int x, int y, WPARAM wParam) {
    if (!mouse_tracking_) {
        TRACKMOUSEEVENT tme = {};
        tme.cbSize = sizeof(TRACKMOUSEEVENT);
        tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = hwnd_;
        TrackMouseEvent(&tme);
        mouse_tracking_ = true;
    }

    float scale = gfx_.get_dpi_scale();
    MouseEvent ev;
    ev.pos = Point(static_cast<float>(x) / scale, static_cast<float>(y) / scale);
    ev.button = MouseButton::None;
    ev.shift = (wParam & MK_SHIFT) != 0;
    ev.ctrl = (wParam & MK_CONTROL) != 0;
    ev.alt = (GetKeyState(VK_MENU) & 0x8000) != 0;

    if (editor_) {
        KillTimer(hwnd_, 2);
        SetTimer(hwnd_, 2, 1000, nullptr);
        const bool changed = editor_->handle_mouse_move(ev);
        if (changed) {
            request_redraw();
            if (!editor_->get_state().pending_command.empty()) {
                const auto& command = editor_->get_state().pending_command;
                const bool sceneTransform = command == "scene_node_translation" || command == "scene_node_rotation_scale";
                const auto now = std::chrono::steady_clock::now();
                const bool throttle = sceneTransform && last_transform_submit_.time_since_epoch().count() != 0 &&
                    now - last_transform_submit_ < std::chrono::milliseconds(20);
                if (!throttle) {
                    defer_project_dirty_ = true;
                    dispatch_editor_command();
                    defer_project_dirty_ = false;
                    if (sceneTransform) last_transform_submit_ = now;
                }
            }
            sync_production_controls();
        }
    }
}

void Window::on_mouse_down(MouseButton btn, int x, int y, WPARAM wParam) {
    SetFocus(hwnd_);
    SetCapture(hwnd_);
    is_dragging_ = true;
    last_transform_submit_ = {};

    float scale = gfx_.get_dpi_scale();
    MouseEvent ev;
    ev.pos = Point(static_cast<float>(x) / scale, static_cast<float>(y) / scale);
    ev.button = btn;
    ev.shift = (wParam & MK_SHIFT) != 0;
    ev.ctrl = (wParam & MK_CONTROL) != 0;
    ev.alt = (GetKeyState(VK_MENU) & 0x8000) != 0;

    if (editor_) {
        begin_history_transaction();
        if (editor_->handle_mouse_down(ev)) {
            request_redraw();
            sync_production_controls();
        }
        dispatch_editor_command();
        if (editor_->asset_drawer_animating()) SetTimer(hwnd_, 6, 16, nullptr);
    }
}

void Window::on_mouse_up(MouseButton btn, int x, int y, WPARAM wParam) {
    if (is_dragging_) {
        ReleaseCapture();
        is_dragging_ = false;
    }

    float scale = gfx_.get_dpi_scale();
    MouseEvent ev;
    ev.pos = Point(static_cast<float>(x) / scale, static_cast<float>(y) / scale);
    ev.button = btn;
    ev.shift = (wParam & MK_SHIFT) != 0;
    ev.ctrl = (wParam & MK_CONTROL) != 0;
    ev.alt = (GetKeyState(VK_MENU) & 0x8000) != 0;

    if (editor_) {
        if (editor_->handle_mouse_up(ev)) {
            request_redraw();
            dispatch_editor_command();
            sync_production_controls();
        }
        else dispatch_editor_command();
        commit_history_transaction();
    }
}

void Window::on_mouse_wheel(float delta, int x, int y, WPARAM wParam) {
    POINT pt = { x, y };
    ScreenToClient(hwnd_, &pt);

    float scale = gfx_.get_dpi_scale();
    Point p(static_cast<float>(pt.x) / scale, static_cast<float>(pt.y) / scale);

    if (editor_) {
        begin_history_transaction();
        if (editor_->handle_mouse_wheel(delta, p)) {
            request_redraw();
            sync_production_controls();
            dispatch_editor_command();
            commit_history_transaction();
        } else {
            interaction_snapshot_.clear();
            interaction_project_id_.clear();
        }
    }
}

void Window::on_mouse_leave() {
    mouse_tracking_ = false;
    KillTimer(hwnd_, 2);
    // Captured drags can legitimately leave the client area. Do not feed the
    // synthetic leave coordinate into an active transform or camera gesture.
    if (is_dragging_) return;
    // Notify editor that mouse left
    MouseEvent ev;
    ev.pos = Point(-9999.0f, -9999.0f);
    ev.button = MouseButton::None;
    if (editor_) {
        if (editor_->handle_mouse_move(ev)) {
            request_redraw();
        }
    }
}

LRESULT CALLBACK Window::window_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    Window *self = nullptr;
    if (msg == WM_NCCREATE) {
        CREATESTRUCTW *cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = reinterpret_cast<Window*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->hwnd_ = hwnd;
    } else {
        self = reinterpret_cast<Window*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (self) {
        return self->handle_message(msg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT Window::handle_message(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_SETCURSOR:
            if (LOWORD(lParam) == HTCLIENT && editor_) {
                CursorType cur = editor_->get_cursor_type();
                if (cur == CursorType::SizeWE) {
                    SetCursor(LoadCursor(nullptr, IDC_SIZEWE));
                    return TRUE;
                } else if (cur == CursorType::SizeNS) {
                    SetCursor(LoadCursor(nullptr, IDC_SIZENS));
                    return TRUE;
                }
            }
            break;

        case WM_ERASEBKGND:
            // Suppress standard GDI flicker
            return 1;

        case WM_PAINT:
            on_paint();
            return 0;

        case WM_CLOSE:
            if (!confirm_project_change()) return 0;
            DestroyWindow(hwnd_);
            return 0;

        case WM_TIMER:
            if (wParam == 6) {
                if (!editor_ || !editor_->tick_asset_drawer_animation()) {
                    KillTimer(hwnd_, 6);
                    request_redraw();
                    return 0;
                }
                request_redraw();
                return 0;
            }
            if (wParam == 4) {
                const bool shotCameraMode = editor_ && editor_->get_state().active_workspace == "Shot";
                if (!editor_ || (!editor_->get_state().camera_pilot_active && !free_look_active_ && !shotCameraMode) ||
                    (free_camera_keys_ == 0 && !free_look_active_)) {
                    KillTimer(hwnd_, 4);
                    if (free_camera_keys_ != 0) {
                        free_camera_keys_ = 0;
                        update_project_dirty();
                    }
                    free_camera_mouse_dx_ = free_camera_mouse_dy_ = 0;
                    free_camera_last_tick_ = {};
                    return 0;
                }
                const int mouseDx = static_cast<int>(std::clamp<int64_t>(free_camera_mouse_dx_, -2000, 2000));
                const int mouseDy = static_cast<int>(std::clamp<int64_t>(free_camera_mouse_dy_, -2000, 2000));
                free_camera_mouse_dx_ = free_camera_mouse_dy_ = 0;
                bool cameraChanged = (mouseDx != 0 || mouseDy != 0) &&
                    editor_->handle_raw_mouse_delta(mouseDx, mouseDy);
                const auto held = [this](uint32_t bit) { return (free_camera_keys_ & bit) != 0; };
                const float forward = (held(1u << 0) ? 1.0f : 0.0f) - (held(1u << 1) ? 1.0f : 0.0f);
                const float right = (held(1u << 3) ? 1.0f : 0.0f) - (held(1u << 2) ? 1.0f : 0.0f);
                const float vertical = (held(1u << 5) ? 1.0f : 0.0f) - (held(1u << 4) ? 1.0f : 0.0f);
                if (forward != 0.0f || right != 0.0f || vertical != 0.0f) {
                    float speed = 1.0f;
                    if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0) speed *= 4.0f;
                    if ((GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0) speed *= 0.25f;
                    const auto now = std::chrono::steady_clock::now();
                    float deltaSeconds = 0.016f;
                    if (free_camera_last_tick_.time_since_epoch().count() != 0) {
                        deltaSeconds = std::chrono::duration<float>(now - free_camera_last_tick_).count();
                    }
                    free_camera_last_tick_ = now;
                    deltaSeconds = std::clamp(deltaSeconds, 0.001f, 0.08f);
                    if (editor_->get_state().active_workspace == "Shot")
                        editor_->get_state().move_active_camera(forward, right, vertical, deltaSeconds, speed);
                    else
                        editor_->get_state().move_viewport(forward, right, vertical, deltaSeconds, speed);
                    cameraChanged = true;
                } else {
                    free_camera_last_tick_ = {};
                }
                if (cameraChanged) {
                    sync_production_controls();
                    request_redraw();
                }
                return 0;
            }
            if (wParam == 8) {
                if (IsIconic(hwnd_) || is_sizing_ ||
                    !gfx_.realtime_view_available() || !gfx_.realtime_convergence_pending()) {
                    KillTimer(hwnd_, 8);
                    return 0;
                }
                request_redraw();
                return 0;
            }
            if (wParam == 9) {
                KillTimer(hwnd_, 9);
                gfx_.set_realtime_interactive(false);
                request_redraw();
                return 0;
            }
            if (wParam == 3) {
                if (!editor_ || !editor_->get_state().is_playing) {
                    KillTimer(hwnd_, 3);
                    animation_timer_active_ = false;
                    animation_timer_interval_ = 0;
                    return 0;
                }
                editor_->advance_animation_frame();
                defer_project_dirty_ = true;
                dispatch_editor_command();
                defer_project_dirty_ = false;
                sync_production_controls();
                request_redraw();
                return 0;
            }
            if (wParam == 2) {
                KillTimer(hwnd_, 2);
                request_redraw();
                return 0;
            }
            if (wParam == 5) {
                save_recovery_snapshot();
                return 0;
            }
            if (wParam == 1) {
                if (!sequence_render_active_ && !export_current_on_complete_) {
                    KillTimer(hwnd_, 1);
                    production_timer_active_ = false;
                    return 0;
                }
                if (IsIconic(hwnd_) || is_sizing_) return 0;
                if (gfx_.production_render_complete()) {
                    KillTimer(hwnd_, 1);
                    production_timer_active_ = false;
                    on_production_frame_complete();
                    return 0;
                }
                trace_production_frame();
                InvalidateRect(hwnd_, nullptr, FALSE);
                if (gfx_.production_render_complete()) {
                    KillTimer(hwnd_, 1);
                    production_timer_active_ = false;
                    on_production_frame_complete();
                }
            }
            return 0;

        case WM_ENTERSIZEMOVE:
            is_sizing_ = true;
            if (production_timer_active_) {
                KillTimer(hwnd_, 1);
                production_timer_active_ = false;
                production_timer_paused_ = true;
            }
            return 0;

        case WM_EXITSIZEMOVE:
            is_sizing_ = false;
            if (resize_pending_) {
                resize_pending_ = false;
                apply_resize(client_width_, client_height_);
            } else if (production_timer_paused_) {
                start_production_timer();
            }
            production_timer_paused_ = false;
            request_redraw();
            return 0;

        case WM_SIZE:
            on_resize(LOWORD(lParam), HIWORD(lParam));
            return 0;

        case WM_KEYDOWN:
            if (editor_) {
                const uint32_t cameraKey = free_camera_key_mask(wParam);
                const bool rangeEditing = editor_->get_state().timeline_range_edit_field !=
                    EditorState::TimelineRangeField::None;
                if (cameraKey && !rangeEditing && !editor_->get_state().render_setup_open &&
                    (editor_->get_state().active_workspace == "Shot" ||
                     editor_->get_state().camera_pilot_active || free_look_active_)) {
                    if (editor_->get_state().camera_pilot_active &&
                        editor_->get_state().active_workspace != "Shot")
                        editor_->get_state().enter_viewport_camera();
                    free_camera_keys_ |= cameraKey;
                    free_camera_last_tick_ = std::chrono::steady_clock::now();
                    SetTimer(hwnd_, 4, 16, nullptr);
                    return 0;
                }
            }
            if (editor_ && (lParam & (1LL << 30)) == 0) {
                KeyEvent key;
                key.key_code = static_cast<uint32_t>(wParam);
                key.shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                key.ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
                key.alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
                begin_history_transaction();
                if (editor_->handle_key_down(key)) {
                    dispatch_editor_command();
                    sync_production_controls();
                    request_redraw();
                    update_project_dirty();
                    commit_history_transaction();
                    return 0;
                }
                interaction_snapshot_.clear();
                interaction_project_id_.clear();
            }
            break;

        case WM_KEYUP: {
            const uint32_t cameraKey = free_camera_key_mask(wParam);
            if (cameraKey && (free_camera_keys_ & cameraKey)) {
                free_camera_keys_ &= ~cameraKey;
                if (free_camera_keys_ == 0) {
                    free_camera_last_tick_ = {};
                    if (!free_look_active_) KillTimer(hwnd_, 4);
                    update_project_dirty();
                }
                return 0;
            }
            break;
        }

        case WM_INPUT: {
            if (free_look_active_ && raw_mouse_registered_ && editor_) {
                RAWINPUT input = {};
                UINT inputSize = sizeof(input);
                const UINT result = GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT,
                                                    &input, &inputSize, sizeof(RAWINPUTHEADER));
                if (result == sizeof(RAWINPUT) && input.header.dwType == RIM_TYPEMOUSE &&
                    (input.data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE) == 0) {
                    free_camera_mouse_dx_ += input.data.mouse.lLastX;
                    free_camera_mouse_dy_ += input.data.mouse.lLastY;
                }
            }
            return DefWindowProcW(hwnd_, msg, wParam, lParam);
        }

        case WM_KILLFOCUS:
            if (free_look_active_ && editor_) {
                MouseEvent cancelLook;
                cancelLook.button = MouseButton::Right;
                editor_->handle_mouse_up(cancelLook);
                request_redraw();
            }
            if (is_dragging_) {
                ReleaseCapture();
                is_dragging_ = false;
                commit_history_transaction();
            }
            free_look_active_ = false;
            free_camera_mouse_dx_ = free_camera_mouse_dy_ = 0;
            KillTimer(hwnd_, 4);
            if (free_camera_keys_ != 0) {
                free_camera_keys_ = 0;
                update_project_dirty();
                free_camera_last_tick_ = {};
            }
            break;

        case WM_MOUSEMOVE:
            if (free_look_active_ && raw_mouse_registered_) return 0;
            on_mouse_move(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), wParam);
            return 0;

        case WM_LBUTTONDOWN:
            on_mouse_down(MouseButton::Left, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), wParam);
            return 0;

        case WM_LBUTTONDBLCLK:
            on_mouse_down(MouseButton::Left, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), wParam);
            return 0;

        case WM_LBUTTONUP:
            on_mouse_up(MouseButton::Left, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), wParam);
            return 0;

        case WM_RBUTTONDOWN:
            on_mouse_down(MouseButton::Right, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), wParam);
            free_look_active_ = editor_ && editor_->get_state().free_camera_look_active;
            if (free_look_active_ && raw_mouse_registered_) SetTimer(hwnd_, 4, 16, nullptr);
            return 0;

        case WM_RBUTTONUP:
            on_mouse_up(MouseButton::Right, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), wParam);
            free_look_active_ = false;
            if (free_camera_keys_ == 0) {
                KillTimer(hwnd_, 4);
                free_camera_last_tick_ = {};
            }
            return 0;

        case WM_MBUTTONDOWN:
            on_mouse_down(MouseButton::Middle, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), wParam);
            return 0;

        case WM_MBUTTONUP:
            on_mouse_up(MouseButton::Middle, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), wParam);
            return 0;

        case WM_MOUSEWHEEL: {
            float delta = static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) / static_cast<float>(WHEEL_DELTA);
            on_mouse_wheel(delta, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), wParam);
            return 0;
        }

        case WM_MOUSELEAVE:
            on_mouse_leave();
            return 0;

        case WM_DESTROY:
            KillTimer(hwnd_, 1);
            KillTimer(hwnd_, 2);
            KillTimer(hwnd_, 4);
            KillTimer(hwnd_, 5);
            KillTimer(hwnd_, 6);
            KillTimer(hwnd_, 7);
            if (editor_ && editor_->get_state().project_dirty && !discard_changes_confirmed_) save_recovery_snapshot();
            else {
                const std::string recovery = recovery_file_path();
                if (!recovery.empty()) {
                    std::error_code ec;
                    std::filesystem::remove(std::filesystem::u8path(recovery), ec);
                }
            }
            recovery_timer_active_ = false;
            free_camera_keys_ = 0;
            free_camera_last_tick_ = {};
            production_timer_active_ = false;
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProcW(hwnd_, msg, wParam, lParam);
}

} // namespace medusa
