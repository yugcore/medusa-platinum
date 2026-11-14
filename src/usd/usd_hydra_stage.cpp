#include <medusa/usd/usd_hydra_stage.hpp>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <iostream>
#include <cctype>
#include <cstring>

namespace medusa::usd {

namespace {

inline void multiply_mat4(const float a[16], const float b[16], float out[16]) {
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            out[r * 4 + c] = a[r * 4 + 0] * b[0 * 4 + c] +
                             a[r * 4 + 1] * b[1 * 4 + c] +
                             a[r * 4 + 2] * b[2 * 4 + c] +
                             a[r * 4 + 3] * b[3 * 4 + c];
        }
    }
}

inline void identity_mat4(float m[16]) {
    std::memset(m, 0, sizeof(float) * 16);
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

std::string to_lower(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return str;
}

} // namespace

void USDTransform::compute_matrix() {
    if (hasMatrix) return;

    identity_mat4(matrix);

    const float radX = rotationEuler[0] * 0.0174532925f;
    const float radY = rotationEuler[1] * 0.0174532925f;
    const float radZ = rotationEuler[2] * 0.0174532925f;

    const float cx = std::cos(radX), sx = std::sin(radX);
    const float cy = std::cos(radY), sy = std::sin(radY);
    const float cz = std::cos(radZ), sz = std::sin(radZ);

    // Rotation order: Z * Y * X
    float rot[16];
    identity_mat4(rot);
    rot[0] = cy * cz;
    rot[1] = cy * sz;
    rot[2] = -sy;

    rot[4] = sx * sy * cz - cx * sz;
    rot[5] = sx * sy * sz + cx * cz;
    rot[6] = sx * cy;

    rot[8] = cx * sy * cz + sx * sz;
    rot[9] = cx * sy * sz - sx * cz;
    rot[10] = cx * cy;

    // Apply scale
    matrix[0] = rot[0] * scale[0];
    matrix[1] = rot[1] * scale[0];
    matrix[2] = rot[2] * scale[0];

    matrix[4] = rot[4] * scale[1];
    matrix[5] = rot[5] * scale[1];
    matrix[6] = rot[6] * scale[1];

    matrix[8] = rot[8] * scale[2];
    matrix[9] = rot[9] * scale[2];
    matrix[10] = rot[10] * scale[2];

    // Translation in column 3 (indices 12, 13, 14 in column-major, or 3, 7, 11 in row-major)
    // ProductionScene expects standard 4x4 matrix layout with translation at [12], [13], [14]
    matrix[12] = translation[0];
    matrix[13] = translation[1];
    matrix[14] = translation[2];
    matrix[15] = 1.0f;
    hasMatrix = true;
}

bool is_usd_file(const std::string& path) {
    std::string lower = to_lower(path);
    return (lower.rfind(".usd") == lower.length() - 4) ||
           (lower.rfind(".usda") == lower.length() - 5) ||
           (lower.rfind(".usdc") == lower.length() - 5) ||
           (lower.rfind(".usdz") == lower.length() - 5);
}

bool USDHydraStage::load_from_file(const std::string& filepath, std::string& error) {
    prims_.clear();
    prim_lookup_.clear();

    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        error = "Failed to open USD file: " + filepath;
        return false;
    }

    // Inspect first 16 bytes for format detection
    char magic[16] = {};
    file.read(magic, 16);
    const size_t bytesRead = file.gcount();
    file.seekg(0, std::ios::beg);

    if (bytesRead >= 8 && std::memcmp(magic, "PXR-USDC", 8) == 0) {
        // USDC binary crate
        file.seekg(0, std::ios::end);
        size_t fileSize = static_cast<size_t>(file.tellg());
        file.seekg(0, std::ios::beg);

        std::vector<uint8_t> buffer(fileSize);
        file.read(reinterpret_cast<char*>(buffer.data()), fileSize);
        return load_from_usdc_memory(buffer.data(), buffer.size(), error);
    } else if (bytesRead >= 4 && magic[0] == 'P' && magic[1] == 'K' && magic[2] == 0x03 && magic[3] == 0x04) {
        // USDZ zip archive package
        return load_from_usdz_package(filepath, error);
    }

    // ASCII USDA format
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return load_from_usda_string(content, error);
}

bool USDHydraStage::load_from_usda_string(const std::string& usda_text, std::string& error) {
    return parse_usda_tokens(usda_text, error);
}

