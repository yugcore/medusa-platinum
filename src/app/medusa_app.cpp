#include <medusa/medusa_app.hpp>
#include <windows.h>
#include <iostream>

namespace medusa {

MedusaApp::MedusaApp() = default;

int MedusaApp::run(int argc, char* argv[]) {
    // Enable Per-Monitor V2 DPI awareness for crystal-clear studio text rendering
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    std::string screenshot_path;
    std::string asset_path;
    bool exit_after_screenshot = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--screenshot" && i + 1 < argc) {
            screenshot_path = argv[++i];
            exit_after_screenshot = true;
        }
        if (std::string(argv[i]) == "--asset" && i + 1 < argc)
            asset_path = argv[++i];
    }

    std::wstring title = L"Medusa Platinum - 3D Cinematic Animation & Production Renderer";
    if (!window_.create(title, 1600, 900)) {
        const std::string detail = window_.get_graphics().last_init_error();
        const std::string message = detail.empty()
            ? "Medusa Platinum could not initialize its graphics system."
            : "Medusa Platinum could not initialize its graphics system.\n\n" + detail;
        MessageBoxA(nullptr, message.c_str(), "Medusa Platinum - Launch Error", MB_ICONERROR | MB_OK);
        return 1;
    }

    if (!asset_path.empty()) {
        std::string error;
        if (!window_.load_production_asset(asset_path, error)) {
            MessageBoxA(nullptr, error.c_str(), "Medusa Platinum - Asset Load Error", MB_ICONERROR | MB_OK);
            return 1;
        }
    }

    if (!screenshot_path.empty()) {
        window_.set_screenshot_target(screenshot_path);
    }

    window_.show(SW_SHOW);

    if (exit_after_screenshot) {
        window_.trace_production_frame();
        window_.render_frame();
        return 0;
    }

    // Responsive native Win32 message pump
    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return static_cast<int>(msg.wParam);
}

} // namespace medusa
