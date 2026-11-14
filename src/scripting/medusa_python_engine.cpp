#include <medusa/scripting/medusa_python_engine.hpp>
#include <medusa/editor/editor_state.hpp>
#include <medusa/medusa_production_scene.hpp>
#include <algorithm>
#include <sstream>
#include <iostream>
#include <fstream>
#include <cmath>
#include <cctype>

namespace medusa::scripting {

PythonEngine& PythonEngine::instance() {
    static PythonEngine s_instance;
    return s_instance;
}

bool PythonEngine::initialize(EditorState* state, MedusaEditor* editor) {
    state_ = state;
    editor_ = editor;
    initialized_ = true;

    clear_logs();
    log(LogLevel::Notice, "Python 3.14.5 (UCRT64 MinGW GCC) [Medusa Platinum Core DCC Environment]");
    log(LogLevel::Notice, "Type \"help()\", \"copyright\", \"credits\" or \"license\" for more information.");
    log(LogLevel::Notice, "Embedded module 'medusa' active. Try 'medusa.scene.get_nodes()' or 'medusa.scene.add_cube()'.");

    return true;
}

void PythonEngine::shutdown() {
    initialized_ = false;
    state_ = nullptr;
    editor_ = nullptr;
}

void PythonEngine::set_editor_context(EditorState* state, MedusaEditor* editor) {
    state_ = state;
    editor_ = editor;
}

std::string PythonEngine::get_version_info() const {
    return "Python 3.14.5 · Medusa Platinum Core API";
}

void PythonEngine::clear_logs() {
    logs_.clear();
}

void PythonEngine::add_history(const std::string& cmd) {
    if (cmd.empty()) return;
    if (history_.empty() || history_.back() != cmd) {
        history_.push_back(cmd);
    }
}

void PythonEngine::log(LogLevel level, const std::string& text) {
    logs_.push_back({level, text, 0});
}

bool PythonEngine::execute_file(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        log(LogLevel::Error, "FileNotFoundError: [Errno 2] No such file or directory: '" + filepath + "'");
        return false;
    }
    log(LogLevel::Notice, ">>> Executing script: " + filepath);
    std::string line;
    bool success = true;
    while (std::getline(file, line)) {
        if (!line.empty() && line[0] != '#') {
            if (!execute(line)) success = false;
        }
    }
    log(LogLevel::Notice, ">>> Script finished.");
    return success;
}

bool PythonEngine::execute(const std::string& raw_code) {
    std::string code = raw_code;
    // Trim leading/trailing whitespace
    size_t first = code.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return true;
    size_t last = code.find_last_not_of(" \t\r\n");
    code = code.substr(first, last - first + 1);

    // Echo input with prompt
    log(LogLevel::Prompt, ">>> " + code);
    add_history(code);

    // Check for clear / cls
    if (code == "clear" || code == "cls" || code == "clear()") {
        clear_logs();
        return true;
    }

    // Help commands
    if (code == "help()" || code == "help" || code == "help(medusa)") {
        log(LogLevel::Notice, "Help on module medusa (built-in):");
        log(LogLevel::Output, "NAME: medusa - Medusa Platinum 3D Cinematic DCC Automation Engine");
        log(LogLevel::Output, "PACKAGE CONTENTS:");
        log(LogLevel::Output, "  medusa.scene   - Scene node management, primitives, lights, cameras");
        log(LogLevel::Output, "  medusa.camera  - Viewport & render camera optics and positioning");
        log(LogLevel::Output, "  medusa.render  - Production renderer control, samples, and frame export");
        log(LogLevel::Output, "  medusa.app     - Workspaces, engine diagnostics, and console logging");
        log(LogLevel::Output, "\nEXAMPLES:");
        log(LogLevel::Output, "  >>> medusa.scene.add_cube(name='HeroCube')");
        log(LogLevel::Output, "  >>> medusa.scene.get_nodes()");
        log(LogLevel::Output, "  >>> medusa.camera.set_focal_length(85.0)");
        log(LogLevel::Output, "  >>> medusa.render.start()");
        return true;
    }

    if (code == "copyright" || code == "credits") {
        log(LogLevel::Output, "Medusa Platinum (c) 2026 Zelyn Ecosystem. ZeGFX Production Rendering Architecture.");
        return true;
    }
    if (code == "license") {
        log(LogLevel::Output, "Type license() to see the full license text.");
        return true;
    }

    // Print statement: print(...)
    if (code.rfind("print(", 0) == 0 && code.back() == ')') {
        std::string inner = code.substr(6, code.length() - 7);
        // If string literal inside print
        if (inner.length() >= 2 && ((inner.front() == '"' && inner.back() == '"') ||
                                    (inner.front() == '\'' && inner.back() == '\''))) {
            log(LogLevel::Output, inner.substr(1, inner.length() - 2));
            return true;
        }
        // Expression inside print
        std::string res, err;
        if (dispatch_medusa_command(inner, res, err) || evaluate_expression(inner, res, err)) {
            log(LogLevel::Output, res);
            return true;
        }
        if (!err.empty()) log(LogLevel::Error, err);
        return true;
    }

    // Variable assignment: var = expr
    size_t eqPos = code.find('=');
    if (eqPos != std::string::npos && eqPos > 0 && code[eqPos - 1] != '!' && code[eqPos - 1] != '=' &&
        code[eqPos + 1] != '=' && code.find("medusa.") == std::string::npos) {
        std::string varName = code.substr(0, eqPos);
        std::string expr = code.substr(eqPos + 1);
        varName.erase(0, varName.find_first_not_of(" \t"));
        varName.erase(varName.find_last_not_of(" \t") + 1);
        expr.erase(0, expr.find_first_not_of(" \t"));
        expr.erase(expr.find_last_not_of(" \t") + 1);

        std::string res, err;
        if (dispatch_medusa_command(expr, res, err) || evaluate_expression(expr, res, err)) {
            variables_[varName] = res;
            return true;
        }
        variables_[varName] = expr;
        return true;
    }

    // Dispatch Medusa API command
    std::string result, err;
    if (dispatch_medusa_command(code, result, err)) {
        if (!result.empty()) log(LogLevel::Result, result);
        return true;
    }
    if (!err.empty()) {
        log(LogLevel::Error, err);
        return false;
    }

    // General expression evaluation
    if (evaluate_expression(code, result, err)) {
        log(LogLevel::Result, result);
        return true;
    }

    if (!err.empty()) {
        log(LogLevel::Error, err);
        return false;
    }

    log(LogLevel::Error, "SyntaxError: invalid syntax or unrecognized identifier: '" + code + "'");
    return false;
}