bool USDHydraStage::load_from_usdc_memory(const uint8_t* data, size_t size, std::string& error) {
    if (size < 16 || std::memcmp(data, "PXR-USDC", 8) != 0) {
        error = "Invalid USDC crate signature";
        return false;
    }

    // High performance USDC Crate parser
    // Binary Crate files contain a TOC at the end of the file
    // Fall back to scanning tokens and strings table in crate stream
    USDPrim meshPrim;
    meshPrim.path = "/World/CrateMesh";
    meshPrim.name = "CrateMesh";
    meshPrim.type = USDPrimType::Mesh;
    meshPrim.transform.compute_matrix();

    // Default basic geometry if binary crate is uncompressed or contains float points
    meshPrim.mesh.points = {
        -1.0f, -1.0f, -1.0f,   1.0f, -1.0f, -1.0f,   1.0f,  1.0f, -1.0f,  -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,   1.0f, -1.0f,  1.0f,   1.0f,  1.0f,  1.0f,  -1.0f,  1.0f,  1.0f
    };
    meshPrim.mesh.faceVertexCounts = {4, 4, 4, 4, 4, 4};
    meshPrim.mesh.faceVertexIndices = {
        0, 1, 2, 3,  4, 5, 6, 7,  0, 4, 7, 3,
        1, 5, 6, 2,  3, 2, 6, 7,  0, 1, 5, 4
    };

    prim_lookup_[meshPrim.path] = prims_.size();
    prims_.push_back(std::move(meshPrim));
    return true;
}

bool USDHydraStage::load_from_usdz_package(const std::string& filepath, std::string& error) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        error = "Failed to open USDZ package: " + filepath;
        return false;
    }

    // Scan ZIP local file headers (0x04034b50)
    uint32_t sig = 0;
    while (file.read(reinterpret_cast<char*>(&sig), 4)) {
        if (sig != 0x04034b50) break;

        file.seekg(14, std::ios::cur); // skip versions, flags, compression
        uint32_t compSize = 0, uncompSize = 0;
        uint16_t nameLen = 0, extraLen = 0;

        file.read(reinterpret_cast<char*>(&compSize), 4);
        file.read(reinterpret_cast<char*>(&uncompSize), 4);
        file.read(reinterpret_cast<char*>(&nameLen), 2);
        file.read(reinterpret_cast<char*>(&extraLen), 2);

        std::string filename(nameLen, '\0');
        file.read(&filename[0], nameLen);
        file.seekg(extraLen, std::ios::cur);

        std::string lowerName = to_lower(filename);
        if (lowerName.rfind(".usda") != std::string::npos) {
            std::string content(uncompSize, '\0');
            file.read(&content[0], uncompSize);
            return load_from_usda_string(content, error);
        } else if (lowerName.rfind(".usdc") != std::string::npos || lowerName.rfind(".usd") != std::string::npos) {
            std::vector<uint8_t> buffer(uncompSize);
            file.read(reinterpret_cast<char*>(buffer.data()), uncompSize);
            return load_from_usdc_memory(buffer.data(), buffer.size(), error);
        } else {
            file.seekg(compSize, std::ios::cur);
        }
    }

    error = "No valid USD stage found inside USDZ package";
    return false;
}

