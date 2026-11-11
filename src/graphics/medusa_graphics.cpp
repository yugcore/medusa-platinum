#include <medusa/medusa_graphics.hpp>
#include <medusa/medusa_icons.hpp>
#include <medusa/medusa_theme.hpp>
#include <iostream>
#include <vector>
#include <array>
#include <wincodec.h>
#include <filesystem>
#include <map>
#include <cstring>
#include <unordered_set>
#include <cmath>
#include <limits>
#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <windowsx.h>
#include <zegfx/render/backend_factory.h>
#include <zegfx/render/shader_source.h>

namespace zegfx { extern VirtualFileSystem g_vfs; }

namespace medusa {

namespace {

constexpr wchar_t kMedusaZeGFXViewportClass[] = L"MedusaZeGFXViewport";

LRESULT CALLBACK medusa_viewport_window_proc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    const HWND parent = GetParent(window);
    if (!parent) return DefWindowProcW(window, message, wParam, lParam);
    switch (message) {
        case WM_MOUSEMOVE:
        case WM_MOUSELEAVE:
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_LBUTTONDBLCLK:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
        case WM_XBUTTONDOWN:
        case WM_XBUTTONUP: {
            if (message == WM_MOUSELEAVE) return SendMessageW(parent, message, wParam, lParam);
            POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            MapWindowPoints(window, parent, &point, 1);
            const LPARAM parentPoint = MAKELPARAM(static_cast<WORD>(point.x), static_cast<WORD>(point.y));
            return SendMessageW(parent, message, wParam, parentPoint);
        }
        case WM_MOUSEWHEEL:
        case WM_MOUSEHWHEEL:
            return SendMessageW(parent, message, wParam, lParam);
        case WM_SETFOCUS:
            SetFocus(parent);
            return 0;
        case WM_KEYDOWN:
        case WM_KEYUP:
        case WM_CHAR:
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
            return SendMessageW(parent, message, wParam, lParam);
        default:
            return DefWindowProcW(window, message, wParam, lParam);
    }
}

bool register_medusa_viewport_window_class() {
    WNDCLASSW viewportClass{};
    viewportClass.lpfnWndProc = medusa_viewport_window_proc;
    viewportClass.hInstance = GetModuleHandleW(nullptr);
    viewportClass.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    viewportClass.lpszClassName = kMedusaZeGFXViewportClass;
    return RegisterClassW(&viewportClass) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

uint8_t unit_to_byte(float value) {
    if (!std::isfinite(value)) return 0;
    return static_cast<uint8_t>(std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f);
}

zegfx::Color material_color(const float rgba[4]) {
    return {unit_to_byte(rgba[0]), unit_to_byte(rgba[1]), unit_to_byte(rgba[2]), unit_to_byte(rgba[3])};
}

zegfx::Mat4 production_transform(const zegfx::ProductionSceneNode& node) {
    zegfx::Mat4 result = zegfx::Mat4::identity();
    for (size_t row = 0; row < 4; ++row)
        for (size_t column = 0; column < 4; ++column)
            result.m[row][column] = node.worldTransform[column * 4 + row];
    return result;
}

void normalize3(float& x, float& y, float& z, float fallbackX, float fallbackY, float fallbackZ) {
    const float length = std::sqrt(x * x + y * y + z * z);
    if (length > 1.0e-6f && std::isfinite(length)) {
        x /= length;
        y /= length;
        z /= length;
    } else {
        x = fallbackX;
        y = fallbackY;
        z = fallbackZ;
    }
}

} // namespace

Graphics::Graphics() = default;

Graphics::~Graphics() {
    destroy();
}

bool Graphics::init(HWND hwnd, uint32_t width, uint32_t height) {
    last_init_error_.clear();
    hwnd_ = hwnd;
    width_ = width;
    height_ = height;

    // Detect system DPI
    UINT dpi = GetDpiForWindow(hwnd);
    dpi_scale_ = (dpi > 0) ? (static_cast<float>(dpi) / 96.0f) : 1.0f;

    const HRESULT comResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    com_initialized_ = SUCCEEDED(comResult);

    // 1. Create Direct2D & DirectWrite Factories
    D2D1_FACTORY_OPTIONS options = {};
    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
                                   __uuidof(ID2D1Factory1),
                                   &options,
                                   reinterpret_cast<void **>(d2d_factory_.GetAddressOf()));
    if (FAILED(hr)) {
        last_init_error_ = "Direct2D factory creation failed (hr=0x" +
            std::to_string(static_cast<unsigned long>(hr)) + ").";
        std::cerr << "[Graphics] " << last_init_error_ << std::endl;
        return false;
    }

    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
                             __uuidof(IDWriteFactory),
                             reinterpret_cast<IUnknown **>(dwrite_factory_.GetAddressOf()));
    if (FAILED(hr)) {
        last_init_error_ = "DirectWrite factory creation failed (hr=0x" +
            std::to_string(static_cast<unsigned long>(hr)) + ").";
        std::cerr << "[Graphics] " << last_init_error_ << std::endl;
        return false;
    }

    const auto reportFailure = [this](const char* stage, HRESULT result) {
        char message[256]{};
        std::snprintf(message, sizeof(message), "[Graphics] %s failed (hr=0x%08lx).\n",
                      stage, static_cast<unsigned long>(result));
        last_init_error_ = std::string(stage) + " failed (hr=0x";
        char code[16]{};
        std::snprintf(code, sizeof(code), "%08lx", static_cast<unsigned long>(result));
        last_init_error_ += code;
        last_init_error_ += ").";
        OutputDebugStringA(message);
        std::cerr << message;
    };
    const auto createUiSurface = [this, hwnd, width, height, &hr, &reportFailure]() {
        // Blueman keeps its Direct2D editor device independent from ZeGFX's
        // D3D12 renderer. Medusa does the same: this device owns only the
        // editor window surface, while ZeGFX owns the viewport child window.
        if (!d3d_device_) {
            const D3D_FEATURE_LEVEL hardwareLevels[] = {
                D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0,
                D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0};
            D3D_FEATURE_LEVEL featureLevel{};
            hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
                D3D11_CREATE_DEVICE_BGRA_SUPPORT, hardwareLevels,
                static_cast<UINT>(std::size(hardwareLevels)), D3D11_SDK_VERSION,
                d3d_device_.GetAddressOf(), &featureLevel, d3d_context_.GetAddressOf());
            if (FAILED(hr)) {
                const D3D_FEATURE_LEVEL warpLevels[] = {
                    D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0,
                    D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0};
                hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr,
                    D3D11_CREATE_DEVICE_BGRA_SUPPORT, warpLevels,
                    static_cast<UINT>(std::size(warpLevels)), D3D11_SDK_VERSION,
                    d3d_device_.GetAddressOf(), &featureLevel, d3d_context_.GetAddressOf());
            }
            if (FAILED(hr)) { reportFailure("Creating the native editor D3D11 device", hr); return false; }
        }

        ComPtr<IDXGIDevice> dxgiDevice;
        hr = d3d_device_.As(&dxgiDevice);
        if (FAILED(hr)) { reportFailure("Querying IDXGIDevice", hr); return false; }

        hr = d2d_factory_->CreateDevice(dxgiDevice.Get(), d2d_device_.GetAddressOf());
        if (FAILED(hr)) { reportFailure("Creating the Direct2D device", hr); return false; }
        hr = d2d_device_->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
                                               d2d_context_.GetAddressOf());
        if (FAILED(hr)) { reportFailure("Creating the Direct2D context", hr); return false; }
        d2d_context_->SetDpi(96.0f * dpi_scale_, 96.0f * dpi_scale_);

        ComPtr<IDXGIAdapter> adapter;
        hr = dxgiDevice->GetAdapter(adapter.GetAddressOf());
        if (FAILED(hr)) { reportFailure("Getting the DXGI adapter", hr); return false; }
        ComPtr<IDXGIFactory2> dxgiFactory;
        hr = adapter->GetParent(IID_PPV_ARGS(dxgiFactory.GetAddressOf()));
        if (FAILED(hr)) { reportFailure("Getting the DXGI factory", hr); return false; }

        DXGI_SWAP_CHAIN_DESC1 swapDesc{};
        swapDesc.Width = width;
        swapDesc.Height = height;
        swapDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        swapDesc.SampleDesc.Count = 1;
        swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapDesc.BufferCount = 2;
        swapDesc.Scaling = DXGI_SCALING_NONE;
        swapDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        swapDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
        hr = dxgiFactory->CreateSwapChainForHwnd(d3d_device_.Get(), hwnd, &swapDesc,
                                                  nullptr, nullptr, swapchain_.GetAddressOf());
        if (FAILED(hr)) { reportFailure("Creating the editor swap chain", hr); return false; }
        swapchain_.As(&swapchain3_);
        if (!rebuild_backbuffer_targets()) {
            if (last_init_error_.empty()) reportFailure("Creating editor backbuffer targets", E_FAIL);
            return false;
        }
        return true;
    };
    // Medusa's interactive viewport must initialize through ZeGFX's existing
    // real-time backend. Do not continue with a UI-only or alternate renderer.
    if (!initialize_realtime_backend(hwnd, width, height)) return false;
    if (!createUiSurface()) return false;

    // Keep ProductionDCC for offline frame and sequence output. Interactive
    // viewport rendering is handled by Renderer above.
