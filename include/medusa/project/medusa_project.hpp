#pragma once

#include <medusa/medusa_editor.hpp>
#include <string>

namespace medusa {

bool save_medusa_project(const EditorState& state, const std::string& path, std::string& error);
bool load_medusa_project(const std::string& path, EditorState& state, std::string& error);
std::string medusa_project_snapshot(const EditorState& state);
bool restore_medusa_project_snapshot(const std::string& snapshot, EditorState& state, std::string& error);
std::string create_medusa_stable_id();
void ensure_medusa_document_identity(EditorState& state);

bool save_medusa_recovery(const EditorState& state, const std::string& recoveryPath,
                          const std::string& sourceProjectPath, std::string& error);
bool load_medusa_recovery(const std::string& recoveryPath, EditorState& state,
                          std::string& sourceProjectPath, std::string& error);

} // namespace medusa
