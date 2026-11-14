#pragma once
#include <medusa/medusa_types.hpp>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>

namespace medusa {

struct EditorState;
class MedusaEditor;

namespace scripting {

enum class LogLevel {
    Prompt,     // Cyan: >>> or ...
    Command,    // White: user input
    Output,     // Silver: stdout print
    Result,     // Gold: expression return value
    Error,      // Coral Red: stderr or exception
    Notice      // Violet: system greetings / banners
};

struct ConsoleLogEntry {
    LogLevel level = LogLevel::Output;
    std::string text;
    uint32_t timestampMs = 0;
};

class PythonEngine {
public:
    static PythonEngine& instance();

    bool initialize(EditorState* state = nullptr, MedusaEditor* editor = nullptr);
    void shutdown();

    void set_editor_context(EditorState* state, MedusaEditor* editor);
    EditorState* get_state() const { return state_; }

    // Execute a line or block of code. Captures stdout/stderr into the log entries.
    bool execute(const std::string& code);
    bool execute_file(const std::string& filepath);

    const std::vector<ConsoleLogEntry>& get_logs() const { return logs_; }
    void clear_logs();

    const std::vector<std::string>& get_history() const { return history_; }
    void add_history(const std::string& cmd);

    bool is_initialized() const { return initialized_; }
    std::string get_version_info() const;

    // Direct output logging helper
    void log(LogLevel level, const std::string& text);

private:
    PythonEngine() = default;
    ~PythonEngine() = default;

    bool initialized_ = false;
    EditorState* state_ = nullptr;
    MedusaEditor* editor_ = nullptr;

    std::vector<ConsoleLogEntry> logs_;
    std::vector<std::string> history_;
    std::unordered_map<std::string, std::string> variables_;

    // Evaluator & Command dispatcher
    bool dispatch_medusa_command(const std::string& line, std::string& result_out, std::string& error_out);
    bool evaluate_expression(const std::string& expr, std::string& result_out, std::string& error_out);
};

} // namespace scripting
} // namespace medusa