#if !defined(MEDUSA_UI_ONLY_BUILD)
    zegfx::ProductionPipelineConfig production_config;
    production_config.progressiveViewport = false;
    production_pipeline_ = zegfx::createProductionPipeline(
        production_config, nullptr);
    if (!production_pipeline_) {
        last_init_error_ = "ZeGFX ProductionDCC initialization failed.";
        std::cerr << "[Graphics] " << last_init_error_ << std::endl;
        return false;
    }
#endif
    if (realtime_renderer_ && realtime_viewport_width_ > 0 && realtime_viewport_height_ > 0)
        create_realtime_target(realtime_viewport_width_, realtime_viewport_height_);
    return true;
}

bool Graphics::initialize_realtime_backend(HWND hwnd, uint32_t width, uint32_t height) {
#if defined(MEDUSA_UI_ONLY_BUILD)
    (void)hwnd;
    (void)width;
    (void)height;
    return false;
#else
    const auto fail = [this](const std::string& reason) {
        last_init_error_ = reason;
        if (!reason.empty()) std::cerr << "[Graphics] ZeGFX realtime viewport initialization failed: " << reason << std::endl;
        destroy_realtime_postprocess();
        if (realtime_renderer_) realtime_renderer_->shutdown();
        realtime_renderer_.reset();
        if (realtime_host_window_) {
            DestroyWindow(realtime_host_window_);
            realtime_host_window_ = nullptr;
        }
        return false;
    };

    std::filesystem::path executablePath;
    wchar_t executable[32768]{};
    const DWORD executableLength = GetModuleFileNameW(nullptr, executable,
                                                       static_cast<DWORD>(std::size(executable)));
    if (executableLength > 0 && executableLength < std::size(executable))
        executablePath = std::filesystem::path(executable).parent_path();
    std::error_code pathError;
    const auto workingDirectory = std::filesystem::current_path(pathError);
    std::vector<std::filesystem::path> shaderCandidates = {
        executablePath / L"resources" / L"ZeGFX" / L"shaders",
        executablePath / L"ZeGFX" / L"shaders",
        executablePath.parent_path() / L"ZeGFX" / L"shaders",
        workingDirectory / L"ZeGFX" / L"shaders",
        workingDirectory.parent_path() / L"ZeGFX" / L"shaders"};
    // Also support a development checkout where MedusaPlatinum and ZeGFX are
    // sibling folders. Resolve from the EXE location so Explorer shortcuts do
    // not depend on their current working directory.
    for (auto ancestor = executablePath; !ancestor.empty(); ancestor = ancestor.parent_path()) {
        const auto siblingShaders = ancestor.parent_path() / L"ZeGFX" / L"shaders";
        if (siblingShaders != shaderCandidates.back()) shaderCandidates.push_back(siblingShaders);
        if (ancestor == ancestor.parent_path()) break;
    }
    std::filesystem::path shaderRoot;
    for (const auto& candidate : shaderCandidates) {
        std::error_code error;
        if (std::filesystem::is_regular_file(candidate / L"dx12" / L"materials" / L"basic_3d.hlsl", error)) {
            shaderRoot = std::filesystem::absolute(candidate, error);
            if (!error) break;
            shaderRoot.clear();
        }
    }
    if (shaderRoot.empty()) return fail("the ZeGFX shader source folder was not found");
    const std::string shaderPath = shaderRoot.u8string();
    _putenv_s("ZEGFX_SHADER_ROOT", shaderPath.c_str());
    zegfx::g_vfs.registerMount("/ZeGFX", shaderPath);
    zegfx::g_vfs.registerMount("/Project", shaderPath);
    zegfx::g_vfs.registerMount("/Generated", (shaderRoot / L"generated").u8string());

    if (!hwnd || !register_medusa_viewport_window_class())
        return fail("could not register the Medusa ZeGFX viewport window class");
    const int viewportWidth = static_cast<int>(std::max(1u, width));
    const int viewportHeight = static_cast<int>(std::max(1u, height));
    realtime_host_window_ = CreateWindowExW(0, kMedusaZeGFXViewportClass, L"",
        WS_CHILD | WS_CLIPSIBLINGS, 0, 0, viewportWidth, viewportHeight,
        hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!realtime_host_window_) return fail("could not create the ZeGFX viewport child window");

    // Match Blueman's renderer-owned child-window swapchain. Keep the quality
    // preset low for interactive DCC use; ProductionDCC owns final output.
    const int hostWidth = std::max(320, viewportWidth);
    const int hostHeight = std::max(180, viewportHeight);
    zegfx::RendererSettings rendererSettings =
        zegfx::RendererSettings::fromPreset(zegfx::QualityPreset::Low);
    rendererSettings.outputWidth = hostWidth;
    rendererSettings.outputHeight = hostHeight;
    rendererSettings.vsync = false;
    rendererSettings.experimentalPipelinePlans = false;
    realtime_renderer_ = std::make_unique<zegfx::Renderer>(rendererSettings);
    auto backend = std::unique_ptr<zegfx::GraphicsBackend>(
        zegfx::createGraphicsBackend(zegfx::BackendKind::DirectX12));
    if (!backend) return fail("Direct3D 12 backend creation failed");
    std::string error;
    if (!realtime_renderer_->initializeWithBackend(std::move(backend), realtime_host_window_, error))
        return fail(error);
    auto* nativeBackend = realtime_renderer_->backend();
    if (!nativeBackend || !nativeBackend->getD3D12SwapChain())
        return fail("ZeGFX could not attach its swapchain to the viewport child window");

    realtime_camera_ = nativeBackend->createCamera3D(0.0f, 2.0f, -5.0f, 0.0f, 1.0f, 0.0f);
    if (!realtime_camera_.valid()) return fail("ZeGFX could not create the viewport camera");
    nativeBackend->setCamera3DFov(realtime_camera_, 38.0f);
    nativeBackend->setCamera3DClipPlanes(realtime_camera_, 0.01f, 10000.0f);
    nativeBackend->setTargetFPS(60);
    mark_realtime_history_dirty();
    return true;
#endif
}

bool Graphics::rebuild_backbuffer_targets() {
    if (!swapchain_ || !d2d_context_) {
        last_init_error_ = "Editor backbuffer setup is missing its DXGI swapchain or Direct2D context.";
        return false;
    }
    d2d_context_->SetTarget(nullptr);
    target_bitmap_.Reset();
    for (auto& bitmap : backbuffer_bitmaps_) bitmap.Reset();
    for (auto& resource : wrapped_backbuffers_) resource.Reset();
    current_backbuffer_index_ = 0;

    D2D1_BITMAP_PROPERTIES1 bitmapProps = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        96.0f * dpi_scale_, 96.0f * dpi_scale_);

    // Direct2D writes the acquired backbuffer as a render target; releasing it
    // returns it to PRESENT for the swap chain.
    // Match Blueman's native D2D surface: keep one target bitmap bound to
    // backbuffer zero and let the flip-sequential swapchain preserve it.
    constexpr UINT index = 0;
    {
        ComPtr<IDXGISurface> surface;
        HRESULT hr = S_OK;
        hr = swapchain_->GetBuffer(index, IID_PPV_ARGS(surface.GetAddressOf()));
        if (FAILED(hr)) {
            last_init_error_ = "Editor swapchain backbuffer " + std::to_string(index) +
                " does not expose IDXGISurface (hr=0x" +
                std::to_string(static_cast<unsigned long>(hr)) + ").";
            std::cerr << "[Graphics] Swap-chain buffer " << index
                      << " does not expose IDXGISurface (hr=0x" << std::hex
                      << static_cast<unsigned long>(hr) << std::dec << ")." << std::endl;
            return false;
        }
        hr = d2d_context_->CreateBitmapFromDxgiSurface(surface.Get(), &bitmapProps,
                                                        backbuffer_bitmaps_[index].GetAddressOf());
        if (FAILED(hr)) {
            DXGI_SURFACE_DESC surfaceDesc{};
            const HRESULT descResult = surface->GetDesc(&surfaceDesc);
            last_init_error_ = "Creating the Direct2D target for editor backbuffer " +
                std::to_string(index) + " failed (hr=0x" +
                std::to_string(static_cast<unsigned long>(hr)) + ", format=" +
                std::to_string(SUCCEEDED(descResult) ? surfaceDesc.Format : DXGI_FORMAT_UNKNOWN) +").";
            std::cerr << "[Graphics] Creating Direct2D bitmap for swap-chain buffer " << index
                      << " failed (hr=0x" << std::hex << static_cast<unsigned long>(hr)
                      << std::dec << ", size=" << (SUCCEEDED(descResult) ? surfaceDesc.Width : 0)
                      << "x" << (SUCCEEDED(descResult) ? surfaceDesc.Height : 0)
                      << ", format=" << (SUCCEEDED(descResult) ? surfaceDesc.Format : DXGI_FORMAT_UNKNOWN)
                      << ")." << std::endl;
            return false;
        }
    }
    current_backbuffer_index_ = 0;
    d2d_context_->SetTarget(backbuffer_bitmaps_[0].Get());
    return true;
}