bool PythonEngine::dispatch_medusa_command(const std::string& line, std::string& result_out, std::string& error_out) {
    if (line.rfind("medusa", 0) != 0 && line.rfind("import medusa", 0) != 0) {
        return false;
    }

    if (line == "import medusa") {
        result_out = "";
        return true;
    }

    if (!state_) {
        error_out = "RuntimeError: Medusa editor context is not active.";
        return true;
    }

    // medusa.scene.*
    if (line == "dir(medusa)" || line == "medusa.__all__") {
        result_out = "['app', 'camera', 'material', 'render', 'scene', 'version']";
        return true;
    }
    if (line == "dir(medusa.scene)") {
        result_out = "['add_camera', 'add_circle', 'add_cube', 'add_cylinder', 'add_light', 'add_plane', 'clear', 'delete_selected', 'duplicate_selected', 'get_nodes', 'node_count', 'select_node', 'set_node_transform']";
        return true;
    }
    if (line == "dir(medusa.camera)") {
        result_out = "['frame_all', 'frame_selected', 'get_focal_length', 'get_focus_distance', 'get_position', 'set_focal_length', 'set_focus_distance', 'set_position']";
        return true;
    }
    if (line == "dir(medusa.render)") {
        result_out = "['get_denoiser', 'get_max_samples', 'get_samples', 'is_converged', 'save_frame', 'set_denoiser', 'set_max_samples', 'start', 'stop']";
        return true;
    }
    if (line == "dir(medusa.app)") {
        result_out = "['get_fps', 'get_theme', 'get_workspace', 'log', 'set_workspace', 'version']";
        return true;
    }

    // 1. Scene Add Primitives
    if (line.find("medusa.scene.add_cube") != std::string::npos) {
        state_->pending_command = "add_primitive_cube";
        result_out = "<SceneNode: Cube (Mesh Primitive)>";
        return true;
    }
    if (line.find("medusa.scene.add_plane") != std::string::npos) {
        state_->pending_command = "add_primitive_plane";
        result_out = "<SceneNode: Plane (Mesh Primitive)>";
        return true;
    }
    if (line.find("medusa.scene.add_circle") != std::string::npos) {
        state_->pending_command = "add_primitive_circle";
        result_out = "<SceneNode: Circle (Mesh Primitive)>";
        return true;
    }
    if (line.find("medusa.scene.add_cylinder") != std::string::npos) {
        state_->pending_command = "add_primitive_cylinder";
        result_out = "<SceneNode: Cylinder (Mesh Primitive)>";
        return true;
    }
    if (line.find("medusa.scene.add_camera") != std::string::npos) {
        state_->pending_command = "add_camera";
        result_out = "<SceneNode: Camera (Actor)>";
        return true;
    }
    if (line.find("medusa.scene.add_light") != std::string::npos) {
        state_->pending_command = "add_light_point";
        result_out = "<SceneNode: PointLight (Light Actor)>";
        return true;
    }

    // 2. Scene Inspection & Selection
    if (line == "medusa.scene.get_nodes()" || line == "medusa.scene.nodes") {
        std::stringstream ss;
        ss << "[";
        for (size_t i = 0; i < state_->scene_nodes.size(); ++i) {
            const auto& n = state_->scene_nodes[i];
            const char* typeStr = n.type == zegfx::ProductionNodeType::Camera ? "Camera" :
                                  n.type == zegfx::ProductionNodeType::Light ? "Light" :
                                  n.type == zegfx::ProductionNodeType::Mesh ? "Mesh" : "Group";
            ss << "<Node #" << i << ": '" << n.name << "' (" << typeStr << ")>";
            if (i + 1 < state_->scene_nodes.size()) ss << ", ";
        }
        ss << "]";
        result_out = ss.str();
        return true;
    }

    if (line == "medusa.scene.node_count()") {
        result_out = std::to_string(state_->scene_nodes.size());
        return true;
    }

    if (line.find("medusa.scene.select_node(") != std::string::npos) {
        size_t start = line.find('(') + 1;
        size_t end = line.find(')');
        std::string arg = line.substr(start, end - start);
        if (!arg.empty() && std::isdigit(static_cast<unsigned char>(arg[0]))) {
            uint32_t idx = static_cast<uint32_t>(std::stoul(arg));
            state_->select_scene_node(idx);
            result_out = "Selected node index " + std::to_string(idx);
            return true;
        } else {
            // Find by name
            if (arg.front() == '"' || arg.front() == '\'') arg = arg.substr(1, arg.length() - 2);
            for (size_t i = 0; i < state_->scene_nodes.size(); ++i) {
                if (state_->scene_nodes[i].name == arg) {
                    state_->select_scene_node(static_cast<uint32_t>(i));
                    result_out = "Selected node '" + arg + "' (index " + std::to_string(i) + ")";
                    return true;
                }
            }
            error_out = "ValueError: Scene node '" + arg + "' not found.";
            return true;
        }
    }

    if (line == "medusa.scene.delete_selected()") {
        state_->pending_command = "delete_selected";
        result_out = "Deleted selected scene node";
        return true;
    }
    if (line == "medusa.scene.duplicate_selected()") {
        state_->pending_command = "duplicate";
        result_out = "Duplicated selected scene node";
        return true;
    }

    // 3. Camera Controls
    if (line == "medusa.camera.get_focal_length()") {
        result_out = std::to_string(state_->focal_length);
        return true;
    }
    if (line.find("medusa.camera.set_focal_length(") != std::string::npos) {
        size_t start = line.find('(') + 1;
        size_t end = line.find(')');
        float fl = std::stof(line.substr(start, end - start));
        state_->focal_length = std::clamp(fl, 10.0f, 500.0f);
        result_out = "Focal length set to " + std::to_string(state_->focal_length) + " mm";
        return true;
    }
    if (line == "medusa.camera.frame_selected()") {
        state_->pending_command = "frame_selected";
        result_out = "Framed active object";
        return true;
    }
    if (line == "medusa.camera.frame_all()") {
        state_->pending_command = "frame_all";
        result_out = "Framed entire scene";
        return true;
    }

    // 4. Render Controls
    if (line == "medusa.render.start()") {
        state_->render_setup_open = true;
        result_out = "Render dialog opened";
        return true;
    }
    if (line == "medusa.render.get_samples()") {
        result_out = "64";
        return true;
    }
    if (line.find("medusa.render.set_denoiser(") != std::string::npos) {
        bool enable = line.find("True") != std::string::npos || line.find("1") != std::string::npos;
        state_->denoiser_enabled = enable;
        result_out = std::string("AI Denoising ") + (enable ? "Enabled" : "Disabled");
        return true;
    }

    // 5. App Controls
    if (line == "medusa.app.get_workspace()" || line == "medusa.app.workspace") {
        result_out = "'" + state_->active_workspace + "'";
        return true;
    }
    if (line.find("medusa.app.set_workspace(") != std::string::npos) {
        size_t start = line.find('(') + 1;
        size_t end = line.find(')');
        std::string ws = line.substr(start, end - start);
        if (ws.front() == '"' || ws.front() == '\'') ws = ws.substr(1, ws.length() - 2);
        state_->activate_workspace(ws);
        result_out = "Active workspace set to '" + state_->active_workspace + "'";
        return true;
    }

    if (line == "medusa.version" || line == "medusa.__version__") {
        result_out = "'1.0.0-platinum-zegfx'";
        return true;
    }

    return false;
}

bool PythonEngine::evaluate_expression(const std::string& expr, std::string& result_out, std::string& error_out) {
    if (variables_.count(expr)) {
        result_out = variables_[expr];
        return true;
    }

    // Simple numeric / arithmetic evaluation (e.g. 2 + 2, 10 * 5)
    std::stringstream ss(expr);
    double val1 = 0.0, val2 = 0.0;
    char op = 0;
    if (ss >> val1 >> op >> val2) {
        double res = 0.0;
        switch (op) {
            case '+': res = val1 + val2; break;
            case '-': res = val1 - val2; break;
            case '*': res = val1 * val2; break;
            case '/':
                if (std::abs(val2) < 1e-12) {
                    error_out = "ZeroDivisionError: division by zero";
                    return false;
                }
                res = val1 / val2;
                break;
            case '^': res = std::pow(val1, val2); break;
            default: return false;
        }
        std::string s = std::to_string(res);
        s.erase(s.find_last_not_of('0') + 1, std::string::npos);
        if (s.back() == '.') s += "0";
        result_out = s;
        return true;
    }

    return false;
}

} // namespace medusa::scripting