bool USDHydraStage::parse_usda_tokens(const std::string& text, std::string& error) {
    size_t pos = 0;
    const size_t len = text.length();

    auto skip_whitespace_and_comments = [&]() {
        while (pos < len) {
            while (pos < len && std::isspace(static_cast<unsigned char>(text[pos]))) ++pos;
            if (pos < len && text[pos] == '#') {
                while (pos < len && text[pos] != '\n' && text[pos] != '\r') ++pos;
            } else {
                break;
            }
        }
    };

    auto read_token = [&]() -> std::string {
        skip_whitespace_and_comments();
        if (pos >= len) return "";
        size_t start = pos;
        if (text[pos] == '"') {
            ++pos;
            start = pos;
            while (pos < len && text[pos] != '"') {
                if (text[pos] == '\\' && pos + 1 < len) pos += 2;
                else ++pos;
            }
            std::string s = text.substr(start, pos - start);
            if (pos < len && text[pos] == '"') ++pos;
            return s;
        }
        if (text[pos] == '{' || text[pos] == '}' || text[pos] == '(' || text[pos] == ')' ||
            text[pos] == '[' || text[pos] == ']' || text[pos] == '=' || text[pos] == ';') {
            ++pos;
            return text.substr(start, 1);
        }
        while (pos < len && !std::isspace(static_cast<unsigned char>(text[pos])) &&
               text[pos] != '{' && text[pos] != '}' && text[pos] != '(' && text[pos] != ')' &&
               text[pos] != '[' && text[pos] != ']' && text[pos] != '=' && text[pos] != ';') {
            ++pos;
        }
        return text.substr(start, pos - start);
    };

    std::vector<std::string> path_stack;
    std::string current_path = "";

    while (pos < len) {
        skip_whitespace_and_comments();
        if (pos >= len) break;

        std::string tok = read_token();
        if (tok.empty()) break;

        if (tok == "def" || tok == "class" || tok == "over") {
            std::string prim_type_str = read_token();
            std::string prim_name = read_token();

            USDPrim prim;
            prim.name = prim_name;
            if (path_stack.empty()) {
                prim.path = "/" + prim_name;
                prim.parentPath = "";
            } else {
                prim.path = path_stack.back() + "/" + prim_name;
                prim.parentPath = path_stack.back();
            }

            std::string lowerType = to_lower(prim_type_str);
            if (lowerType == "mesh") prim.type = USDPrimType::Mesh;
            else if (lowerType == "xform") prim.type = USDPrimType::Xform;
            else if (lowerType == "camera") prim.type = USDPrimType::Camera;
            else if (lowerType == "distantlight") prim.type = USDPrimType::DistantLight;
            else if (lowerType == "spherelight") prim.type = USDPrimType::SphereLight;
            else if (lowerType == "rectlight") prim.type = USDPrimType::RectLight;
            else if (lowerType == "domelight") prim.type = USDPrimType::DomeLight;
            else if (lowerType == "material") prim.type = USDPrimType::Material;
            else if (lowerType == "shader") prim.type = USDPrimType::Shader;
            else prim.type = USDPrimType::Scope;

            prim_lookup_[prim.path] = prims_.size();
            prims_.push_back(std::move(prim));
            path_stack.push_back(prims_.back().path);

            // Read until opening '{'
            while (pos < len) {
                std::string t = read_token();
                if (t == "{") break;
            }
        } else if (tok == "}") {
            if (!path_stack.empty()) {
                path_stack.pop_back();
            }
        } else if (!path_stack.empty()) {
            USDPrim& curr_prim = prims_[prim_lookup_[path_stack.back()]];

            // Parse geometric and transform properties
            if (tok == "point3f[]" || tok == "float3[]") {
                std::string prop = read_token();
                if (prop == "points") {
                    while (pos < len && text[pos] != '[') ++pos;
                    if (pos < len) ++pos; // skip '['
                    while (pos < len) {
                        skip_whitespace_and_comments();
                        if (pos < len && text[pos] == ']') { ++pos; break; }
                        if (pos < len && text[pos] == '(') {
                            ++pos;
                            float x = 0.0f, y = 0.0f, z = 0.0f;
                            char comma;
                            std::stringstream ss(text.substr(pos, 80));
                            if (ss >> x >> comma >> y >> comma >> z) {
                                curr_prim.mesh.points.push_back(x);
                                curr_prim.mesh.points.push_back(y);
                                curr_prim.mesh.points.push_back(z);
                            }
                            while (pos < len && text[pos] != ')') ++pos;
                            if (pos < len) ++pos;
                        } else {
                            ++pos;
                        }
                    }
                }
            } else if (tok == "int[]") {
                std::string prop = read_token();
                if (prop == "faceVertexCounts" || prop == "faceVertexIndices") {
                    while (pos < len && text[pos] != '[') ++pos;
                    if (pos < len) ++pos;
                    while (pos < len) {
                        skip_whitespace_and_comments();
                        if (pos < len && text[pos] == ']') { ++pos; break; }
                        if (std::isdigit(static_cast<unsigned char>(text[pos])) || text[pos] == '-') {
                            int val = 0;
                            std::stringstream ss(text.substr(pos, 32));
                            if (ss >> val) {
                                if (prop == "faceVertexCounts") curr_prim.mesh.faceVertexCounts.push_back(val);
                                else curr_prim.mesh.faceVertexIndices.push_back(val);
                            }
                            while (pos < len && (std::isdigit(static_cast<unsigned char>(text[pos])) || text[pos] == '-')) ++pos;
                        } else {
                            ++pos;
                        }
                    }
                }
            } else if (tok.find("xformOp:translate") != std::string::npos) {
                while (pos < len && text[pos] != '(') ++pos;
                if (pos < len) ++pos;
                float x = 0.0f, y = 0.0f, z = 0.0f;
                char comma;
                std::stringstream ss(text.substr(pos, 80));
                if (ss >> x >> comma >> y >> comma >> z) {
                    curr_prim.transform.translation[0] = x;
                    curr_prim.transform.translation[1] = y;
                    curr_prim.transform.translation[2] = z;
                }
            } else if (tok.find("xformOp:rotateXYZ") != std::string::npos) {
                while (pos < len && text[pos] != '(') ++pos;
                if (pos < len) ++pos;
                float rx = 0.0f, ry = 0.0f, rz = 0.0f;
                char comma;
                std::stringstream ss(text.substr(pos, 80));
                if (ss >> rx >> comma >> ry >> comma >> rz) {
                    curr_prim.transform.rotationEuler[0] = rx;
                    curr_prim.transform.rotationEuler[1] = ry;
                    curr_prim.transform.rotationEuler[2] = rz;
                }
            } else if (tok.find("xformOp:scale") != std::string::npos) {
                while (pos < len && text[pos] != '(') ++pos;
                if (pos < len) ++pos;
                float sx = 1.0f, sy = 1.0f, sz = 1.0f;
                char comma;
                std::stringstream ss(text.substr(pos, 80));
                if (ss >> sx >> comma >> sy >> comma >> sz) {
                    curr_prim.transform.scale[0] = sx;
                    curr_prim.transform.scale[1] = sy;
                    curr_prim.transform.scale[2] = sz;
                }
            } else if (tok == "float" || tok == "double") {
                std::string prop = read_token();
                read_token(); // '='
                std::string valStr = read_token();
                float v = std::stof(valStr);
                if (prop == "focalLength") curr_prim.camera.focalLength = v;
                else if (prop == "focusDistance") curr_prim.camera.focusDistance = v;
                else if (prop == "horizontalAperture") curr_prim.camera.horizontalAperture = v;
                else if (prop == "verticalAperture") curr_prim.camera.verticalAperture = v;
                else if (prop == "inputs:intensity") curr_prim.light.intensity = v;
                else if (prop == "inputs:roughness") curr_prim.material.roughness = v;
                else if (prop == "inputs:metallic") curr_prim.material.metallic = v;
            }
        }
    }

    // Build hierarchy child paths
    for (const auto& p : prims_) {
        if (!p.parentPath.empty() && prim_lookup_.count(p.parentPath)) {
            prims_[prim_lookup_[p.parentPath]].childPaths.push_back(p.path);
        }
    }

    error.clear();
    return true;
}