bool Graphics::recreate_d2d_device_targets() {
    if (!d2d_factory_ || !d3d_device_ || !swapchain_) {
        std::cerr << "[Graphics] Direct2D recovery is missing its factory, D3D device, or swap chain."
                  << std::endl;
        return false;
    }

    if (d2d_context_) d2d_context_->SetTarget(nullptr);
    target_bitmap_.Reset();
    realtime_preview_bitmap_.Reset();
    app_logo_.Reset();
    app_logo_loaded_ = false;
    for (auto& bitmap : backbuffer_bitmaps_) bitmap.Reset();
    for (auto& resource : wrapped_backbuffers_) resource.Reset();
    brush_cache_.clear();
    d2d_context_.Reset();
    d2d_device_.Reset();

    ComPtr<IDXGIDevice> dxgiDevice;
    HRESULT hr = d3d_device_.As(&dxgiDevice);
    if (FAILED(hr)) {
        std::cerr << "[Graphics] Direct2D recovery could not query IDXGIDevice (hr=0x"
                  << std::hex << hr << std::dec << ")." << std::endl;
        return false;
    }
    hr = d2d_factory_->CreateDevice(dxgiDevice.Get(), d2d_device_.GetAddressOf());
    if (FAILED(hr)) {
        std::cerr << "[Graphics] Direct2D recovery could not recreate the device (hr=0x"
                  << std::hex << hr << std::dec << ")." << std::endl;
        return false;
    }
    hr = d2d_device_->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
                                          d2d_context_.GetAddressOf());
    if (FAILED(hr)) {
        std::cerr << "[Graphics] Direct2D recovery could not recreate the context (hr=0x"
                  << std::hex << hr << std::dec << ")." << std::endl;
        return false;
    }
    d2d_context_->SetDpi(96.0f * dpi_scale_, 96.0f * dpi_scale_);

    if (!rebuild_backbuffer_targets()) {
        std::cerr << "[Graphics] Direct2D recovery could not rebuild swap-chain targets."
                  << std::endl;
        return false;
    }

    // The ZeGFX render target remains valid. Rebind its Direct2D sampling
    // bitmap to the new context so the viewport can resume without recreating
    // the renderer or the Production Pipeline.
    if (realtime_wrapped_target_) {
        ComPtr<IDXGISurface> surface;
        hr = realtime_wrapped_target_.As(&surface);
        if (FAILED(hr)) {
            std::cerr << "[Graphics] Direct2D recovery could not query the ZeGFX viewport surface (hr=0x"
                      << std::hex << hr << std::dec << ")." << std::endl;
            return false;
        }
        const auto bitmapProps = D2D1::BitmapProperties1(
            D2D1_BITMAP_OPTIONS_NONE,
            D2D1::PixelFormat(DXGI_FORMAT_R8G8B8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
            96.0f * dpi_scale_, 96.0f * dpi_scale_);
        hr = d2d_context_->CreateBitmapFromDxgiSurface(surface.Get(), &bitmapProps,
                                                        realtime_preview_bitmap_.GetAddressOf());
        if (FAILED(hr)) {
            std::cerr << "[Graphics] Direct2D recovery could not rebuild the ZeGFX preview bitmap (hr=0x"
                      << std::hex << hr << std::dec << ")." << std::endl;
            return false;
        }
    }
    mark_realtime_history_dirty();
    return true;
}

bool Graphics::create_realtime_target(uint32_t width, uint32_t height) {
    if (!realtime_renderer_ || width == 0 || height == 0) return false;
    auto* backend = realtime_renderer_->backend();
    if (backend) backend->waitIdle();
    realtime_renderer_->resize(width, height);
    if (backend) {
        if (realtime_hdr_pipeline_)
            backend->hdrPipelineResize(realtime_hdr_pipeline_, static_cast<int>(width), static_cast<int>(height));
        if (realtime_depth_normal_)
            backend->depthNormalResize(realtime_depth_normal_, static_cast<int>(width), static_cast<int>(height));
        if (realtime_ao_) backend->aoDestroy(realtime_ao_);
        realtime_ao_ = backend->aoCreate("Medusa viewport ambient occlusion",
            static_cast<int>(width), static_cast<int>(height), "low");
        if (realtime_ao_) {
            backend->aoSettings(realtime_ao_, 1.0f, 0.45f, 0.025f, 1.25f, 0.10f);
            backend->aoTemporal(realtime_ao_, false, 0.90f);
        }
        mark_realtime_history_dirty();
    }
    realtime_render_width_ = width;
    realtime_render_height_ = height;
    return realtime_renderer_->ready();
}

bool Graphics::resize(uint32_t width, uint32_t height) {
    if (!swapchain_ || width == 0 || height == 0) return false;
    const bool targetsReady = backbuffer_bitmaps_[0] != nullptr;
    if (width_ == width && height_ == height && targetsReady) return true;

    if (d2d_context_) d2d_context_->SetTarget(nullptr);
    target_bitmap_.Reset();
    if (d3d_context_) d3d_context_->Flush();
    for (auto& bitmap : backbuffer_bitmaps_) bitmap.Reset();
    for (auto& resource : wrapped_backbuffers_) resource.Reset();

    HRESULT hr = swapchain_->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(hr)) {
        std::cerr << "[Graphics] Editor swap-chain resize failed (hr=0x" << std::hex
                  << static_cast<unsigned long>(hr) << std::dec
                  << "); restoring the current Direct2D backbuffers." << std::endl;
        swapchain_.As(&swapchain3_);
        if (!rebuild_backbuffer_targets())
            std::cerr << "[Graphics] Could not restore Direct2D targets after resize failure." << std::endl;
        return false;
    }
    width_ = width;
    height_ = height;
    swapchain_.As(&swapchain3_);
    if (!rebuild_backbuffer_targets()) {
        std::cerr << "[Graphics] Could not rebuild Direct2D targets after editor swap-chain resize." << std::endl;
        return false;
    }
    return true;
}

bool Graphics::resize_realtime_viewport(uint32_t width, uint32_t height) {
    if (width == 0 || height == 0) return false;
    const uint32_t renderWidth = realtime_interactive_
        ? std::max(1u, (width * 3u + 3u) / 4u) : width;
    const uint32_t renderHeight = realtime_interactive_
        ? std::max(1u, (height * 3u + 3u) / 4u) : height;
    if (realtime_viewport_width_ == width && realtime_viewport_height_ == height &&
        realtime_render_width_ == renderWidth && realtime_render_height_ == renderHeight &&
        realtime_renderer_) return true;
    realtime_viewport_width_ = width;
    realtime_viewport_height_ = height;
    if (realtime_renderer_ && !create_realtime_target(renderWidth, renderHeight)) {
        std::cerr << "[Graphics] Could not resize the ZeGFX realtime viewport target." << std::endl;
        return false;
    }
    return realtime_renderer_ != nullptr;
}

void Graphics::set_realtime_interactive(bool active) {
    if (realtime_interactive_ == active) return;
    realtime_interactive_ = active;
    if (realtime_viewport_width_ > 0 && realtime_viewport_height_ > 0)
        resize_realtime_viewport(realtime_viewport_width_, realtime_viewport_height_);
}

bool Graphics::resize_production_output(uint32_t width, uint32_t height) {
    if (!production_pipeline_ || width == 0 || height == 0) return false;
    // Offline outputs use the explicit project resolution instead of the
    // lower pixel budget used to keep the interactive viewport responsive.
    constexpr uint32_t maxWidth = 1920;
    constexpr uint32_t maxHeight = 1080;
    width = std::clamp(width, 320u, maxWidth);
    height = std::clamp(height, 180u, maxHeight);
    production_pipeline_->resize(width, height);
    return true;
}

void Graphics::destroy() {
    destroy_realtime_scene();
    destroy_realtime_postprocess();
    if (realtime_renderer_) {
        realtime_renderer_->setExternalRenderTargetCPU(0);
        if (auto* backend = realtime_renderer_->backend()) {
            if (realtime_camera_.valid()) backend->destroyCamera3D(realtime_camera_);
            realtime_camera_ = {};
            backend->waitIdle();
        }
    }
    IconRenderer::clear_cache();
    brush_cache_.clear();
    font_cache_.clear();
    app_logo_.Reset();
    app_logo_loaded_ = false;
    target_bitmap_.Reset();
    realtime_preview_bitmap_.Reset();
    if (d2d_context_) d2d_context_->SetTarget(nullptr);
    if (d3d_context_) d3d_context_->Flush();
    realtime_wrapped_target_.Reset();
    realtime_target_.Reset();
    realtime_rtv_heap_.Reset();
    for (auto& bitmap : backbuffer_bitmaps_) bitmap.Reset();
    for (auto& resource : wrapped_backbuffers_) resource.Reset();
    realtime_convergence_frames_remaining_ = 0;
    d2d_context_.Reset();
    d2d_device_.Reset();
    swapchain_.Reset();
    swapchain3_.Reset();
    d3d_context_.Reset();
    d3d_device_.Reset();
    d3d11on12_device_.Reset();
    production_pipeline_.reset();
    realtime_renderer_.reset();
    d3d12_queue_.Reset();
    d3d12_device_.Reset();
    if (realtime_host_window_) {
        DestroyWindow(realtime_host_window_);
        realtime_host_window_ = nullptr;
    }
    dwrite_factory_.Reset();
    d2d_factory_.Reset();
    if (com_initialized_) {
        CoUninitialize();
        com_initialized_ = false;
    }
}

