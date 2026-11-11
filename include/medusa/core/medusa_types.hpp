#pragma once
#include <cstdint>
#include <string>
#include <algorithm>
#include <cmath>

namespace medusa {

struct Point {
    float x = 0.0f;
    float y = 0.0f;
    constexpr Point() = default;
    constexpr Point(float x, float y) : x(x), y(y) {}
};

struct Rect {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;

    constexpr Rect() = default;
    constexpr Rect(float x, float y, float width, float height)
        : x(x), y(y), width(width), height(height) {}

    constexpr float right() const { return x + width; }
    constexpr float bottom() const { return y + height; }
    constexpr bool contains(float px, float py) const {
        return px >= x && px <= right() && py >= y && py <= bottom();
    }
    constexpr bool contains(Point p) const { return contains(p.x, p.y); }
};

struct Color {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 1.0f;

    constexpr Color() = default;
    constexpr Color(float r, float g, float b, float a = 1.0f)
        : r(r), g(g), b(b), a(a) {}

    static constexpr Color from_rgb_u8(uint8_t red, uint8_t green, uint8_t blue, float alpha = 1.0f) {
        return Color(red / 255.0f, green / 255.0f, blue / 255.0f, alpha);
    }
    static constexpr Color from_hex(uint32_t hex, float alpha = 1.0f) {
        return from_rgb_u8((hex >> 16) & 0xFF, (hex >> 8) & 0xFF, hex & 0xFF, alpha);
    }
};

enum class CursorType {
    Arrow,
    SizeWE,
    SizeNS
};

enum class ViewportShadingMode : uint8_t {
    Wireframe = 0,
    Solid = 1,
    Rendered = 2
};

enum class MouseButton {
    None,
    Left,
    Middle,
    Right
};

struct MouseEvent {
    Point pos;
    Point delta;
    MouseButton button = MouseButton::None;
    bool shift = false;
    bool ctrl = false;
    bool alt = false;
};

struct KeyEvent {
    uint32_t key_code = 0;
    bool shift = false;
    bool ctrl = false;
    bool alt = false;
};

} // namespace medusa