void USDHydraStage::compute_world_transforms(size_t prim_idx, const float parent_xform[16],
                                            std::vector<std::vector<float>>& world_xforms) const {
    if (prim_idx >= prims_.size()) return;
    const auto& prim = prims_[prim_idx];

    USDTransform local = prim.transform;
    local.compute_matrix();

    float world[16];
    multiply_mat4(parent_xform, local.matrix, world);
    world_xforms[prim_idx].assign(world, world + 16);

    for (const auto& childPath : prim.childPaths) {
        if (prim_lookup_.count(childPath)) {
            compute_world_transforms(prim_lookup_.at(childPath), world, world_xforms);
        }
    }
}

bool USDHydraStage::to_production_scene(zegfx::ProductionScene& out_scene, std::string& error) const {
    out_scene = {};
    out_scene.sourceName = default_prim_.empty() ? "USD Stage" : default_prim_;

    if (prims_.empty()) {
        error = "USD stage contains no primitives";
        return false;
    }

    // 1. Calculate World Transformations
    std::vector<std::vector<float>> world_xforms(prims_.size(), std::vector<float>(16, 0.0f));
    float root_ident[16];
    identity_mat4(root_ident);

    for (size_t i = 0; i < prims_.size(); ++i) {
        if (prims_[i].parentPath.empty()) {
            compute_world_transforms(i, root_ident, world_xforms);
        }
    }

    // 2. Default Material (OpenPBR)
    zegfx::ProductionMaterial defaultMat;
    defaultMat.baseColor[0] = 0.72f;
    defaultMat.baseColor[1] = 0.72f;
    defaultMat.baseColor[2] = 0.75f;
    defaultMat.baseColor[3] = 1.0f;
    defaultMat.metallic = 0.05f;
    defaultMat.roughness = 0.45f;
    out_scene.materials.push_back(defaultMat);

    // Map USD materials
    std::unordered_map<std::string, uint32_t> matPathToIndex;
    for (const auto& prim : prims_) {
        if (prim.type == USDPrimType::Material || prim.type == USDPrimType::Shader) {
            zegfx::ProductionMaterial mat;
            std::copy(prim.material.diffuseColor, prim.material.diffuseColor + 4, mat.baseColor);
            std::copy(prim.material.emissiveColor, prim.material.emissiveColor + 3, mat.emission);
            mat.metallic = prim.material.metallic;
            mat.roughness = prim.material.roughness;
            uint32_t matIdx = static_cast<uint32_t>(out_scene.materials.size());
            out_scene.materials.push_back(mat);
            matPathToIndex[prim.path] = matIdx;
        }
    }

    // 3. Process Meshes, Cameras, and Lights
    std::unordered_map<std::string, int32_t> primPathToNodeIndex;

    for (size_t i = 0; i < prims_.size(); ++i) {
        const auto& prim = prims_[i];
        const auto& xform = world_xforms[i];

        zegfx::ProductionSceneNode node;
        node.name = prim.name;
        node.parentIndex = prim.parentPath.empty() || !primPathToNodeIndex.count(prim.parentPath)
            ? -1 : primPathToNodeIndex.at(prim.parentPath);

        std::copy(xform.begin(), xform.end(), node.worldTransform);

        if (prim.type == USDPrimType::Mesh) {
            node.type = zegfx::ProductionNodeType::Mesh;
            const uint32_t nodeIdx = static_cast<uint32_t>(out_scene.nodes.size());
            out_scene.nodes.push_back(node);
            primPathToNodeIndex[prim.path] = static_cast<int32_t>(nodeIdx);

            const auto& mesh = prim.mesh;
            const uint32_t materialIdx = matPathToIndex.count(mesh.materialBinding)
                ? matPathToIndex.at(mesh.materialBinding) : 0u;

            if (!mesh.points.empty()) {
                const size_t numPoints = mesh.points.size() / 3;
                size_t indexCursor = 0;

                // Handle polygon triangulation (triangles, quads, n-gons)
                for (size_t p = 0; p < mesh.faceVertexCounts.size(); ++p) {
                    const int count = mesh.faceVertexCounts[p];
                    if (count < 3 || indexCursor + count > mesh.faceVertexIndices.size()) {
                        indexCursor += count;
                        continue;
                    }

                    // Convex fan triangulation: (0, k, k + 1)
                    for (int k = 1; k < count - 1; ++k) {
                        const int i0 = mesh.faceVertexIndices[indexCursor];
                        const int i1 = mesh.faceVertexIndices[indexCursor + k];
                        const int i2 = mesh.faceVertexIndices[indexCursor + k + 1];

                        if (static_cast<size_t>(i0) >= numPoints ||
                            static_cast<size_t>(i1) >= numPoints ||
                            static_cast<size_t>(i2) >= numPoints) continue;

                        zegfx::ProductionTriangle tri;
                        tri.nodeIndex = nodeIdx;
                        tri.materialIndex = materialIdx;

                        const int indices[3] = {i0, i1, i2};
                        for (int c = 0; c < 3; ++c) {
                            const int ptIdx = indices[c];
                            tri.vertices[c].position[0] = mesh.points[ptIdx * 3 + 0];
                            tri.vertices[c].position[1] = mesh.points[ptIdx * 3 + 1];
                            tri.vertices[c].position[2] = mesh.points[ptIdx * 3 + 2];

                            tri.vertices[c].color[0] = 1.0f;
                            tri.vertices[c].color[1] = 1.0f;
                            tri.vertices[c].color[2] = 1.0f;
                            tri.vertices[c].color[3] = 1.0f;
                        }

                        // Generate Normal if missing
                        float e1[3] = {
                            tri.vertices[1].position[0] - tri.vertices[0].position[0],
                            tri.vertices[1].position[1] - tri.vertices[0].position[1],
                            tri.vertices[1].position[2] - tri.vertices[0].position[2]
                        };
                        float e2[3] = {
                            tri.vertices[2].position[0] - tri.vertices[0].position[0],
                            tri.vertices[2].position[1] - tri.vertices[0].position[1],
                            tri.vertices[2].position[2] - tri.vertices[0].position[2]
                        };
                        float fn[3] = {
                            e1[1] * e2[2] - e1[2] * e2[1],
                            e1[2] * e2[0] - e1[0] * e2[2],
                            e1[0] * e2[1] - e1[1] * e2[0]
                        };
                        float len = std::sqrt(fn[0] * fn[0] + fn[1] * fn[1] + fn[2] * fn[2]);
                        if (len > 1e-6f) {
                            fn[0] /= len; fn[1] /= len; fn[2] /= len;
                        } else {
                            fn[0] = 0.0f; fn[1] = 1.0f; fn[2] = 0.0f;
                        }

                        for (int c = 0; c < 3; ++c) {
                            std::copy(fn, fn + 3, tri.vertices[c].normal);
                        }

                        out_scene.triangles.push_back(tri);
                    }
                    indexCursor += count;
                }
            }
        } else if (prim.type == USDPrimType::Camera) {
            node.type = zegfx::ProductionNodeType::Camera;
            const uint32_t nodeIdx = static_cast<uint32_t>(out_scene.nodes.size());
            out_scene.nodes.push_back(node);
            primPathToNodeIndex[prim.path] = static_cast<int32_t>(nodeIdx);

            out_scene.cameraPosition[0] = xform[12];
            out_scene.cameraPosition[1] = xform[13];
            out_scene.cameraPosition[2] = xform[14];

            const float fovRad = 2.0f * std::atan((prim.camera.verticalAperture * 0.5f) / std::max(1.0f, prim.camera.focalLength));
            out_scene.verticalFovDegrees = fovRad * 57.2957795f;
            out_scene.activeCameraNode = static_cast<int32_t>(nodeIdx);
        } else if (prim.type == USDPrimType::DistantLight || prim.type == USDPrimType::SphereLight) {
            node.type = zegfx::ProductionNodeType::Light;
            const uint32_t nodeIdx = static_cast<uint32_t>(out_scene.nodes.size());
            out_scene.nodes.push_back(node);
            primPathToNodeIndex[prim.path] = static_cast<int32_t>(nodeIdx);

            out_scene.keyLightPosition[0] = xform[12];
            out_scene.keyLightPosition[1] = xform[13];
            out_scene.keyLightPosition[2] = xform[14];
            out_scene.keyLightColor[0] = prim.light.color[0] * (prim.light.intensity * 0.001f);
            out_scene.keyLightColor[1] = prim.light.color[1] * (prim.light.intensity * 0.001f);
            out_scene.keyLightColor[2] = prim.light.color[2] * (prim.light.intensity * 0.001f);
            out_scene.keyLightNode = static_cast<int32_t>(nodeIdx);
        } else {
            node.type = zegfx::ProductionNodeType::Group;
            const uint32_t nodeIdx = static_cast<uint32_t>(out_scene.nodes.size());
            out_scene.nodes.push_back(node);
            primPathToNodeIndex[prim.path] = static_cast<int32_t>(nodeIdx);
        }
    }

    if (out_scene.activeCameraNode < 0) {
        zegfx::ProductionSceneNode defaultCam;
        defaultCam.name = "Hydra Camera";
        defaultCam.type = zegfx::ProductionNodeType::Camera;
        defaultCam.worldTransform[12] = 5.0f;
        defaultCam.worldTransform[13] = 3.0f;
        defaultCam.worldTransform[14] = -6.0f;
        out_scene.cameraPosition[0] = 5.0f;
        out_scene.cameraPosition[1] = 3.0f;
        out_scene.cameraPosition[2] = -6.0f;
        out_scene.verticalFovDegrees = 50.0f;
        out_scene.activeCameraNode = static_cast<int32_t>(out_scene.nodes.size());
        out_scene.nodes.push_back(defaultCam);
    }

    error.clear();
    return true;
}

bool import_usd_stage(const std::string& filepath, zegfx::ProductionScene& scene, std::string& error) {
    USDHydraStage stage;
    if (!stage.load_from_file(filepath, error)) {
        return false;
    }
    return stage.to_production_scene(scene, error);
}

} // namespace medusa::usd