void Graphics::destroy_realtime_postprocess() {
    auto* backend = realtime_renderer_ ? realtime_renderer_->backend() : nullptr;
    if (backend) {
        backend->waitIdle();
        if (realtime_ao_) backend->aoDestroy(realtime_ao_);
        if (realtime_depth_normal_) backend->depthNormalDestroy(realtime_depth_normal_);
        if (realtime_bloom_) backend->bloomDestroy(realtime_bloom_);
        if (realtime_tone_map_) backend->toneMapDestroy(realtime_tone_map_);
        if (realtime_color_grade_) backend->colorGradeDestroy(realtime_color_grade_);
        if (realtime_hdr_pipeline_) {
            backend->hdrPipelineEnd(realtime_hdr_pipeline_);
            backend->hdrPipelineDestroy(realtime_hdr_pipeline_);
        }
    }
    realtime_ao_ = nullptr;
    realtime_depth_normal_ = nullptr;
    realtime_bloom_ = nullptr;
    realtime_tone_map_ = nullptr;
    realtime_color_grade_ = nullptr;
    realtime_hdr_pipeline_ = nullptr;
}

void Graphics::draw_app_logo(const Rect& bounds) {
    if (!d2d_context_ || !d2d_frame_active_ || bounds.width <= 0.0f || bounds.height <= 0.0f) return;
    if (!app_logo_loaded_) {
        app_logo_loaded_ = true;
        wchar_t executablePath[32768] = {};
        const DWORD length = GetModuleFileNameW(nullptr, executablePath,
                                                static_cast<DWORD>(std::size(executablePath)));
        if (length > 0 && length < std::size(executablePath)) {
            const std::filesystem::path logoPath =
                std::filesystem::path(executablePath).parent_path() / L"resources" / L"logo" / L"logo.png";
            ComPtr<IWICImagingFactory> wicFactory;
            HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                          IID_PPV_ARGS(wicFactory.GetAddressOf()));
            ComPtr<IWICBitmapDecoder> decoder;
            if (SUCCEEDED(hr)) hr = wicFactory->CreateDecoderFromFilename(
                logoPath.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad,
                decoder.GetAddressOf());
            ComPtr<IWICBitmapFrameDecode> frame;
            if (SUCCEEDED(hr)) hr = decoder->GetFrame(0, frame.GetAddressOf());
            ComPtr<IWICFormatConverter> converter;
            if (SUCCEEDED(hr)) hr = wicFactory->CreateFormatConverter(converter.GetAddressOf());
            if (SUCCEEDED(hr)) hr = converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppPBGRA,
                WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
            if (SUCCEEDED(hr)) hr = d2d_context_->CreateBitmapFromWicBitmap(
                converter.Get(), nullptr, app_logo_.GetAddressOf());
            if (FAILED(hr)) app_logo_.Reset();
        }
    }
    if (app_logo_) {
        // The provided mark has generous transparent padding. Zoom within its
        // viewport and clip so the visible artwork fills the brand tile without
        // changing the source image or distorting its square proportions.
        constexpr float kLogoContentZoom = 1.50f;
        const float drawWidth = bounds.width * kLogoContentZoom;
        const float drawHeight = bounds.height * kLogoContentZoom;
        const float drawX = bounds.x + (bounds.width - drawWidth) * 0.5f;
        const float drawY = bounds.y + (bounds.height - drawHeight) * 0.5f;
        d2d_context_->PushAxisAlignedClip(
            D2D1::RectF(bounds.x, bounds.y, bounds.right(), bounds.bottom()),
            D2D1_ANTIALIAS_MODE_ALIASED);
        d2d_context_->DrawBitmap(app_logo_.Get(),
            D2D1::RectF(drawX, drawY, drawX + drawWidth, drawY + drawHeight), 1.0f,
            D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
        d2d_context_->PopAxisAlignedClip();
    }
}

void Graphics::begin_draw() {
    if (!d2d_context_ || d2d_frame_active_) return;
    if (swapchain3_) {
        current_backbuffer_index_ = 0;
        if (!backbuffer_bitmaps_[current_backbuffer_index_]) {
            if (!rebuild_backbuffer_targets()) {
                std::cerr << "[Graphics] Direct2D backbuffer target is unavailable; skipping this UI frame."
                          << std::endl;
                return;
            }
            current_backbuffer_index_ = 0;
        }
        d2d_context_->SetTarget(backbuffer_bitmaps_[current_backbuffer_index_].Get());
    }
    ComPtr<ID2D1Image> target;
    d2d_context_->GetTarget(target.GetAddressOf());
    if (!target) {
        std::cerr << "[Graphics] Direct2D has no active target; skipping this UI frame." << std::endl;
        return;
    }
    d2d_context_->BeginDraw();
    d2d_frame_active_ = true;
}

void Graphics::end_draw() {
    bool recreateTargets = false;
    if (d2d_context_ && d2d_frame_active_) {
        const HRESULT hr = d2d_context_->EndDraw();
        d2d_frame_active_ = false;
        if (FAILED(hr)) {
            std::cerr << "[Graphics] EndDraw failed! hr=0x" << std::hex << hr << std::dec << std::endl;
            recreateTargets = hr == D2DERR_RECREATE_TARGET;
        }
    }
    if (recreateTargets) {
        if (!recreate_d2d_device_targets()) {
            std::cerr << "[Graphics] Could not recreate Direct2D device targets after target loss."
                      << std::endl;
        } else if (hwnd_) {
            InvalidateRect(hwnd_, nullptr, FALSE);
        }
    }
}

void Graphics::present() {
    if (swapchain_) {
        swapchain_->Present(1, 0); // VSync enabled
    }
}

void Graphics::mark_realtime_history_dirty() {
    // The direct viewport path is fully rasterized on each frame and has no
    // temporal history to converge. Avoid scheduling redundant redraws.
    realtime_convergence_frames_remaining_ = 0;
    realtime_render_dirty_ = true;
    if (realtime_renderer_) {
        if (auto* backend = realtime_renderer_->backend()) backend->resetTaaHistory();
    }
}

void Graphics::set_viewport_shading_mode(ViewportShadingMode mode) {
    if (viewport_shading_mode_ == mode) return;
    viewport_shading_mode_ = mode;
    mark_realtime_history_dirty();
}

void Graphics::set_realtime_grid_enabled(bool enabled) {
    if (realtime_grid_enabled_ == enabled) return;
    realtime_grid_enabled_ = enabled;
    mark_realtime_history_dirty();
}

void Graphics::set_realtime_modal_overlay(bool enabled) {
    if (realtime_modal_overlay_active_ == enabled) return;
    realtime_modal_overlay_active_ = enabled;
    mark_realtime_history_dirty();
}

bool Graphics::set_realtime_view(const zegfx::ProductionViewSettings& view) {
    bool changed = view.verticalFovDegrees != realtime_view_.verticalFovDegrees;
    for (int i = 0; i < 3; ++i) {
        changed = changed || view.cameraPosition[i] != realtime_view_.cameraPosition[i] ||
            view.cameraTarget[i] != realtime_view_.cameraTarget[i] || view.cameraUp[i] != realtime_view_.cameraUp[i] ||
            view.keyLightPosition[i] != realtime_view_.keyLightPosition[i] ||
            view.keyLightColor[i] != realtime_view_.keyLightColor[i];
    }
    realtime_view_ = view;
    if (changed) mark_realtime_history_dirty();
    return changed;
}

