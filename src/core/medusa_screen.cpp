#include <medusa/medusa_screen.hpp>
#include <medusa/medusa_theme.hpp>

namespace medusa {

Area::Area(const std::string &title) : title_(title) {}

void Area::set_bounds(const Rect &r) {
    bounds_ = r;
    layout(bounds_);
}

void Area::layout(const Rect &bounds) {
    bounds_ = bounds;
    for (auto &reg : regions_) {
        if (reg) reg->layout(bounds_);
    }
}

void Area::draw(Graphics &gfx) {
    // Keep dock areas visually connected while giving each work region a
    // clear, recessed seam instead of outlining panels like floating cards.
    gfx.fill_rect(bounds_, Theme::kPanelBg);

    for (auto &reg : regions_) {
        if (reg) {
            gfx.push_clip(reg->clip_bounds());
            reg->draw(gfx);
            gfx.pop_clip();
        }
    }

    gfx.draw_line(Point(bounds_.right() - 1.0f, bounds_.y), Point(bounds_.right() - 1.0f, bounds_.bottom()),
                  Theme::kDockEdge, 1.0f);
    gfx.draw_line(Point(bounds_.right(), bounds_.y), Point(bounds_.right(), bounds_.bottom()),
                  Theme::kDockDivider, 1.0f);
    gfx.draw_line(Point(bounds_.x, bounds_.bottom() - 1.0f), Point(bounds_.right(), bounds_.bottom() - 1.0f),
                  Theme::kDockEdge, 1.0f);
    gfx.draw_line(Point(bounds_.x, bounds_.bottom()), Point(bounds_.right(), bounds_.bottom()),
                  Theme::kDockDivider, 1.0f);
}

bool Area::handle_mouse_down(const MouseEvent &e) {
    for (auto &reg : regions_) {
        if (reg && reg->accepts_input(e.pos)) {
            if (reg->handle_mouse_down(e)) return true;
        }
    }
    return false;
}

bool Area::handle_mouse_move(const MouseEvent &e) {
    bool changed = false;
    for (auto &reg : regions_) {
        if (reg) changed = reg->handle_mouse_move(e) || changed;
    }
    return changed;
}

bool Area::handle_mouse_up(const MouseEvent &e) {
    for (auto &reg : regions_) {
        if (reg) {
            if (reg->handle_mouse_up(e)) return true;
        }
    }
    return false;
}

bool Area::handle_mouse_wheel(float delta, Point p) {
    for (auto &reg : regions_) {
        if (reg && reg->is_hit(p)) {
            if (reg->handle_mouse_wheel(delta, p)) return true;
        }
    }
    return false;
}

bool Area::handle_raw_mouse_delta(int deltaX, int deltaY) {
    for (auto it = regions_.rbegin(); it != regions_.rend(); ++it) {
        if (*it && (*it)->handle_raw_mouse_delta(deltaX, deltaY)) return true;
    }
    return false;
}

Screen::Screen() = default;

void Screen::set_size(float width, float height) {
    width_ = width;
    height_ = height;
}

void Screen::draw(Graphics &gfx) {
    gfx.clear(Theme::kChassisBg);

    for (auto &area : areas_) {
        if (area) {
            area->draw(gfx);
        }
    }
}

bool Screen::handle_mouse_down(const MouseEvent &e) {
    for (auto it = areas_.rbegin(); it != areas_.rend(); ++it) {
        if ((*it)->get_bounds().contains(e.pos)) {
            captured_area_ = *it;
            if ((*it)->handle_mouse_down(e)) return true;
        }
    }
    return false;
}

bool Screen::handle_mouse_move(const MouseEvent &e) {
    bool changed = false;
    if (captured_area_) changed = captured_area_->handle_mouse_move(e) || changed;
    for (auto it = areas_.rbegin(); it != areas_.rend(); ++it) {
        if (!*it || *it == captured_area_) continue;
        changed = (*it)->handle_mouse_move(e) || changed;
    }
    return changed;
}

bool Screen::handle_mouse_up(const MouseEvent &e) {
    bool handled = false;
    if (captured_area_) {
        handled = captured_area_->handle_mouse_up(e);
        captured_area_.reset();
    }
    for (auto it = areas_.rbegin(); it != areas_.rend(); ++it) {
        if ((*it)->get_bounds().contains(e.pos)) {
            if ((*it)->handle_mouse_up(e)) return true;
        }
    }
    return handled;
}

bool Screen::handle_mouse_wheel(float delta, Point p) {
    for (auto it = areas_.rbegin(); it != areas_.rend(); ++it) {
        if ((*it)->get_bounds().contains(p)) {
            if ((*it)->handle_mouse_wheel(delta, p)) return true;
        }
    }
    return false;
}

bool Screen::handle_raw_mouse_delta(int deltaX, int deltaY) {
    if (captured_area_) return captured_area_->handle_raw_mouse_delta(deltaX, deltaY);
    for (auto it = areas_.rbegin(); it != areas_.rend(); ++it) {
        if (*it && (*it)->handle_raw_mouse_delta(deltaX, deltaY)) return true;
    }
    return false;
}

} // namespace medusa
