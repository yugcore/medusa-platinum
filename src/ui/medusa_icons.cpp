#include <medusa/medusa_icons.hpp>
#include <d2d1_3.h>
#include <unordered_map>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <algorithm>
#include <unordered_set>

namespace medusa {

static std::unordered_map<std::string, ComPtr<ID2D1PathGeometry>> s_geometries;
static std::unordered_map<std::string, ComPtr<ID2D1SvgDocument>> s_svg_documents;
static std::unordered_map<std::string, std::filesystem::path> s_icon_paths;
static std::unordered_set<std::string> s_missing_icon_paths;
static ComPtr<ID2D1DeviceContext5> s_svg_context;

static std::string color_hex(Color color) {
    auto channel = [](float value) {
        return static_cast<unsigned int>(std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f);
    };
    std::ostringstream out;
    out << '#' << std::hex << std::uppercase << std::setfill('0')
        << std::setw(2) << channel(color.r)
        << std::setw(2) << channel(color.g)
        << std::setw(2) << channel(color.b);
    return out.str();
}

static std::filesystem::path find_icon_file(const std::string &name) {
    if (name.empty() || std::filesystem::path(name).filename() != name) return {};
    const auto cached = s_icon_paths.find(name);
    if (cached != s_icon_paths.end()) return cached->second;
    if (s_missing_icon_paths.find(name) != s_missing_icon_paths.end()) return {};
    std::wstring module_path(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, module_path.data(), static_cast<DWORD>(module_path.size()));
    if (length == 0 || length >= module_path.size()) return {};
    module_path.resize(length);

    std::vector<std::filesystem::path> roots;
    const auto executable_dir = std::filesystem::path(module_path).parent_path();
    roots.push_back(executable_dir / "resources" / "icons");
    auto ancestor = executable_dir;
    for (int depth = 0; depth < 8 && !ancestor.empty(); ++depth) {
        roots.push_back(ancestor / "MedusaPlatinum" / "resources" / "icons");
        roots.push_back(ancestor / "resources" / "icons");
        const auto parent = ancestor.parent_path();
        if (parent == ancestor) break;
        ancestor = parent;
    }
    roots.push_back(std::filesystem::current_path() / "MedusaPlatinum" / "resources" / "icons");
    roots.push_back(std::filesystem::current_path() / "resources" / "icons");

    for (const auto &root : roots) {
        const auto candidate = root / (name + ".svg");
        std::error_code ec;
        if (std::filesystem::is_regular_file(candidate, ec)) {
            s_icon_paths.emplace(name, candidate);
            return candidate;
        }
    }
    s_missing_icon_paths.insert(name);
    return {};
}

static ID2D1SvgDocument *get_svg_document(ID2D1DeviceContext5 *context,
                                           const std::string &name, Color color) {
    if (!context) return nullptr;
    const std::string tint = color_hex(color);
    const std::string key = name + tint;
    const auto cached = s_svg_documents.find(key);
    if (cached != s_svg_documents.end()) return cached->second.Get();

    const auto path = find_icon_file(name);
    if (path.empty()) return nullptr;
    std::ifstream file(path, std::ios::binary);
    if (!file) return nullptr;
    std::string xml((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    const auto svg_tag = xml.find("<svg");
    if (svg_tag == std::string::npos) return nullptr;
    const auto tag_end = xml.find('>', svg_tag);
    if (tag_end == std::string::npos) return nullptr;
    xml.insert(tag_end, " color=\"" + tint + "\"");

    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, xml.size());
    if (!memory) return nullptr;
    void *bytes = GlobalLock(memory);
    if (!bytes) { GlobalFree(memory); return nullptr; }
    std::memcpy(bytes, xml.data(), xml.size());
    GlobalUnlock(memory);

    ComPtr<IStream> stream;
    HRESULT hr = CreateStreamOnHGlobal(memory, TRUE, stream.GetAddressOf());
    if (FAILED(hr)) { GlobalFree(memory); return nullptr; }
    ComPtr<ID2D1SvgDocument> document;
    hr = context->CreateSvgDocument(stream.Get(), D2D1::SizeF(24.0f, 24.0f), document.GetAddressOf());
    if (FAILED(hr)) return nullptr;
    s_svg_documents.emplace(key, document);
    return document.Get();
}

static bool draw_svg_icon(Graphics &gfx, const std::string &name, const Rect &dest, Color color) {
    auto *context = gfx.get_context();
    if (!context) return false;
    if (!s_svg_context && FAILED(context->QueryInterface(IID_PPV_ARGS(s_svg_context.GetAddressOf())))) return false;
    auto *document = get_svg_document(s_svg_context.Get(), name, color);
    if (!document) return false;

    D2D1_MATRIX_3X2_F original;
    context->GetTransform(&original);
    const float scale = std::min(dest.width, dest.height) / 24.0f;
    context->SetTransform(D2D1::Matrix3x2F::Scale(scale, scale) *
                          D2D1::Matrix3x2F::Translation(dest.x, dest.y) * original);
    s_svg_context->DrawSvgDocument(document);
    context->SetTransform(original);
    return true;
}

ID2D1PathGeometry* IconRenderer::get_geometry(ID2D1Factory1 *factory, const std::string &name) {
    if (!factory) return nullptr;
    auto it = s_geometries.find(name);
    if (it != s_geometries.end()) {
        return it->second.Get();
    }

    ComPtr<ID2D1PathGeometry> geom;
    HRESULT hr = factory->CreatePathGeometry(geom.GetAddressOf());
    if (FAILED(hr)) return nullptr;

    ComPtr<ID2D1GeometrySink> sink;
    hr = geom->Open(sink.GetAddressOf());
    if (FAILED(hr)) return nullptr;

    sink->SetFillMode(D2D1_FILL_MODE_ALTERNATE);

    if (name == "tool_select") {
        sink->BeginFigure(D2D1::Point2F(4.0f, 2.5f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(18.5f, 12.0f));
        sink->AddLine(D2D1::Point2F(11.5f, 14.0f));
        sink->AddLine(D2D1::Point2F(15.5f, 21.5f));
        sink->AddLine(D2D1::Point2F(12.0f, 23.5f));
        sink->AddLine(D2D1::Point2F(8.0f, 16.0f));
        sink->AddLine(D2D1::Point2F(4.0f, 20.0f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    } else if (name == "tool_move") {
        sink->BeginFigure(D2D1::Point2F(12.0f, 1.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(16.0f, 5.0f));
        sink->AddLine(D2D1::Point2F(13.0f, 5.0f));
        sink->AddLine(D2D1::Point2F(13.0f, 11.0f));
        sink->AddLine(D2D1::Point2F(19.0f, 11.0f));
        sink->AddLine(D2D1::Point2F(19.0f, 8.0f));
        sink->AddLine(D2D1::Point2F(23.0f, 12.0f));
        sink->AddLine(D2D1::Point2F(19.0f, 16.0f));
        sink->AddLine(D2D1::Point2F(19.0f, 13.0f));
        sink->AddLine(D2D1::Point2F(13.0f, 13.0f));
        sink->AddLine(D2D1::Point2F(13.0f, 19.0f));
        sink->AddLine(D2D1::Point2F(16.0f, 19.0f));
        sink->AddLine(D2D1::Point2F(12.0f, 23.0f));
        sink->AddLine(D2D1::Point2F(8.0f, 19.0f));
        sink->AddLine(D2D1::Point2F(11.0f, 19.0f));
        sink->AddLine(D2D1::Point2F(11.0f, 13.0f));
        sink->AddLine(D2D1::Point2F(5.0f, 13.0f));
        sink->AddLine(D2D1::Point2F(5.0f, 16.0f));
        sink->AddLine(D2D1::Point2F(1.0f, 12.0f));
        sink->AddLine(D2D1::Point2F(5.0f, 8.0f));
        sink->AddLine(D2D1::Point2F(5.0f, 11.0f));
        sink->AddLine(D2D1::Point2F(11.0f, 11.0f));
        sink->AddLine(D2D1::Point2F(11.0f, 5.0f));
        sink->AddLine(D2D1::Point2F(8.0f, 5.0f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    } else if (name == "tool_rotate") {
        sink->BeginFigure(D2D1::Point2F(12.0f, 2.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(16.0f, 6.0f));
        sink->AddLine(D2D1::Point2F(13.0f, 6.0f));
        sink->AddLine(D2D1::Point2F(13.0f, 9.0f));
        sink->AddLine(D2D1::Point2F(18.0f, 9.0f));
        sink->AddLine(D2D1::Point2F(21.0f, 12.0f));
        sink->AddLine(D2D1::Point2F(18.0f, 15.0f));
        sink->AddLine(D2D1::Point2F(13.0f, 15.0f));
        sink->AddLine(D2D1::Point2F(13.0f, 18.0f));
        sink->AddLine(D2D1::Point2F(16.0f, 18.0f));
        sink->AddLine(D2D1::Point2F(12.0f, 22.0f));
        sink->AddLine(D2D1::Point2F(8.0f, 18.0f));
        sink->AddLine(D2D1::Point2F(11.0f, 18.0f));
        sink->AddLine(D2D1::Point2F(11.0f, 15.0f));
        sink->AddLine(D2D1::Point2F(6.0f, 15.0f));
        sink->AddLine(D2D1::Point2F(3.0f, 12.0f));
        sink->AddLine(D2D1::Point2F(6.0f, 9.0f));
        sink->AddLine(D2D1::Point2F(11.0f, 9.0f));
        sink->AddLine(D2D1::Point2F(11.0f, 6.0f));
        sink->AddLine(D2D1::Point2F(8.0f, 6.0f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    } else if (name == "tool_scale") {
        // Top-left expanding box
        sink->BeginFigure(D2D1::Point2F(2.0f, 2.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(9.0f, 2.0f));
        sink->AddLine(D2D1::Point2F(9.0f, 5.0f));
        sink->AddLine(D2D1::Point2F(5.0f, 5.0f));
        sink->AddLine(D2D1::Point2F(9.5f, 9.5f));
        sink->AddLine(D2D1::Point2F(7.5f, 11.5f));
        sink->AddLine(D2D1::Point2F(3.0f, 7.0f));
        sink->AddLine(D2D1::Point2F(3.0f, 11.0f));
        sink->AddLine(D2D1::Point2F(0.0f, 11.0f));
        sink->AddLine(D2D1::Point2F(0.0f, 2.0f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);

        // Bottom-right expanding box
        sink->BeginFigure(D2D1::Point2F(22.0f, 22.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(15.0f, 22.0f));
        sink->AddLine(D2D1::Point2F(15.0f, 19.0f));
        sink->AddLine(D2D1::Point2F(19.0f, 19.0f));
        sink->AddLine(D2D1::Point2F(14.5f, 14.5f));
        sink->AddLine(D2D1::Point2F(16.5f, 12.5f));
        sink->AddLine(D2D1::Point2F(21.0f, 17.0f));
        sink->AddLine(D2D1::Point2F(21.0f, 13.0f));
        sink->AddLine(D2D1::Point2F(24.0f, 13.0f));
        sink->AddLine(D2D1::Point2F(24.0f, 22.0f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    } else if (name == "tool_snapping") {
        // Magnet horseshoe
        sink->BeginFigure(D2D1::Point2F(3.0f, 2.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(8.0f, 2.0f));
        sink->AddLine(D2D1::Point2F(8.0f, 9.0f));
        sink->AddLine(D2D1::Point2F(16.0f, 9.0f));
        sink->AddLine(D2D1::Point2F(16.0f, 2.0f));
        sink->AddLine(D2D1::Point2F(21.0f, 2.0f));
        sink->AddLine(D2D1::Point2F(21.0f, 10.0f));
        sink->AddLine(D2D1::Point2F(17.0f, 18.0f));
        sink->AddLine(D2D1::Point2F(7.0f, 18.0f));
        sink->AddLine(D2D1::Point2F(3.0f, 10.0f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    } else if (name == "tool_pivot") {
        // Crosshair reticle + center ring
        sink->BeginFigure(D2D1::Point2F(11.0f, 1.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(13.0f, 1.0f));
        sink->AddLine(D2D1::Point2F(13.0f, 5.0f));
        sink->AddLine(D2D1::Point2F(11.0f, 5.0f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);

        sink->BeginFigure(D2D1::Point2F(11.0f, 19.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(13.0f, 19.0f));
        sink->AddLine(D2D1::Point2F(13.0f, 23.0f));
        sink->AddLine(D2D1::Point2F(11.0f, 23.0f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);

        sink->BeginFigure(D2D1::Point2F(1.0f, 11.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(5.0f, 11.0f));
        sink->AddLine(D2D1::Point2F(5.0f, 13.0f));
        sink->AddLine(D2D1::Point2F(1.0f, 13.0f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);

        sink->BeginFigure(D2D1::Point2F(19.0f, 11.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(23.0f, 11.0f));
        sink->AddLine(D2D1::Point2F(23.0f, 13.0f));
        sink->AddLine(D2D1::Point2F(19.0f, 13.0f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);

        // Center ring
        sink->BeginFigure(D2D1::Point2F(12.0f, 7.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(17.0f, 12.0f));
        sink->AddLine(D2D1::Point2F(12.0f, 17.0f));
        sink->AddLine(D2D1::Point2F(7.0f, 12.0f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    } else if (name == "tool_camera_pilot") {
        // Pilot navigation wings & compass
        sink->BeginFigure(D2D1::Point2F(12.0f, 2.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(20.0f, 12.0f));
        sink->AddLine(D2D1::Point2F(14.0f, 11.0f));
        sink->AddLine(D2D1::Point2F(12.0f, 22.0f));
        sink->AddLine(D2D1::Point2F(10.0f, 11.0f));
        sink->AddLine(D2D1::Point2F(4.0f, 12.0f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    } else if (name == "action_render") {
        // Cinema film clapboard
        sink->BeginFigure(D2D1::Point2F(2.0f, 5.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(22.0f, 5.0f));
        sink->AddLine(D2D1::Point2F(22.0f, 19.0f));
        sink->AddLine(D2D1::Point2F(2.0f, 19.0f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);

        // Inner clapboard play cut
        sink->BeginFigure(D2D1::Point2F(10.0f, 8.5f), D2D1_FIGURE_BEGIN_HOLLOW);
        sink->AddLine(D2D1::Point2F(16.0f, 12.0f));
        sink->AddLine(D2D1::Point2F(10.0f, 15.5f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    } else if (name == "file_open" || name == "file_import") {
        // Folder body with a raised tab, matching the resource SVG family.
        sink->BeginFigure(D2D1::Point2F(2.0f, 5.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(9.0f, 5.0f));
        sink->AddLine(D2D1::Point2F(11.0f, 8.0f));
        sink->AddLine(D2D1::Point2F(22.0f, 8.0f));
        sink->AddLine(D2D1::Point2F(19.0f, 20.0f));
        sink->AddLine(D2D1::Point2F(2.0f, 20.0f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
        sink->BeginFigure(D2D1::Point2F(3.0f, 7.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(20.0f, 9.0f));
        sink->AddLine(D2D1::Point2F(18.0f, 18.0f));
        sink->AddLine(D2D1::Point2F(3.0f, 18.0f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    } else if (name == "file_save") {
        sink->BeginFigure(D2D1::Point2F(4.0f, 2.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(17.0f, 2.0f));
        sink->AddLine(D2D1::Point2F(22.0f, 7.0f));
        sink->AddLine(D2D1::Point2F(22.0f, 22.0f));
        sink->AddLine(D2D1::Point2F(2.0f, 22.0f));
        sink->AddLine(D2D1::Point2F(2.0f, 4.0f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
        sink->BeginFigure(D2D1::Point2F(7.0f, 3.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(15.0f, 3.0f));
        sink->AddLine(D2D1::Point2F(15.0f, 9.0f));
        sink->AddLine(D2D1::Point2F(7.0f, 9.0f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
        sink->BeginFigure(D2D1::Point2F(6.0f, 13.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(18.0f, 13.0f));
        sink->AddLine(D2D1::Point2F(18.0f, 21.0f));
        sink->AddLine(D2D1::Point2F(6.0f, 21.0f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    } else if (name == "edit_undo" || name == "edit_redo") {
        const bool redo = name == "edit_redo";
        const float left = redo ? 12.0f : 2.0f;
        const float right = redo ? 22.0f : 12.0f;
        sink->BeginFigure(D2D1::Point2F(redo ? 22.0f : 2.0f, 7.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(redo ? left : right, 2.0f));
        sink->AddLine(D2D1::Point2F(redo ? left : right, 6.0f));
        sink->AddLine(D2D1::Point2F(redo ? 6.0f : 18.0f, 6.0f));
        sink->AddLine(D2D1::Point2F(redo ? 2.0f : 22.0f, 10.0f));
        sink->AddLine(D2D1::Point2F(redo ? 6.0f : 18.0f, 14.0f));
        sink->AddLine(D2D1::Point2F(redo ? left : right, 14.0f));
        sink->AddLine(D2D1::Point2F(redo ? left : right, 18.0f));
        sink->AddLine(D2D1::Point2F(redo ? 7.0f : 17.0f, 18.0f));
        sink->AddLine(D2D1::Point2F(redo ? 7.0f : 17.0f, 10.0f));
        sink->AddLine(D2D1::Point2F(redo ? 22.0f : 2.0f, 10.0f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    } else if (name == "mesh_cube") {
        sink->BeginFigure(D2D1::Point2F(12.0f, 2.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(22.0f, 7.0f));
        sink->AddLine(D2D1::Point2F(22.0f, 17.0f));
        sink->AddLine(D2D1::Point2F(12.0f, 22.0f));
        sink->AddLine(D2D1::Point2F(2.0f, 17.0f));
        sink->AddLine(D2D1::Point2F(2.0f, 7.0f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
        sink->BeginFigure(D2D1::Point2F(12.0f, 4.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(19.0f, 7.5f));
        sink->AddLine(D2D1::Point2F(12.0f, 11.0f));
        sink->AddLine(D2D1::Point2F(5.0f, 7.5f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    } else if (name == "mesh_sphere") {
        sink->BeginFigure(D2D1::Point2F(12.0f, 2.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(18.0f, 4.0f));
        sink->AddLine(D2D1::Point2F(22.0f, 10.0f));
        sink->AddLine(D2D1::Point2F(21.0f, 16.0f));
        sink->AddLine(D2D1::Point2F(16.0f, 21.0f));
        sink->AddLine(D2D1::Point2F(9.0f, 21.0f));
        sink->AddLine(D2D1::Point2F(4.0f, 16.0f));
        sink->AddLine(D2D1::Point2F(2.0f, 10.0f));
        sink->AddLine(D2D1::Point2F(6.0f, 4.0f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    } else if (name == "view_grid") {
        for (int y = 3; y <= 14; y += 9) {
            for (int x = 3; x <= 14; x += 9) {
                sink->BeginFigure(D2D1::Point2F(static_cast<float>(x), static_cast<float>(y)), D2D1_FIGURE_BEGIN_FILLED);
                sink->AddLine(D2D1::Point2F(static_cast<float>(x + 6), static_cast<float>(y)));
                sink->AddLine(D2D1::Point2F(static_cast<float>(x + 6), static_cast<float>(y + 6)));
                sink->AddLine(D2D1::Point2F(static_cast<float>(x), static_cast<float>(y + 6)));
                sink->EndFigure(D2D1_FIGURE_END_CLOSED);
            }
        }
    } else if (name == "camera_safe_frame") {
        sink->BeginFigure(D2D1::Point2F(3.0f, 3.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(9.0f, 3.0f)); sink->AddLine(D2D1::Point2F(9.0f, 6.0f));
        sink->AddLine(D2D1::Point2F(6.0f, 6.0f)); sink->AddLine(D2D1::Point2F(6.0f, 9.0f));
        sink->AddLine(D2D1::Point2F(3.0f, 9.0f)); sink->EndFigure(D2D1_FIGURE_END_CLOSED);
        sink->BeginFigure(D2D1::Point2F(15.0f, 3.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(21.0f, 3.0f)); sink->AddLine(D2D1::Point2F(21.0f, 9.0f));
        sink->AddLine(D2D1::Point2F(18.0f, 9.0f)); sink->AddLine(D2D1::Point2F(18.0f, 6.0f));
        sink->AddLine(D2D1::Point2F(15.0f, 6.0f)); sink->EndFigure(D2D1_FIGURE_END_CLOSED);
        sink->BeginFigure(D2D1::Point2F(3.0f, 15.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(6.0f, 15.0f)); sink->AddLine(D2D1::Point2F(6.0f, 18.0f));
        sink->AddLine(D2D1::Point2F(9.0f, 18.0f)); sink->AddLine(D2D1::Point2F(9.0f, 21.0f));
        sink->AddLine(D2D1::Point2F(3.0f, 21.0f)); sink->EndFigure(D2D1_FIGURE_END_CLOSED);
        sink->BeginFigure(D2D1::Point2F(18.0f, 15.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(21.0f, 15.0f)); sink->AddLine(D2D1::Point2F(21.0f, 21.0f));
        sink->AddLine(D2D1::Point2F(15.0f, 21.0f)); sink->AddLine(D2D1::Point2F(15.0f, 18.0f));
        sink->AddLine(D2D1::Point2F(18.0f, 18.0f)); sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    } else if (name == "light_area") {
        sink->BeginFigure(D2D1::Point2F(3.0f, 3.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(21.0f, 3.0f)); sink->AddLine(D2D1::Point2F(21.0f, 15.0f));
        sink->AddLine(D2D1::Point2F(3.0f, 15.0f)); sink->EndFigure(D2D1_FIGURE_END_CLOSED);
        sink->BeginFigure(D2D1::Point2F(5.0f, 5.0f), D2D1_FIGURE_BEGIN_HOLLOW);
        sink->AddLine(D2D1::Point2F(19.0f, 5.0f)); sink->AddLine(D2D1::Point2F(19.0f, 13.0f));
        sink->AddLine(D2D1::Point2F(5.0f, 13.0f)); sink->EndFigure(D2D1_FIGURE_END_CLOSED);
        sink->BeginFigure(D2D1::Point2F(11.0f, 16.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(13.0f, 16.0f)); sink->AddLine(D2D1::Point2F(13.0f, 20.0f));
        sink->AddLine(D2D1::Point2F(17.0f, 20.0f)); sink->AddLine(D2D1::Point2F(17.0f, 22.0f));
        sink->AddLine(D2D1::Point2F(7.0f, 22.0f)); sink->AddLine(D2D1::Point2F(7.0f, 20.0f));
        sink->AddLine(D2D1::Point2F(11.0f, 20.0f)); sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    } else if (name == "camera_aperture") {
        sink->BeginFigure(D2D1::Point2F(12.0f, 2.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(19.0f, 5.0f)); sink->AddLine(D2D1::Point2F(22.0f, 12.0f));
        sink->AddLine(D2D1::Point2F(19.0f, 19.0f)); sink->AddLine(D2D1::Point2F(12.0f, 22.0f));
        sink->AddLine(D2D1::Point2F(5.0f, 19.0f)); sink->AddLine(D2D1::Point2F(2.0f, 12.0f));
        sink->AddLine(D2D1::Point2F(5.0f, 5.0f)); sink->EndFigure(D2D1_FIGURE_END_CLOSED);
        sink->BeginFigure(D2D1::Point2F(9.0f, 8.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(16.0f, 8.0f)); sink->AddLine(D2D1::Point2F(16.0f, 16.0f));
        sink->AddLine(D2D1::Point2F(8.0f, 16.0f)); sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    } else if (name == "render_settings") {
        for (int y = 5; y <= 17; y += 6) {
            sink->BeginFigure(D2D1::Point2F(3.0f, static_cast<float>(y)), D2D1_FIGURE_BEGIN_FILLED);
            sink->AddLine(D2D1::Point2F(21.0f, static_cast<float>(y)));
            sink->AddLine(D2D1::Point2F(21.0f, static_cast<float>(y + 2)));
            sink->AddLine(D2D1::Point2F(3.0f, static_cast<float>(y + 2)));
            sink->EndFigure(D2D1_FIGURE_END_CLOSED);
        }
    } else if (name == "camera_actor" || name == "outliner_camera") {
        // Professional Studio Camera (Body + Lens Cone)
        // Camera Main Body Box
        sink->BeginFigure(D2D1::Point2F(2.0f, 7.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(14.0f, 7.0f));
        sink->AddLine(D2D1::Point2F(14.0f, 19.0f));
        sink->AddLine(D2D1::Point2F(2.0f, 19.0f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);

        // Top Magazine / Viewfinder
        sink->BeginFigure(D2D1::Point2F(5.0f, 4.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(11.0f, 4.0f));
        sink->AddLine(D2D1::Point2F(11.0f, 7.0f));
        sink->AddLine(D2D1::Point2F(5.0f, 7.0f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);

        // Front Lens Cone
        sink->BeginFigure(D2D1::Point2F(14.0f, 10.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(22.0f, 6.0f));
        sink->AddLine(D2D1::Point2F(22.0f, 18.0f));
        sink->AddLine(D2D1::Point2F(14.0f, 14.0f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    } else if (name == "outliner_collection") {
        // Blender Folder/Collection Icon (Box with tab)
        sink->BeginFigure(D2D1::Point2F(2.0f, 4.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(9.0f, 4.0f));
        sink->AddLine(D2D1::Point2F(11.0f, 7.0f));
        sink->AddLine(D2D1::Point2F(22.0f, 7.0f));
        sink->AddLine(D2D1::Point2F(22.0f, 20.0f));
        sink->AddLine(D2D1::Point2F(2.0f, 20.0f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);

        // Horizontal slot cutout inside folder
        sink->BeginFigure(D2D1::Point2F(5.0f, 10.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(19.0f, 10.0f));
        sink->AddLine(D2D1::Point2F(19.0f, 12.0f));
        sink->AddLine(D2D1::Point2F(5.0f, 12.0f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    } else if (name == "outliner_mesh") {
        // 3D Isometric Faceted Cube / Polygon Mesh (Top, Left, Right faces)
        sink->BeginFigure(D2D1::Point2F(12.0f, 2.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(21.0f, 7.0f));
        sink->AddLine(D2D1::Point2F(21.0f, 17.0f));
        sink->AddLine(D2D1::Point2F(12.0f, 22.0f));
        sink->AddLine(D2D1::Point2F(3.0f, 17.0f));
        sink->AddLine(D2D1::Point2F(3.0f, 7.0f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);

        // Internal Y-axis seam lines for 3D volume
        sink->BeginFigure(D2D1::Point2F(12.0f, 12.0f), D2D1_FIGURE_BEGIN_HOLLOW);
        sink->AddLine(D2D1::Point2F(12.0f, 2.0f));
        sink->EndFigure(D2D1_FIGURE_END_OPEN);

        sink->BeginFigure(D2D1::Point2F(12.0f, 12.0f), D2D1_FIGURE_BEGIN_HOLLOW);
        sink->AddLine(D2D1::Point2F(3.0f, 17.0f));
        sink->EndFigure(D2D1_FIGURE_END_OPEN);

        sink->BeginFigure(D2D1::Point2F(12.0f, 12.0f), D2D1_FIGURE_BEGIN_HOLLOW);
        sink->AddLine(D2D1::Point2F(21.0f, 17.0f));
        sink->EndFigure(D2D1_FIGURE_END_OPEN);
    } else if (name == "light_directional") {
        // Sun Light: Center sphere with radial rays
        sink->BeginFigure(D2D1::Point2F(12.0f, 8.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(16.0f, 12.0f));
        sink->AddLine(D2D1::Point2F(12.0f, 16.0f));
        sink->AddLine(D2D1::Point2F(8.0f, 12.0f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);

        // 4 Cardinal rays
        sink->BeginFigure(D2D1::Point2F(11.0f, 1.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(13.0f, 1.0f));
        sink->AddLine(D2D1::Point2F(13.0f, 5.0f));
        sink->AddLine(D2D1::Point2F(11.0f, 5.0f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);

        sink->BeginFigure(D2D1::Point2F(11.0f, 19.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(13.0f, 19.0f));
        sink->AddLine(D2D1::Point2F(13.0f, 23.0f));
        sink->AddLine(D2D1::Point2F(11.0f, 23.0f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);

        sink->BeginFigure(D2D1::Point2F(1.0f, 11.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(5.0f, 11.0f));
        sink->AddLine(D2D1::Point2F(5.0f, 13.0f));
        sink->AddLine(D2D1::Point2F(1.0f, 13.0f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);

        sink->BeginFigure(D2D1::Point2F(19.0f, 11.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(23.0f, 11.0f));
        sink->AddLine(D2D1::Point2F(23.0f, 13.0f));
        sink->AddLine(D2D1::Point2F(19.0f, 13.0f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    } else if (name == "light_point") {
        // Point Light: Lightbulb profile
        sink->BeginFigure(D2D1::Point2F(9.0f, 2.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(15.0f, 2.0f));
        sink->AddLine(D2D1::Point2F(19.0f, 7.0f));
        sink->AddLine(D2D1::Point2F(16.0f, 13.0f));
        sink->AddLine(D2D1::Point2F(14.0f, 17.0f));
        sink->AddLine(D2D1::Point2F(10.0f, 17.0f));
        sink->AddLine(D2D1::Point2F(8.0f, 13.0f));
        sink->AddLine(D2D1::Point2F(5.0f, 7.0f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);

        // Socket screw threads
        sink->BeginFigure(D2D1::Point2F(10.0f, 19.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(14.0f, 19.0f));
        sink->AddLine(D2D1::Point2F(13.0f, 22.0f));
        sink->AddLine(D2D1::Point2F(11.0f, 22.0f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    } else if (name == "light_spot") {
        // Spotlight: Downward expanding cone with lamp housing
        sink->BeginFigure(D2D1::Point2F(9.0f, 2.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(15.0f, 2.0f));
        sink->AddLine(D2D1::Point2F(15.0f, 6.0f));
        sink->AddLine(D2D1::Point2F(21.0f, 17.0f));
        sink->AddLine(D2D1::Point2F(3.0f, 17.0f));
        sink->AddLine(D2D1::Point2F(9.0f, 6.0f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);

        // Ground stage base
        sink->BeginFigure(D2D1::Point2F(2.0f, 19.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(22.0f, 19.0f));
        sink->AddLine(D2D1::Point2F(22.0f, 21.0f));
        sink->AddLine(D2D1::Point2F(2.0f, 21.0f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    } else if (name == "light_hdri") {
        // Environment HDRI Dome: Upper hemispherical shell + horizon
        sink->BeginFigure(D2D1::Point2F(2.0f, 17.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(3.0f, 12.0f));
        sink->AddLine(D2D1::Point2F(7.0f, 6.0f));
        sink->AddLine(D2D1::Point2F(12.0f, 3.0f));
        sink->AddLine(D2D1::Point2F(17.0f, 6.0f));
        sink->AddLine(D2D1::Point2F(21.0f, 12.0f));
        sink->AddLine(D2D1::Point2F(22.0f, 17.0f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);

        // Horizon pedestal line
        sink->BeginFigure(D2D1::Point2F(1.0f, 19.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(23.0f, 19.0f));
        sink->AddLine(D2D1::Point2F(23.0f, 21.0f));
        sink->AddLine(D2D1::Point2F(1.0f, 21.0f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    } else if (name == "toggle_eye_on") {
        // Human eye outline with central iris
        sink->BeginFigure(D2D1::Point2F(1.0f, 12.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(6.0f, 6.0f));
        sink->AddLine(D2D1::Point2F(12.0f, 4.0f));
        sink->AddLine(D2D1::Point2F(18.0f, 6.0f));
        sink->AddLine(D2D1::Point2F(23.0f, 12.0f));
        sink->AddLine(D2D1::Point2F(18.0f, 18.0f));
        sink->AddLine(D2D1::Point2F(12.0f, 20.0f));
        sink->AddLine(D2D1::Point2F(6.0f, 18.0f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);

        // Pupil / Iris cutout
        sink->BeginFigure(D2D1::Point2F(12.0f, 8.5f), D2D1_FIGURE_BEGIN_HOLLOW);
        sink->AddLine(D2D1::Point2F(15.5f, 12.0f));
        sink->AddLine(D2D1::Point2F(12.0f, 15.5f));
        sink->AddLine(D2D1::Point2F(8.5f, 12.0f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    } else if (name == "toggle_eye_off") {
        // Eye with diagonal slash
        sink->BeginFigure(D2D1::Point2F(3.0f, 8.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(12.0f, 5.0f));
        sink->AddLine(D2D1::Point2F(21.0f, 8.0f));
        sink->AddLine(D2D1::Point2F(19.0f, 10.0f));
        sink->AddLine(D2D1::Point2F(12.0f, 7.5f));
        sink->AddLine(D2D1::Point2F(5.0f, 10.0f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);

        sink->BeginFigure(D2D1::Point2F(3.0f, 16.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(5.0f, 14.0f));
        sink->AddLine(D2D1::Point2F(12.0f, 16.5f));
        sink->AddLine(D2D1::Point2F(19.0f, 14.0f));
        sink->AddLine(D2D1::Point2F(21.0f, 16.0f));
        sink->AddLine(D2D1::Point2F(12.0f, 19.0f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);

        // Diagonal slash
        sink->BeginFigure(D2D1::Point2F(2.0f, 3.5f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(3.5f, 2.0f));
        sink->AddLine(D2D1::Point2F(22.0f, 20.5f));
        sink->AddLine(D2D1::Point2F(20.5f, 22.0f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    } else if (name == "toggle_lock_on") {
        // Closed padlock: Shackle + Body
        sink->BeginFigure(D2D1::Point2F(7.0f, 10.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(7.0f, 5.0f));
        sink->AddLine(D2D1::Point2F(12.0f, 2.0f));
        sink->AddLine(D2D1::Point2F(17.0f, 5.0f));
        sink->AddLine(D2D1::Point2F(17.0f, 10.0f));
        sink->AddLine(D2D1::Point2F(14.0f, 10.0f));
        sink->AddLine(D2D1::Point2F(14.0f, 6.0f));
        sink->AddLine(D2D1::Point2F(12.0f, 4.5f));
        sink->AddLine(D2D1::Point2F(10.0f, 6.0f));
        sink->AddLine(D2D1::Point2F(10.0f, 10.0f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);

        sink->BeginFigure(D2D1::Point2F(5.0f, 10.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(19.0f, 10.0f));
        sink->AddLine(D2D1::Point2F(19.0f, 21.0f));
        sink->AddLine(D2D1::Point2F(5.0f, 21.0f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);

        // Keyhole
        sink->BeginFigure(D2D1::Point2F(11.0f, 13.0f), D2D1_FIGURE_BEGIN_HOLLOW);
        sink->AddLine(D2D1::Point2F(13.0f, 13.0f));
        sink->AddLine(D2D1::Point2F(13.0f, 18.0f));
        sink->AddLine(D2D1::Point2F(11.0f, 18.0f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    } else if (name == "toggle_lock_off") {
        // Open padlock: Raised shackle + Body
        sink->BeginFigure(D2D1::Point2F(7.0f, 8.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(7.0f, 3.0f));
        sink->AddLine(D2D1::Point2F(12.0f, 0.5f));
        sink->AddLine(D2D1::Point2F(17.0f, 3.0f));
        sink->AddLine(D2D1::Point2F(17.0f, 6.0f));
        sink->AddLine(D2D1::Point2F(14.0f, 6.0f));
        sink->AddLine(D2D1::Point2F(14.0f, 4.0f));
        sink->AddLine(D2D1::Point2F(12.0f, 2.5f));
        sink->AddLine(D2D1::Point2F(10.0f, 4.0f));
        sink->AddLine(D2D1::Point2F(10.0f, 8.0f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);

        sink->BeginFigure(D2D1::Point2F(5.0f, 10.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(19.0f, 10.0f));
        sink->AddLine(D2D1::Point2F(19.0f, 21.0f));
        sink->AddLine(D2D1::Point2F(5.0f, 21.0f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    } else if (name == "toggle_render_off") {
        // Camera with diagonal slash (Render disabled)
        sink->BeginFigure(D2D1::Point2F(2.0f, 7.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(14.0f, 7.0f));
        sink->AddLine(D2D1::Point2F(14.0f, 19.0f));
        sink->AddLine(D2D1::Point2F(2.0f, 19.0f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);

        sink->BeginFigure(D2D1::Point2F(14.0f, 10.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(22.0f, 6.0f));
        sink->AddLine(D2D1::Point2F(22.0f, 18.0f));
        sink->AddLine(D2D1::Point2F(14.0f, 14.0f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);

        // Diagonal slash
        sink->BeginFigure(D2D1::Point2F(2.0f, 3.5f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(3.5f, 2.0f));
        sink->AddLine(D2D1::Point2F(22.0f, 20.5f));
        sink->AddLine(D2D1::Point2F(20.5f, 22.0f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    } else if (name == "search") {
        // Magnifying glass: Lens ring + handle
        sink->BeginFigure(D2D1::Point2F(10.0f, 2.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(16.0f, 4.0f));
        sink->AddLine(D2D1::Point2F(18.0f, 10.0f));
        sink->AddLine(D2D1::Point2F(16.0f, 16.0f));
        sink->AddLine(D2D1::Point2F(10.0f, 18.0f));
        sink->AddLine(D2D1::Point2F(4.0f, 16.0f));
        sink->AddLine(D2D1::Point2F(2.0f, 10.0f));
        sink->AddLine(D2D1::Point2F(4.0f, 4.0f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);

        // Inner lens cutout
        sink->BeginFigure(D2D1::Point2F(10.0f, 5.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(14.5f, 7.0f));
        sink->AddLine(D2D1::Point2F(15.0f, 10.0f));
        sink->AddLine(D2D1::Point2F(14.5f, 13.0f));
        sink->AddLine(D2D1::Point2F(10.0f, 15.0f));
        sink->AddLine(D2D1::Point2F(5.5f, 13.0f));
        sink->AddLine(D2D1::Point2F(5.0f, 10.0f));
        sink->AddLine(D2D1::Point2F(5.5f, 7.0f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);

        // Handle
        sink->BeginFigure(D2D1::Point2F(14.5f, 14.5f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(21.0f, 21.0f));
        sink->AddLine(D2D1::Point2F(19.5f, 22.5f));
        sink->AddLine(D2D1::Point2F(13.0f, 16.0f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    } else if (name == "anim_play") {
        sink->BeginFigure(D2D1::Point2F(6.0f, 4.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(19.0f, 12.0f));
        sink->AddLine(D2D1::Point2F(6.0f, 20.0f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    } else if (name == "anim_play_reverse") {
        sink->BeginFigure(D2D1::Point2F(18.0f, 4.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(5.0f, 12.0f));
        sink->AddLine(D2D1::Point2F(18.0f, 20.0f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    } else if (name == "anim_keying") {
        // Key icon
        sink->BeginFigure(D2D1::Point2F(6.0f, 5.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(12.0f, 5.0f));
        sink->AddLine(D2D1::Point2F(14.0f, 7.0f));
        sink->AddLine(D2D1::Point2F(21.0f, 7.0f));
        sink->AddLine(D2D1::Point2F(21.0f, 10.0f));
        sink->AddLine(D2D1::Point2F(19.0f, 10.0f));
        sink->AddLine(D2D1::Point2F(19.0f, 13.0f));
        sink->AddLine(D2D1::Point2F(17.0f, 13.0f));
        sink->AddLine(D2D1::Point2F(17.0f, 10.0f));
        sink->AddLine(D2D1::Point2F(14.0f, 10.0f));
        sink->AddLine(D2D1::Point2F(12.0f, 12.0f));
        sink->AddLine(D2D1::Point2F(6.0f, 12.0f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    } else if (name == "toggle_sync") {
        // Sync / looping arrows: 2 circular curved arrows
        // Top arrow
        sink->BeginFigure(D2D1::Point2F(6.0f, 10.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(9.0f, 6.0f));
        sink->AddLine(D2D1::Point2F(15.0f, 6.0f));
        sink->AddLine(D2D1::Point2F(18.0f, 9.0f));
        sink->AddLine(D2D1::Point2F(20.0f, 7.0f));
        sink->AddLine(D2D1::Point2F(20.0f, 12.0f));
        sink->AddLine(D2D1::Point2F(15.0f, 12.0f));
        sink->AddLine(D2D1::Point2F(17.0f, 10.0f));
        sink->AddLine(D2D1::Point2F(14.5f, 8.0f));
        sink->AddLine(D2D1::Point2F(9.5f, 8.0f));
        sink->AddLine(D2D1::Point2F(7.5f, 10.5f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);

        // Bottom arrow
        sink->BeginFigure(D2D1::Point2F(18.0f, 14.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(15.0f, 18.0f));
        sink->AddLine(D2D1::Point2F(9.0f, 18.0f));
        sink->AddLine(D2D1::Point2F(6.0f, 15.0f));
        sink->AddLine(D2D1::Point2F(4.0f, 17.0f));
        sink->AddLine(D2D1::Point2F(4.0f, 12.0f));
        sink->AddLine(D2D1::Point2F(9.0f, 12.0f));
        sink->AddLine(D2D1::Point2F(7.0f, 14.0f));
        sink->AddLine(D2D1::Point2F(9.5f, 16.0f));
        sink->AddLine(D2D1::Point2F(14.5f, 16.0f));
        sink->AddLine(D2D1::Point2F(16.5f, 13.5f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    } else if (name == "anim_pause") {
        sink->BeginFigure(D2D1::Point2F(6.0f, 4.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(9.5f, 4.0f));
        sink->AddLine(D2D1::Point2F(9.5f, 20.0f));
        sink->AddLine(D2D1::Point2F(6.0f, 20.0f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);

        sink->BeginFigure(D2D1::Point2F(14.5f, 4.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(18.0f, 4.0f));
        sink->AddLine(D2D1::Point2F(18.0f, 20.0f));
        sink->AddLine(D2D1::Point2F(14.5f, 20.0f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    } else if (name == "anim_step_forward") {
        sink->BeginFigure(D2D1::Point2F(5.0f, 4.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(14.0f, 12.0f));
        sink->AddLine(D2D1::Point2F(5.0f, 20.0f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);

        sink->BeginFigure(D2D1::Point2F(16.0f, 4.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(19.0f, 4.0f));
        sink->AddLine(D2D1::Point2F(19.0f, 20.0f));
        sink->AddLine(D2D1::Point2F(16.0f, 20.0f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    } else if (name == "anim_step_back") {
        sink->BeginFigure(D2D1::Point2F(5.0f, 4.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(8.0f, 4.0f));
        sink->AddLine(D2D1::Point2F(8.0f, 20.0f));
        sink->AddLine(D2D1::Point2F(5.0f, 20.0f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);

        sink->BeginFigure(D2D1::Point2F(19.0f, 4.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(10.0f, 12.0f));
        sink->AddLine(D2D1::Point2F(19.0f, 20.0f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    } else if (name == "anim_jump_start") {
        sink->BeginFigure(D2D1::Point2F(4.0f, 4.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(7.0f, 4.0f));
        sink->AddLine(D2D1::Point2F(7.0f, 20.0f));
        sink->AddLine(D2D1::Point2F(4.0f, 20.0f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);

        sink->BeginFigure(D2D1::Point2F(14.0f, 4.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(7.0f, 12.0f));
        sink->AddLine(D2D1::Point2F(14.0f, 20.0f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);

        sink->BeginFigure(D2D1::Point2F(21.0f, 4.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(14.0f, 12.0f));
        sink->AddLine(D2D1::Point2F(21.0f, 20.0f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    } else if (name == "anim_jump_end") {
        sink->BeginFigure(D2D1::Point2F(3.0f, 4.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(10.0f, 12.0f));
        sink->AddLine(D2D1::Point2F(3.0f, 20.0f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);

        sink->BeginFigure(D2D1::Point2F(10.0f, 4.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(17.0f, 12.0f));
        sink->AddLine(D2D1::Point2F(10.0f, 20.0f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);

        sink->BeginFigure(D2D1::Point2F(17.0f, 4.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(20.0f, 4.0f));
        sink->AddLine(D2D1::Point2F(20.0f, 20.0f));
        sink->AddLine(D2D1::Point2F(17.0f, 20.0f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    } else if (name == "space_timeline") {
        // Timeline Clock Editor Icon (Dial Ring + Clock Hands)
        // Outer ring contour
        sink->BeginFigure(D2D1::Point2F(9.0f, 2.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(15.0f, 2.0f));
        sink->AddLine(D2D1::Point2F(20.0f, 5.0f));
        sink->AddLine(D2D1::Point2F(22.0f, 9.0f));
        sink->AddLine(D2D1::Point2F(22.0f, 15.0f));
        sink->AddLine(D2D1::Point2F(20.0f, 19.0f));
        sink->AddLine(D2D1::Point2F(15.0f, 22.0f));
        sink->AddLine(D2D1::Point2F(9.0f, 22.0f));
        sink->AddLine(D2D1::Point2F(4.0f, 19.0f));
        sink->AddLine(D2D1::Point2F(2.0f, 15.0f));
        sink->AddLine(D2D1::Point2F(2.0f, 9.0f));
        sink->AddLine(D2D1::Point2F(4.0f, 5.0f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);

        // Inner circle cutout (creates ring with alternate fill mode)
        sink->BeginFigure(D2D1::Point2F(9.5f, 4.5f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(14.5f, 4.5f));
        sink->AddLine(D2D1::Point2F(18.0f, 7.0f));
        sink->AddLine(D2D1::Point2F(19.5f, 9.5f));
        sink->AddLine(D2D1::Point2F(19.5f, 14.5f));
        sink->AddLine(D2D1::Point2F(18.0f, 17.0f));
        sink->AddLine(D2D1::Point2F(14.5f, 19.5f));
        sink->AddLine(D2D1::Point2F(9.5f, 19.5f));
        sink->AddLine(D2D1::Point2F(6.0f, 17.0f));
        sink->AddLine(D2D1::Point2F(4.5f, 14.5f));
        sink->AddLine(D2D1::Point2F(4.5f, 9.5f));
        sink->AddLine(D2D1::Point2F(6.0f, 7.0f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);

        // Clock Hands: Hour hand points up to 6, Minute hand points right to 16
        sink->BeginFigure(D2D1::Point2F(11.0f, 6.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(13.0f, 6.0f));
        sink->AddLine(D2D1::Point2F(13.0f, 11.0f));
        sink->AddLine(D2D1::Point2F(16.5f, 11.0f));
        sink->AddLine(D2D1::Point2F(16.5f, 13.0f));
        sink->AddLine(D2D1::Point2F(11.0f, 13.0f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    } else if (name == "channel_summary") {
        // Dope Sheet Action Summary: Action Bars
        sink->BeginFigure(D2D1::Point2F(3.0f, 5.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(21.0f, 5.0f));
        sink->AddLine(D2D1::Point2F(21.0f, 8.0f));
        sink->AddLine(D2D1::Point2F(3.0f, 8.0f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);

        sink->BeginFigure(D2D1::Point2F(3.0f, 10.5f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(21.0f, 10.5f));
        sink->AddLine(D2D1::Point2F(21.0f, 13.5f));
        sink->AddLine(D2D1::Point2F(3.0f, 13.5f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);

        sink->BeginFigure(D2D1::Point2F(3.0f, 16.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(21.0f, 16.0f));
        sink->AddLine(D2D1::Point2F(21.0f, 19.0f));
        sink->AddLine(D2D1::Point2F(3.0f, 19.0f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    } else if (name == "channel_fcurve") {
        // F-Curve Bezier waveform
        sink->BeginFigure(D2D1::Point2F(3.0f, 18.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(6.0f, 18.0f));
        sink->AddLine(D2D1::Point2F(10.0f, 13.0f));
        sink->AddLine(D2D1::Point2F(14.0f, 11.0f));
        sink->AddLine(D2D1::Point2F(18.0f, 6.0f));
        sink->AddLine(D2D1::Point2F(21.0f, 6.0f));
        sink->AddLine(D2D1::Point2F(21.0f, 9.0f));
        sink->AddLine(D2D1::Point2F(17.5f, 9.0f));
        sink->AddLine(D2D1::Point2F(13.5f, 14.0f));
        sink->AddLine(D2D1::Point2F(9.5f, 16.0f));
        sink->AddLine(D2D1::Point2F(5.5f, 21.0f));
        sink->AddLine(D2D1::Point2F(3.0f, 21.0f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    } else if (name == "anim_keyframe") {
        sink->BeginFigure(D2D1::Point2F(12.0f, 3.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(21.0f, 12.0f));
        sink->AddLine(D2D1::Point2F(12.0f, 21.0f));
        sink->AddLine(D2D1::Point2F(3.0f, 12.0f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    } else if (name == "anim_autokey") {
        // Record Circle (crisp round record disc)
        sink->BeginFigure(D2D1::Point2F(9.0f, 4.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(15.0f, 4.0f));
        sink->AddLine(D2D1::Point2F(20.0f, 9.0f));
        sink->AddLine(D2D1::Point2F(20.0f, 15.0f));
        sink->AddLine(D2D1::Point2F(15.0f, 20.0f));
        sink->AddLine(D2D1::Point2F(9.0f, 20.0f));
        sink->AddLine(D2D1::Point2F(4.0f, 15.0f));
        sink->AddLine(D2D1::Point2F(4.0f, 9.0f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    } else {
        // Default clean circular dot (round disc, never a diamond)
        sink->BeginFigure(D2D1::Point2F(10.0f, 8.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(14.0f, 8.0f));
        sink->AddLine(D2D1::Point2F(16.0f, 10.0f));
        sink->AddLine(D2D1::Point2F(16.0f, 14.0f));
        sink->AddLine(D2D1::Point2F(14.0f, 16.0f));
        sink->AddLine(D2D1::Point2F(10.0f, 16.0f));
        sink->AddLine(D2D1::Point2F(8.0f, 14.0f));
        sink->AddLine(D2D1::Point2F(8.0f, 10.0f));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    }

    sink->Close();
    s_geometries[name] = geom;
    return geom.Get();
}

void IconRenderer::clear_cache() {
    s_geometries.clear();
    s_svg_documents.clear();
    s_icon_paths.clear();
    s_missing_icon_paths.clear();
    s_svg_context.Reset();
}

void IconRenderer::draw_icon(Graphics &gfx, const std::string &name, const Rect &dest, Color color) {
    if (draw_svg_icon(gfx, name, dest, color)) return;
    ID2D1DeviceContext *d2d = gfx.get_context();
    ID2D1Factory1 *factory = gfx.get_factory();
    if (!d2d || !factory) return;

    ID2D1PathGeometry *geom = get_geometry(factory, name);
    if (!geom) return;

    auto* brush = gfx.get_brush(color);
    if (!brush) return;

    float scale = std::min(dest.width, dest.height) / 24.0f;
    float offset_x = dest.x + (dest.width - 24.0f * scale) * 0.5f;
    float offset_y = dest.y + (dest.height - 24.0f * scale) * 0.5f;

    D2D1_MATRIX_3X2_F old_transform;
    d2d->GetTransform(&old_transform);

    D2D1_MATRIX_3X2_F m = D2D1::Matrix3x2F::Scale(scale, scale) *
                          D2D1::Matrix3x2F::Translation(offset_x, offset_y) *
                          old_transform;
    d2d->SetTransform(m);
    d2d->FillGeometry(geom, brush);
    d2d->SetTransform(old_transform);
}

} // namespace medusa