bool Graphics::render_realtime_view() {
    if (!realtime_renderer_ || !realtime_view_available_) return false;
    if (!realtime_render_dirty_) return true;
    auto* backend = realtime_renderer_->backend();
    if (!backend || !realtime_camera_.valid()) return false;

    backend->setCamera3DPosition(realtime_camera_, realtime_view_.cameraPosition[0],
        realtime_view_.cameraPosition[1], realtime_view_.cameraPosition[2]);
    backend->setCamera3DTarget(realtime_camera_, realtime_view_.cameraTarget[0],
        realtime_view_.cameraTarget[1], realtime_view_.cameraTarget[2]);
    backend->setCamera3DFov(realtime_camera_, realtime_view_.verticalFovDegrees);

    const zegfx::Color ambient = viewport_shading_mode_ == ViewportShadingMode::Solid
        ? zegfx::Color(108, 111, 116, 255) : zegfx::Color(38, 42, 47, 255);
    auto& localLights = realtime_local_lights_;
    localLights.clear();
    if (localLights.capacity() < realtime_nodes_.size()) localLights.reserve(realtime_nodes_.size());
    bool hasSceneLight = false;
    bool hasDirectional = false;
    float sunDirectionX = -0.45f, sunDirectionY = -0.82f, sunDirectionZ = 0.36f;
    zegfx::Color sunColor(255, 255, 255, 255);
    float sunIntensity = 0.0f;
    for (const auto& node : realtime_nodes_) {
        if (node.type != zegfx::ProductionNodeType::Light || !node.visible || !node.renderable ||
            node.lightEnergy <= 0.0f) continue;
        hasSceneLight = true;
        const float px = node.worldTransform[12];
        const float py = node.worldTransform[13];
        const float pz = node.worldTransform[14];
        float dx = -node.worldTransform[8], dy = -node.worldTransform[9], dz = -node.worldTransform[10];
        normalize3(dx, dy, dz, 0.0f, -1.0f, 0.0f);
        const zegfx::Color color(unit_to_byte(node.lightColor[0]), unit_to_byte(node.lightColor[1]),
                                 unit_to_byte(node.lightColor[2]), 255);
        if (node.areaLight || node.lightType != zegfx::LightType::Directional) {
            zegfx::LightRenderCommand light{};
            light.type = node.lightType == zegfx::LightType::Spot
                ? zegfx::LightType::Spot : zegfx::LightType::Point;
            light.x = px; light.y = py; light.z = pz;
            light.directionX = dx; light.directionY = dy; light.directionZ = dz;
            light.angle = std::clamp(node.lightSpotAngleDegrees, 1.0f, 179.0f);
            light.radius = 35.0f;
            light.intensity = node.lightEnergy;
            light.color = color;
            light.enabled = true;
            light.castsShadows = true;
            localLights.push_back(light);
        } else if (!hasDirectional) {
            hasDirectional = true;
            sunDirectionX = dx; sunDirectionY = dy; sunDirectionZ = dz;
            sunColor = color;
            sunIntensity = node.lightEnergy;
        }
    }
    if (!hasSceneLight && viewport_shading_mode_ == ViewportShadingMode::Rendered) {
        const float keyStrength = std::max({realtime_view_.keyLightColor[0],
            realtime_view_.keyLightColor[1], realtime_view_.keyLightColor[2], 0.0f});
        if (keyStrength > 0.0f) {
            sunIntensity = keyStrength;
            sunColor = zegfx::Color(
                unit_to_byte(realtime_view_.keyLightColor[0] / keyStrength),
                unit_to_byte(realtime_view_.keyLightColor[1] / keyStrength),
                unit_to_byte(realtime_view_.keyLightColor[2] / keyStrength), 255);
            sunDirectionX = -realtime_view_.keyLightPosition[0];
            sunDirectionY = -realtime_view_.keyLightPosition[1];
            sunDirectionZ = -realtime_view_.keyLightPosition[2];
            normalize3(sunDirectionX, sunDirectionY, sunDirectionZ, -0.45f, -0.82f, 0.36f);
            hasDirectional = true;
        }
    }
    // Blender-style solid shading uses a viewport-only studio key so geometry
    // stays legible without adding a light object to the scene or final render.
    if (!hasSceneLight && viewport_shading_mode_ == ViewportShadingMode::Solid) {
        hasDirectional = true;
        sunColor = zegfx::Color(235, 237, 240, 255);
        sunIntensity = 0.85f;
    }
    backend->setRenderer3DLighting(ambient, hasDirectional ? sunColor : zegfx::Color(0, 0, 0, 255),
        sunDirectionX, sunDirectionY, sunDirectionZ, hasDirectional ? sunIntensity : 0.0f,
        1.0f, 2.2f, 0.08f);
    backend->setRendererV2Lights(localLights, ambient);
    backend->setDirectionalShadowParams(hasDirectional && viewport_shading_mode_ == ViewportShadingMode::Rendered,
        1024.0f, 100.0f,
        0.0002f, 0.001f, 0.1f, 1.0f, 1.0f, 0.85f,
        sunDirectionX, sunDirectionY, sunDirectionZ);

    realtime_renderer_->beginFrame();
    realtime_renderer_->clear(zegfx::Color(29, 31, 35, 0));
    if (viewport_shading_mode_ == ViewportShadingMode::Rendered && realtime_ao_ && realtime_depth_normal_) {
        backend->depthNormalCapture(realtime_depth_normal_);
        backend->aoDispatch(realtime_ao_, realtime_depth_normal_);
    }
    if (realtime_renderer_->beginScene(realtime_camera_)) {
        if (realtime_grid_enabled_) {
            const float dx = realtime_view_.cameraTarget[0] - realtime_view_.cameraPosition[0];
            const float dy = realtime_view_.cameraTarget[1] - realtime_view_.cameraPosition[1];
            const float dz = realtime_view_.cameraTarget[2] - realtime_view_.cameraPosition[2];
            const float viewDistance = std::max(0.1f, std::sqrt(dx * dx + dy * dy + dz * dz));
            const float viewportHeight = static_cast<float>(std::max(1u, realtime_viewport_height_));
            const float viewHeight = 2.0f * viewDistance * std::tan(
                std::clamp(realtime_view_.verticalFovDegrees, 1.0f, 170.0f) * 0.00872664626f);
            const float requestedStep = std::max(1.0e-4f, viewHeight / viewportHeight * 52.0f);
            const float decade = std::pow(10.0f, std::floor(std::log10(requestedStep)));
            const float normalizedStep = requestedStep / decade;
            const float gridStep = decade * (normalizedStep < 1.5f ? 1.0f :
                                             normalizedStep < 3.5f ? 2.0f : 5.0f);
            constexpr int lineCount = 48;
            const float extent = static_cast<float>(lineCount) * gridStep;
            const float centerX = std::floor(realtime_view_.cameraPosition[0] / gridStep) * gridStep;
            const float centerZ = std::floor(realtime_view_.cameraPosition[2] / gridStep) * gridStep;
            for (int i = -lineCount; i <= lineCount; ++i) {
                const float offset = static_cast<float>(i);
                const float fade = 1.0f - std::abs(offset) / static_cast<float>(lineCount + 1);
                const bool major = (std::abs(i) % 5) == 0;
                const uint8_t alpha = static_cast<uint8_t>((major ? 150.0f : 82.0f) * fade);
                const zegfx::Color color = major
                    ? zegfx::Color(70, 74, 80, alpha)
                    : zegfx::Color(48, 52, 58, alpha);
                const float x = centerX + offset * gridStep;
                const float z = centerZ + offset * gridStep;
                backend->drawLine3D(x, 0.0f, centerZ - extent, x, 0.0f, centerZ + extent, color);
                backend->drawLine3D(centerX - extent, 0.0f, z, centerX + extent, 0.0f, z, color);
            }
            // Keep the world axes fixed at the scene origin while the finer grid
            // follows the camera in snapped increments to feel effectively infinite.
            backend->drawLine3D(-extent, 0.001f, 0.0f, extent, 0.001f, 0.0f,
                                zegfx::Color(180, 83, 83, 210));
            backend->drawLine3D(0.0f, 0.001f, -extent, 0.0f, 0.001f, extent,
                                zegfx::Color(83, 135, 194, 210));
        }
        for (const auto& item : realtime_draw_items_) {
            if (!item.mesh.valid() || item.node_index >= realtime_nodes_.size()) continue;
            const auto& node = realtime_nodes_[item.node_index];
            if (!node.visible || !node.renderable || node.type != zegfx::ProductionNodeType::Mesh) continue;
            if (viewport_shading_mode_ != ViewportShadingMode::Wireframe) {
                realtime_renderer_->drawMesh(item.mesh, item.material, production_transform(node));
                continue;
            }
            backend->pushTransform3D();
            backend->setTransform3D(production_transform(node));
            for (const auto& edge : item.wire_edges) {
                backend->drawLine3D(edge.start[0], edge.start[1], edge.start[2],
                                    edge.end[0], edge.end[1], edge.end[2],
                                    zegfx::Color(190, 194, 202, 225));
            }
            backend->popTransform3D();
        }
    }
    if (realtime_modal_overlay_active_ && realtime_viewport_width_ > 0 && realtime_viewport_height_ > 0) {
        // Match the dimmed backdrop Medusa draws for modal dialogs while keeping
        // the native ZeGFX viewport behind the dialog's own UI surface.
        backend->drawRect(0.0f, 0.0f, static_cast<float>(realtime_viewport_width_),
                          static_cast<float>(realtime_viewport_height_),
                          zegfx::Color(0, 0, 0, 145));
    }
    realtime_renderer_->endFrame();
    if (realtime_convergence_frames_remaining_ > 0) --realtime_convergence_frames_remaining_;
    realtime_render_dirty_ = false;
    return true;
}

bool Graphics::submit_production_scene(zegfx::ProductionScene scene, std::string& error) {
    if (!production_pipeline_) { error = "The ZeGFX production pipeline is not initialized."; return false; }
    std::string realtimeError;
    if (realtime_renderer_ && !build_realtime_scene(scene, realtimeError)) {
        std::cerr << "[Graphics] ZeGFX realtime viewport scene upload failed: "
                  << realtimeError << std::endl;
    }
    production_scene_submitted_ = production_pipeline_->setScene(std::move(scene), error);
    return production_scene_submitted_;
}

