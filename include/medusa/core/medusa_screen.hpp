#pragma once
#include <medusa/medusa_types.hpp>
#include <medusa/medusa_graphics.hpp>
#include <vector>
#include <memory>
#include <string>
#include <functional>

namespace medusa {

enum class SplitType {
    None,
    Horizontal,
    Vertical
};

class Region {
public:
    virtual ~Region() = default;
    virtual void layout(const Rect &bounds) { bounds_ = bounds; }
    virtual void draw(Graphics &gfx) = 0;
    virtual bool accepts_input(Point p) const { return bounds_.contains(p); }
    virtual Rect clip_bounds() const { return bounds_; }

    virtual bool handle_mouse_down(const MouseEvent &e) { return false; }
    virtual bool handle_mouse_move(const MouseEvent &e) { return false; }
    virtual bool handle_mouse_up(const MouseEvent &e) { return false; }
    virtual bool handle_mouse_wheel(float delta, Point p) { return false; }
    virtual bool handle_raw_mouse_delta(int deltaX, int deltaY) { return false; }

    const Rect& get_bounds() const { return bounds_; }
    bool is_hit(Point p) const { return bounds_.contains(p); }

protected:
    Rect bounds_;
};

class Area {
public:
    Area(const std::string &title = "");
    virtual ~Area() = default;

    void set_bounds(const Rect &r);
    const Rect& get_bounds() const { return bounds_; }

    void add_region(std::shared_ptr<Region> reg) { regions_.push_back(reg); }

    virtual void layout(const Rect &bounds);
    virtual void draw(Graphics &gfx);

    virtual bool handle_mouse_down(const MouseEvent &e);
    virtual bool handle_mouse_move(const MouseEvent &e);
    virtual bool handle_mouse_up(const MouseEvent &e);
    virtual bool handle_mouse_wheel(float delta, Point p);
    virtual bool handle_raw_mouse_delta(int deltaX, int deltaY);

protected:
    std::string title_;
    Rect bounds_;
    std::vector<std::shared_ptr<Region>> regions_;
};

class Screen {
public:
    Screen();
    virtual ~Screen() = default;

    void set_size(float width, float height);
    void add_area(std::shared_ptr<Area> area) { areas_.push_back(area); }
    void clear_areas() { areas_.clear(); }

    virtual void draw(Graphics &gfx);

    virtual bool handle_mouse_down(const MouseEvent &e);
    virtual bool handle_mouse_move(const MouseEvent &e);
    virtual bool handle_mouse_up(const MouseEvent &e);
    virtual bool handle_mouse_wheel(float delta, Point p);
    virtual bool handle_raw_mouse_delta(int deltaX, int deltaY);

    float width() const { return width_; }
    float height() const { return height_; }

protected:
    float width_ = 1600.0f;
    float height_ = 900.0f;
    std::vector<std::shared_ptr<Area>> areas_;
    std::shared_ptr<Area> captured_area_;
};

} // namespace medusa
