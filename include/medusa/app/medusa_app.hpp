#pragma once
#include <medusa/medusa_window.hpp>
#include <memory>

namespace medusa {

class MedusaApp {
public:
    MedusaApp();
    ~MedusaApp() = default;

    int run(int argc, char* argv[]);

    Window& get_window() { return window_; }

private:
    Window window_;
};

} // namespace medusa