bool Graphics::build_realtime_scene(const zegfx::ProductionScene& scene, std::string& error) {
    error.clear();
    if (!realtime_renderer_ || !realtime_renderer_->ready() || !realtime_renderer_->backend()) {
        error = "ZeGFX realtime Renderer is not ready.";
        return false;
    }

    destroy_realtime_scene();
    realtime_nodes_ = scene.nodes;
    if (realtime_nodes_.empty()) {
        zegfx::ProductionSceneNode root;
        root.name = scene.sourceName.empty() ? "Scene" : scene.sourceName;
        realtime_nodes_.push_back(root);
    }

    const size_t materialCount = std::max<size_t>(1, scene.materials.size());
    realtime_materials_.reserve(materialCount);
    for (size_t materialIndex = 0; materialIndex < materialCount; ++materialIndex) {
        const auto material = realtime_renderer_->createMaterial(
            materialIndex < scene.materials.size() ? "Medusa material " + std::to_string(materialIndex)
                                                    : "Medusa default material");
        if (!material.valid()) {
            error = "ZeGFX could not allocate a scene material.";
            destroy_realtime_scene();
            return false;
        }
        realtime_materials_.push_back(material);
        if (materialIndex >= scene.materials.size()) continue;
        const auto& source = scene.materials[materialIndex];
        realtime_renderer_->setMaterialColor(material, "baseColor", material_color(source.baseColor));
        realtime_renderer_->setMaterialFloat(material, "metallic", std::clamp(source.metallic, 0.0f, 1.0f));
        realtime_renderer_->setMaterialFloat(material, "roughness", std::clamp(source.roughness, 0.02f, 1.0f));
        realtime_renderer_->setMaterialFloat(material, "alphaCutoff", std::clamp(source.alphaCutoff, 0.0f, 1.0f));
        realtime_renderer_->setMaterialInt(material, "alphaMode", std::clamp<int>(source.alphaMode, 0, 2));
        realtime_renderer_->setMaterialFloat(material, "emissiveStrength",
            std::max({source.emission[0], source.emission[1], source.emission[2], 0.0f}));
    }

    auto* backend = realtime_renderer_->backend();
    realtime_textures_.reserve(scene.textures.size());
    for (const auto& source : scene.textures) {
        zegfx::TextureHandle texture;
        if (source.width > 0 && source.height > 0 &&
            source.rgba.size() >= static_cast<size_t>(source.width) * source.height * 4) {
            texture = backend->createTextureRgba(source.rgba.data(), static_cast<int>(source.width),
                                                  static_cast<int>(source.height));
        }
        realtime_textures_.push_back(texture);
    }
    for (size_t materialIndex = 0; materialIndex < scene.materials.size(); ++materialIndex) {
        const auto& source = scene.materials[materialIndex];
        const auto bindTexture = [&](int32_t textureIndex, const char* slot) {
            if (textureIndex >= 0 && static_cast<size_t>(textureIndex) < realtime_textures_.size() &&
                realtime_textures_[static_cast<size_t>(textureIndex)].valid()) {
                realtime_renderer_->setMaterialTextureHandle(realtime_materials_[materialIndex], slot,
                    realtime_textures_[static_cast<size_t>(textureIndex)]);
            }
        };
        bindTexture(source.baseColorTexture, "baseColorTexture");
        bindTexture(source.normalTexture, "normalMap");
        bindTexture(source.metallicRoughnessTexture, "metallicRoughnessTexture");
        bindTexture(source.emissiveTexture, "emissiveMap");
        bindTexture(source.occlusionTexture, "occlusionMap");
    }

    struct MeshBuild {
        std::vector<zegfx::ProceduralVertex> vertices;
        std::vector<uint32_t> indices;
        std::vector<RealtimeWireEdge> wire_edges;
    };
    std::map<std::pair<uint32_t, uint32_t>, MeshBuild> meshes;
    using PositionKey = std::array<uint32_t, 3>;
    using EdgeKey = std::pair<PositionKey, PositionKey>;
    struct EdgeKeyHash {
        size_t operator()(const EdgeKey& edge) const noexcept {
            size_t hash = static_cast<size_t>(1469598103934665603ull);
            const auto mix = [&hash](uint32_t value) {
                hash ^= static_cast<size_t>(value);
                hash *= static_cast<size_t>(1099511628211ull);
            };
            for (uint32_t value : edge.first) mix(value);
            for (uint32_t value : edge.second) mix(value);
            return hash;
        }
    };
    std::map<std::pair<uint32_t, uint32_t>, std::unordered_set<EdgeKey, EdgeKeyHash>> uniqueEdges;
    const auto positionKey = [](const auto& vertex) {
        PositionKey key{};
        for (size_t axis = 0; axis < 3; ++axis) {
            float value = vertex.position[axis] == 0.0f ? 0.0f : vertex.position[axis];
            std::memcpy(&key[axis], &value, sizeof(value));
        }
        return key;
    };
    for (const auto& triangle : scene.triangles) {
        uint32_t nodeIndex = triangle.nodeIndex >= 0 &&
            static_cast<size_t>(triangle.nodeIndex) < realtime_nodes_.size()
                ? static_cast<uint32_t>(triangle.nodeIndex) : 0u;
        const uint32_t materialIndex = triangle.materialIndex < realtime_materials_.size()
            ? triangle.materialIndex : 0u;
        auto& mesh = meshes[{nodeIndex, materialIndex}];
        if (mesh.vertices.size() > std::numeric_limits<uint32_t>::max() - 3u) {
            error = "Scene mesh exceeds ZeGFX's 32-bit index range.";
            destroy_realtime_scene();
            return false;
        }
        const uint32_t firstVertex = static_cast<uint32_t>(mesh.vertices.size());
        for (const auto& source : triangle.vertices) {
            zegfx::ProceduralVertex vertex{};
            vertex.x = source.position[0]; vertex.y = source.position[1]; vertex.z = source.position[2];
            vertex.nx = source.normal[0]; vertex.ny = source.normal[1]; vertex.nz = source.normal[2];
            vertex.u = source.uv[0]; vertex.v = source.uv[1];
            vertex.u1 = source.uv1[0]; vertex.v1 = source.uv1[1];
            vertex.hasUv1 = source.uv1[0] != source.uv[0] || source.uv1[1] != source.uv[1];
            vertex.color = material_color(source.color);
            mesh.vertices.push_back(vertex);
        }
        mesh.indices.push_back(firstVertex);
        mesh.indices.push_back(firstVertex + 1u);
        mesh.indices.push_back(firstVertex + 2u);
        auto& edges = uniqueEdges[{nodeIndex, materialIndex}];
        for (size_t edgeIndex = 0; edgeIndex < 3; ++edgeIndex) {
            const auto& start = triangle.vertices[edgeIndex];
            const auto& end = triangle.vertices[(edgeIndex + 1) % 3];
            const PositionKey startKey = positionKey(start);
            const PositionKey endKey = positionKey(end);
            const bool forward = startKey < endKey;
            const EdgeKey edgeKey = forward ? EdgeKey{startKey, endKey} : EdgeKey{endKey, startKey};
            if (!edges.insert(edgeKey).second) continue;
            RealtimeWireEdge wireEdge;
            const auto& first = forward ? start : end;
            const auto& second = forward ? end : start;
            for (size_t axis = 0; axis < 3; ++axis) {
                wireEdge.start[axis] = first.position[axis];
                wireEdge.end[axis] = second.position[axis];
            }
            mesh.wire_edges.push_back(wireEdge);
        }
    }

    realtime_draw_items_.reserve(meshes.size());
    for (auto& [key, source] : meshes) {
        if (source.vertices.empty() || source.indices.empty()) continue;
        const auto mesh = realtime_renderer_->createProceduralMesh(source.vertices, source.indices);
        if (!mesh.valid()) {
            error = "ZeGFX could not upload one or more scene meshes.";
            destroy_realtime_scene();
            return false;
        }
        realtime_draw_items_.push_back({key.first, mesh, realtime_materials_[key.second],
                                        std::move(source.wire_edges)});
    }
    realtime_view_available_ = true;
    ++realtime_scene_generation_;
    mark_realtime_history_dirty();
    return true;
}

void Graphics::clear_production_scene() {
    destroy_realtime_scene();
    if (production_pipeline_) production_pipeline_->clearScene();
    production_scene_submitted_ = false;
    mark_realtime_history_dirty();
}

void Graphics::destroy_realtime_scene() {
    if (realtime_renderer_ && realtime_renderer_->backend()) {
        auto* backend = realtime_renderer_->backend();
        backend->waitIdle();
        for (const auto& item : realtime_draw_items_)
            if (item.mesh.valid()) realtime_renderer_->destroyMesh(item.mesh);
        for (const auto material : realtime_materials_)
            if (material.valid()) realtime_renderer_->destroyMaterial(material);
        for (const auto texture : realtime_textures_)
            if (texture.valid()) backend->unloadTexture(texture);
    }
    realtime_draw_items_.clear();
    realtime_materials_.clear();
    realtime_textures_.clear();
    realtime_nodes_.clear();
    realtime_view_available_ = false;
}

bool Graphics::update_production_view(const zegfx::ProductionViewSettings& view, std::string& error) {
    if (!production_scene_submitted_ || !production_pipeline_) {
        error = "Open a production asset before changing the render view.";
        return false;
    }
    const bool updated = production_pipeline_->updateView(view, error);
    return updated;
}

bool Graphics::update_production_settings(const zegfx::ProductionRenderSettings& settings, std::string& error) {
    if (!production_pipeline_) {
        error = "The ZeGFX production pipeline is not initialized.";
        return false;
    }
    return production_pipeline_->updateRenderSettings(settings, error);
}

bool Graphics::set_production_node_state(uint32_t nodeIndex, bool visible, bool renderable,
                                         std::string& error) {
    if (!production_scene_submitted_ || !production_pipeline_) {
        error = "There is no imported production scene.";
        return false;
    }
    const bool updated = production_pipeline_->setNodeState(nodeIndex, visible, renderable, error);
    if (updated) {
        if (nodeIndex < realtime_nodes_.size()) {
            realtime_nodes_[nodeIndex].visible = visible;
            realtime_nodes_[nodeIndex].renderable = renderable;
        }
        mark_realtime_history_dirty();
    }
    return updated;
}

