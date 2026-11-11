#pragma once
#include <medusa/medusa_graphics.hpp>
#include <string>

namespace medusa {

class IconRenderer {
public:
    static void draw_icon(Graphics &gfx, const std::string &name, const Rect &dest, Color color);
    static void clear_cache();

private:
    static ID2D1PathGeometry* get_geometry(ID2D1Factory1 *factory, const std::string &name);
};

} // namespace medusa