bool Graphics::set_production_node_translation(uint32_t nodeIndex, const float position[3],
                                               std::string& error) {
    if (!production_scene_submitted_ || !production_pipeline_) {
        error = "There is no imported production scene.";
        return false;
    }
    const bool updated = production_pipeline_->setNodeTranslation(nodeIndex, position, error);
    if (updated) {
        if (nodeIndex < realtime_nodes_.size() && position) {
            const float delta[3] = {
                position[0] - realtime_nodes_[nodeIndex].worldTransform[12],
                position[1] - realtime_nodes_[nodeIndex].worldTransform[13],
                position[2] - realtime_nodes_[nodeIndex].worldTransform[14]};
            for (size_t candidate = 0; candidate < realtime_nodes_.size(); ++candidate) {
                int ancestor = static_cast<int>(candidate);
                while (ancestor >= 0 && static_cast<size_t>(ancestor) < realtime_nodes_.size() &&
                       ancestor != static_cast<int>(nodeIndex))
                    ancestor = realtime_nodes_[static_cast<size_t>(ancestor)].parentIndex;
                if (candidate != nodeIndex && ancestor != static_cast<int>(nodeIndex)) continue;
                for (int axis = 0; axis < 3; ++axis)
                    realtime_nodes_[candidate].worldTransform[12 + axis] += delta[axis];
            }
        }
        mark_realtime_history_dirty();
    }
    return updated;
}

bool Graphics::set_production_node_light_energy(uint32_t nodeIndex, float energy, std::string& error) {
    if (!production_scene_submitted_ || !production_pipeline_) {
        error = "There is no rendered scene to update.";
        return false;
    }
    const bool updated = production_pipeline_->setNodeLightEnergy(nodeIndex, energy, error);
    if (updated) {
        if (nodeIndex < realtime_nodes_.size()) realtime_nodes_[nodeIndex].lightEnergy = energy;
        mark_realtime_history_dirty();
    }
    return updated;
}

bool Graphics::pick_production_node(const float rayOrigin[3], const float rayDirection[3],
                                    uint32_t& nodeIndex, float& distance) const {
    return production_scene_submitted_ && production_pipeline_ &&
           production_pipeline_->pickNode(rayOrigin, rayDirection, nodeIndex, distance);
}

bool Graphics::set_production_node_rotation_scale(
    uint32_t nodeIndex, const float rotationOffset[3], const float scaleFactor[3],
    std::vector<zegfx::ProductionSceneNode>& updatedNodes, std::string& error) {
    if (!production_scene_submitted_ || !production_pipeline_) {
        error = "There is no imported production scene.";
        return false;
    }
    const bool updated = production_pipeline_->setNodeRotationScale(nodeIndex, rotationOffset, scaleFactor,
                                                                      updatedNodes, error);
    if (updated) {
        if (updatedNodes.size() == realtime_nodes_.size()) realtime_nodes_ = updatedNodes;
        mark_realtime_history_dirty();
    }
    return updated;
}

bool Graphics::export_production_png(const std::string& path, std::string& error) {
    if (!production_scene_submitted_ || !production_pipeline_) {
        error = "There is no rendered production frame to export.";
        return false;
    }
    return production_pipeline_->exportPNG(path, error);
}

bool Graphics::export_production_exr(const std::string& path, std::string& error) {
    if (!production_scene_submitted_ || !production_pipeline_) {
        error = "There is no rendered production frame to export.";
        return false;
    }
    return production_pipeline_->exportOpenEXR(path, error);
}

bool Graphics::restart_production_render() {
    if (!production_scene_submitted_ || !production_pipeline_) return false;
    production_pipeline_->resetAccumulation();
    return true;
}

void Graphics::trace_production_frame() {
    if (production_render_active_ && production_scene_submitted_ && production_pipeline_ &&
        !production_pipeline_->getProgress().isConverged)
        production_pipeline_->traceSamples(1);
}

bool Graphics::production_render_complete() const {
    return !production_scene_submitted_ || !production_pipeline_ ||
           production_pipeline_->getProgress().isConverged;
}

zegfx::ProductionRenderProgress Graphics::production_render_progress() const {
    return production_pipeline_ ? production_pipeline_->getProgress() : zegfx::ProductionRenderProgress{};
}

bool Graphics::draw_realtime_viewport(const Rect& bounds, const Rect* overlayBounds) {
    if (!realtime_host_window_ || !realtime_view_available_ || bounds.width <= 0.0f || bounds.height <= 0.0f) {
        if (realtime_host_window_) ShowWindow(realtime_host_window_, SW_HIDE);
        return false;
    }
    const int x = static_cast<int>(std::lround(bounds.x * dpi_scale_));
    const int y = static_cast<int>(std::lround(bounds.y * dpi_scale_));
    const int width = std::max(1, static_cast<int>(std::lround(bounds.width * dpi_scale_)));
    const int height = std::max(1, static_cast<int>(std::lround(bounds.height * dpi_scale_)));
    realtime_viewport_logical_bounds_ = bounds;
    const RECT nextBounds{x, y, x + width, y + height};
    if (!realtime_child_bounds_valid_ ||
        std::memcmp(&nextBounds, &realtime_child_bounds_, sizeof(RECT)) != 0) {
        if (realtime_viewport_width_ != static_cast<uint32_t>(width) ||
            realtime_viewport_height_ != static_cast<uint32_t>(height))
            resize_realtime_viewport(static_cast<uint32_t>(width), static_cast<uint32_t>(height));
        SetWindowPos(realtime_host_window_, HWND_TOP, x, y, width, height,
                     SWP_NOACTIVATE | SWP_SHOWWINDOW);
        realtime_child_bounds_ = nextBounds;
        realtime_child_bounds_valid_ = true;
        realtime_child_overlay_valid_ = false;
    }
    set_realtime_viewport_overlay_bounds(overlayBounds);
    ShowWindow(realtime_host_window_, SW_SHOWNOACTIVATE);
    return true;
}

void Graphics::set_realtime_viewport_overlay_bounds(const Rect* overlayBounds) {
    if (!realtime_host_window_ || !realtime_child_bounds_valid_) return;
    const int width = realtime_child_bounds_.right - realtime_child_bounds_.left;
    const int height = realtime_child_bounds_.bottom - realtime_child_bounds_.top;
    RECT overlay{};
    bool hasOverlay = false;
    if (overlayBounds && overlayBounds->width > 0.0f && overlayBounds->height > 0.0f) {
        overlay.left = static_cast<LONG>(std::lround(
            (overlayBounds->x - realtime_viewport_logical_bounds_.x) * dpi_scale_));
        overlay.top = static_cast<LONG>(std::lround(
            (overlayBounds->y - realtime_viewport_logical_bounds_.y) * dpi_scale_));
        overlay.right = static_cast<LONG>(std::lround(
            (overlayBounds->right() - realtime_viewport_logical_bounds_.x) * dpi_scale_));
        overlay.bottom = static_cast<LONG>(std::lround(
            (overlayBounds->bottom() - realtime_viewport_logical_bounds_.y) * dpi_scale_));
        RECT viewportRect{0, 0, width, height};
        RECT clipped{};
        hasOverlay = IntersectRect(&clipped, &overlay, &viewportRect) != FALSE;
        if (hasOverlay) overlay = clipped;
    }
    if (hasOverlay) {
        if (!realtime_child_overlay_valid_ ||
            std::memcmp(&overlay, &realtime_child_overlay_bounds_, sizeof(RECT)) != 0) {
            HRGN viewportRegion = CreateRectRgn(0, 0, width, height);
            HRGN overlayRegion = CreateRectRgn(overlay.left, overlay.top, overlay.right, overlay.bottom);
            if (viewportRegion && overlayRegion &&
                CombineRgn(viewportRegion, viewportRegion, overlayRegion, RGN_DIFF) != ERROR) {
                if (SetWindowRgn(realtime_host_window_, viewportRegion, TRUE) != 0) {
                    realtime_child_overlay_bounds_ = overlay;
                    realtime_child_overlay_valid_ = true;
                    viewportRegion = nullptr; // ownership transferred to the window
                }
            }
            if (viewportRegion) DeleteObject(viewportRegion);
            if (overlayRegion) DeleteObject(overlayRegion);
        }
    } else if (realtime_child_overlay_valid_) {
        SetWindowRgn(realtime_host_window_, nullptr, TRUE);
        realtime_child_overlay_valid_ = false;
    }
}
void Graphics::clear(Color color) {
    if (d2d_context_ && d2d_frame_active_) {
        d2d_context_->Clear(D2D1::ColorF(color.r, color.g, color.b, color.a));
    }
}

ID2D1SolidColorBrush* Graphics::get_brush(Color color) {
    uint32_t key = (static_cast<uint32_t>(color.r * 255.0f) << 24) |
                   (static_cast<uint32_t>(color.g * 255.0f) << 16) |
                   (static_cast<uint32_t>(color.b * 255.0f) << 8)  |
                   (static_cast<uint32_t>(color.a * 255.0f));

    auto it = brush_cache_.find(key);
    if (it != brush_cache_.end()) {
        return it->second.Get();
    }

    ComPtr<ID2D1SolidColorBrush> brush;
    if (!d2d_context_ || FAILED(d2d_context_->CreateSolidColorBrush(
            D2D1::ColorF(color.r, color.g, color.b, color.a), brush.GetAddressOf())))
        return nullptr;
    auto inserted = brush_cache_.emplace(key, std::move(brush));
    return inserted.first->second.Get();
}

IDWriteTextFormat* Graphics::get_text_format(const std::string &font_family, float font_size, bool bold) {
    std::string key = font_family + "_" + std::to_string(font_size) + (bold ? "_b" : "");
    auto it = font_cache_.find(key);
    if (it != font_cache_.end()) {
        return it->second.Get();
    }

    if (!dwrite_factory_) return nullptr;

    std::wstring wfamily(font_family.begin(), font_family.end());
    ComPtr<IDWriteTextFormat> fmt;
    HRESULT hr = dwrite_factory_->CreateTextFormat(
        wfamily.c_str(),
        nullptr,
        bold ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        font_size,
        L"en-us",
        fmt.GetAddressOf()
    );
    if (FAILED(hr)) {
        // Fallback to standard Segoe UI if specific variable font not present
        hr = dwrite_factory_->CreateTextFormat(
            L"Segoe UI",
            nullptr,
            bold ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            font_size,
            L"en-us",
            fmt.GetAddressOf()
        );
    }
    if (SUCCEEDED(hr)) {
        fmt->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        font_cache_[key] = fmt;
        return fmt.Get();
    }
    return nullptr;
}

void Graphics::fill_rect(const Rect &r, Color color) {
    auto* brush = get_brush(color);
    if (brush && d2d_context_ && d2d_frame_active_) {
        d2d_context_->FillRectangle(D2D1::RectF(r.x, r.y, r.right(), r.bottom()), brush);
    }
}

void Graphics::fill_polygon(const Point* points, size_t count, Color color) {
    if (!d2d_factory_ || !d2d_context_ || !d2d_frame_active_ || !points || count < 3) return;
    auto* brush = get_brush(color);
    if (!brush) return;

    ComPtr<ID2D1PathGeometry> geometry;
    if (FAILED(d2d_factory_->CreatePathGeometry(geometry.GetAddressOf()))) return;
    ComPtr<ID2D1GeometrySink> sink;
    if (FAILED(geometry->Open(sink.GetAddressOf()))) return;
    sink->BeginFigure(D2D1::Point2F(points[0].x, points[0].y), D2D1_FIGURE_BEGIN_FILLED);
    std::vector<D2D1_POINT_2F> vertices;
    vertices.reserve(count - 1);
    for (size_t i = 1; i < count; ++i)
        vertices.push_back(D2D1::Point2F(points[i].x, points[i].y));
    sink->AddLines(vertices.data(), static_cast<UINT32>(vertices.size()));
    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    if (FAILED(sink->Close())) return;
    d2d_context_->FillGeometry(geometry.Get(), brush);
}

void Graphics::draw_rect(const Rect &r, Color color, float stroke) {
    auto* brush = get_brush(color);
    if (brush && d2d_context_ && d2d_frame_active_) {
        d2d_context_->DrawRectangle(D2D1::RectF(r.x, r.y, r.right(), r.bottom()), brush, stroke);
    }
}

void Graphics::fill_rounded_rect(const Rect &r, float radius, Color color) {
    auto* brush = get_brush(color);
    if (brush && d2d_context_ && d2d_frame_active_) {
        D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(D2D1::RectF(r.x, r.y, r.right(), r.bottom()), radius, radius);
        d2d_context_->FillRoundedRectangle(rr, brush);
    }
}

void Graphics::draw_rounded_rect(const Rect &r, float radius, Color color, float stroke) {
    auto* brush = get_brush(color);
    if (brush && d2d_context_ && d2d_frame_active_) {
        D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(D2D1::RectF(r.x, r.y, r.right(), r.bottom()), radius, radius);
        d2d_context_->DrawRoundedRectangle(rr, brush, stroke);
    }
}

void Graphics::draw_line(Point p1, Point p2, Color color, float stroke) {
    auto* brush = get_brush(color);
    if (brush && d2d_context_ && d2d_frame_active_) {
        d2d_context_->DrawLine(D2D1::Point2F(p1.x, p1.y), D2D1::Point2F(p2.x, p2.y), brush, stroke);
    }
}

void Graphics::draw_text(const std::string &text, const Rect &r, Color color,
                        float font_size, bool bold,
                        bool center_h, bool center_v,
                        const std::string &font_family) {
    if (text.empty()) return;
    auto* brush = get_brush(color);
    auto fmt = get_text_format(font_family, font_size, bold);
    if (brush && fmt && d2d_context_ && d2d_frame_active_) {
        fmt->SetTextAlignment(center_h ? DWRITE_TEXT_ALIGNMENT_CENTER : DWRITE_TEXT_ALIGNMENT_LEADING);
        fmt->SetParagraphAlignment(center_v ? DWRITE_PARAGRAPH_ALIGNMENT_CENTER : DWRITE_PARAGRAPH_ALIGNMENT_NEAR);

        int len = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
        if (len <= 0) return;
        std::array<wchar_t, 128> stack_text{};
        std::wstring heap_text;
        wchar_t* wide_text = stack_text.data();
        if (static_cast<size_t>(len) > stack_text.size()) {
            heap_text.resize(static_cast<size_t>(len));
            wide_text = heap_text.data();
        }
        if (MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide_text, len) != len)
            return;

        d2d_context_->DrawText(wide_text, static_cast<UINT32>(len), fmt,
                               D2D1::RectF(r.x, r.y, r.right(), r.bottom()), brush);
    }
}

void Graphics::push_clip(const Rect &r) {
    if (d2d_context_ && d2d_frame_active_) {
        d2d_context_->PushAxisAlignedClip(D2D1::RectF(r.x, r.y, r.right(), r.bottom()), D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    }
}

void Graphics::pop_clip() {
    if (d2d_context_ && d2d_frame_active_) {
        d2d_context_->PopAxisAlignedClip();
    }
}

bool Graphics::save_screenshot(const std::string &filepath) {
    if (!swapchain_ || !d3d_device_ || !d3d_context_) return false;

    ComPtr<ID3D11Texture2D> back_buffer;
    ID3D11Resource* wrapped_back_buffer = nullptr;
    const UINT index = swapchain3_ ? swapchain3_->GetCurrentBackBufferIndex() : 0;
    bool acquired_for_capture = false;
    HRESULT hr = S_OK;
    if (d3d11on12_device_) {
        const UINT wrapped_index = index % static_cast<UINT>(wrapped_backbuffers_.size());
        if (!wrapped_backbuffers_[wrapped_index]) return false;
        wrapped_back_buffer = wrapped_backbuffers_[wrapped_index].Get();
        hr = wrapped_back_buffer->QueryInterface(IID_PPV_ARGS(back_buffer.GetAddressOf()));
        if (FAILED(hr)) return false;
        if (!backbuffer_acquired_) {
            d3d11on12_device_->AcquireWrappedResources(&wrapped_back_buffer, 1);
            acquired_for_capture = true;
        }
    } else {
        hr = swapchain_->GetBuffer(index, IID_PPV_ARGS(back_buffer.GetAddressOf()));
        if (FAILED(hr)) return false;
    }

    D3D11_TEXTURE2D_DESC desc;
    back_buffer->GetDesc(&desc);

    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.MiscFlags = 0;

    ComPtr<ID3D11Texture2D> staging_tex;
    hr = d3d_device_->CreateTexture2D(&desc, nullptr, staging_tex.GetAddressOf());
    if (FAILED(hr)) {
        if (acquired_for_capture) d3d11on12_device_->ReleaseWrappedResources(&wrapped_back_buffer, 1);
        return false;
    }

    d3d_context_->CopyResource(staging_tex.Get(), back_buffer.Get());
    if (acquired_for_capture) d3d11on12_device_->ReleaseWrappedResources(&wrapped_back_buffer, 1);
    d3d_context_->Flush();

    D3D11_MAPPED_SUBRESOURCE mapped;
    hr = d3d_context_->Map(staging_tex.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr)) return false;

    uint32_t width = desc.Width;
    uint32_t height = desc.Height;
    uint32_t row_size = width * 4;
    uint32_t image_size = row_size * height;

    BITMAPFILEHEADER bfh = {};
    bfh.bfType = 0x4D42; // "BM"
    bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    bfh.bfSize = bfh.bfOffBits + image_size;

    BITMAPINFOHEADER bih = {};
    bih.biSize = sizeof(BITMAPINFOHEADER);
    bih.biWidth = width;
    bih.biHeight = -static_cast<LONG>(height); // Top-down
    bih.biPlanes = 1;
    bih.biBitCount = 32;
    bih.biCompression = BI_RGB;

    HANDLE hFile = CreateFileA(filepath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        d3d_context_->Unmap(staging_tex.Get(), 0);
        return false;
    }

    DWORD bytes_written = 0;
    WriteFile(hFile, &bfh, sizeof(bfh), &bytes_written, nullptr);
    WriteFile(hFile, &bih, sizeof(bih), &bytes_written, nullptr);

    std::vector<uint8_t> row_buffer(row_size);
    const uint8_t *src = static_cast<const uint8_t*>(mapped.pData);
    for (uint32_t y = 0; y < height; ++y) {
        const uint8_t *src_row = src + y * mapped.RowPitch;
        for (uint32_t x = 0; x < width; ++x) {
            row_buffer[x * 4 + 0] = src_row[x * 4 + 0]; // B
            row_buffer[x * 4 + 1] = src_row[x * 4 + 1]; // G
            row_buffer[x * 4 + 2] = src_row[x * 4 + 2]; // R
            row_buffer[x * 4 + 3] = 0xFF;               // Force 100% opaque alpha
        }
        WriteFile(hFile, row_buffer.data(), row_size, &bytes_written, nullptr);
    }

    CloseHandle(hFile);
    d3d_context_->Unmap(staging_tex.Get(), 0);
    return true;
}

} // namespace medusa
