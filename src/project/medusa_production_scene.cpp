#include <medusa/medusa_production_scene.hpp>
#include <algorithm>
#include <utility>

namespace medusa {

bool is_builtin_production_scene(const std::string& path) {
    return path == kBuiltinCubeScenePath;
}

bool create_builtin_cube_scene(zegfx::ProductionScene& scene, std::string& error) {
    scene = {};
    scene.sourceName = "Scene";

    zegfx::ProductionSceneNode cube;
    cube.name = "Cube";
    cube.type = zegfx::ProductionNodeType::Mesh;
    scene.nodes.push_back(cube);

    zegfx::ProductionMaterial material;
    material.baseColor[0] = 0.62f;
    material.baseColor[1] = 0.62f;
    material.baseColor[2] = 0.62f;
    material.baseColor[3] = 1.0f;
    material.metallic = 0.05f;
    material.roughness = 0.48f;
    scene.materials.push_back(material);

    static constexpr float faceCorners[6][4][3] = {
        {{ 0.5f,-0.5f,-0.5f}, { 0.5f, 0.5f,-0.5f}, { 0.5f, 0.5f, 0.5f}, { 0.5f,-0.5f, 0.5f}},
        {{-0.5f,-0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f,-0.5f}, {-0.5f,-0.5f,-0.5f}},
        {{-0.5f, 0.5f,-0.5f}, {-0.5f, 0.5f, 0.5f}, { 0.5f, 0.5f, 0.5f}, { 0.5f, 0.5f,-0.5f}},
        {{-0.5f,-0.5f, 0.5f}, {-0.5f,-0.5f,-0.5f}, { 0.5f,-0.5f,-0.5f}, { 0.5f,-0.5f, 0.5f}},
        {{ 0.5f,-0.5f, 0.5f}, { 0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f}, {-0.5f,-0.5f, 0.5f}},
        {{-0.5f,-0.5f,-0.5f}, {-0.5f, 0.5f,-0.5f}, { 0.5f, 0.5f,-0.5f}, { 0.5f,-0.5f,-0.5f}}
    };
    static constexpr float faceNormals[6][3] = {
        { 1.0f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f},
        { 0.0f, 1.0f, 0.0f}, { 0.0f,-1.0f, 0.0f},
        { 0.0f, 0.0f, 1.0f}, { 0.0f, 0.0f,-1.0f}
    };
    static constexpr float faceUvs[4][2] = {
        {0.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}
    };
    static constexpr uint8_t triangles[2][3] = {{0, 1, 2}, {0, 2, 3}};
    scene.triangles.reserve(12);
    for (size_t face = 0; face < 6; ++face) {
        for (const auto& indices : triangles) {
            zegfx::ProductionTriangle triangle;
            triangle.materialIndex = 0;
            triangle.nodeIndex = 0;
            for (size_t corner = 0; corner < 3; ++corner) {
                const size_t source = indices[corner];
                auto& vertex = triangle.vertices[corner];
                std::copy(faceCorners[face][source], faceCorners[face][source] + 3, vertex.position);
                std::copy(faceNormals[face], faceNormals[face] + 3, vertex.normal);
                vertex.color[0] = vertex.color[1] = vertex.color[2] = vertex.color[3] = 1.0f;
                std::copy(faceUvs[source], faceUvs[source] + 2, vertex.uv);
            }
            scene.triangles.push_back(std::move(triangle));
        }
    }

    scene.cameraPosition[0] = 5.0f;
    scene.cameraPosition[1] = 3.0f;
    scene.cameraPosition[2] = -6.0f;
    scene.cameraTarget[0] = scene.cameraTarget[1] = scene.cameraTarget[2] = 0.0f;
    scene.cameraUp[0] = 0.0f;
    scene.cameraUp[1] = 1.0f;
    scene.cameraUp[2] = 0.0f;
    scene.verticalFovDegrees = 50.0f;
    scene.keyLightPosition[0] = 0.0f;
    scene.keyLightPosition[1] = 0.0f;
    scene.keyLightPosition[2] = 0.0f;
    scene.keyLightColor[0] = 0.0f;
    scene.keyLightColor[1] = 0.0f;
    scene.keyLightColor[2] = 0.0f;
    zegfx::ProductionSceneNode camera;
    camera.name = "Render Camera";
    camera.type = zegfx::ProductionNodeType::Camera;
    camera.worldTransform[12] = scene.cameraPosition[0];
    camera.worldTransform[13] = scene.cameraPosition[1];
    camera.worldTransform[14] = scene.cameraPosition[2];
    scene.activeCameraNode = static_cast<int32_t>(scene.nodes.size());
    scene.nodes.push_back(camera);
    scene.keyLightNode = -1;
    error.clear();
    return true;
}

bool append_builtin_primitive_node(zegfx::ProductionScene& scene, const std::string& shape,
                                   const std::string& name, uint32_t& nodeIndex, std::string& error) {
    if (shape != "plane" && shape != "cube" && shape != "circle" && shape != "cylinder") {
        error = "The requested primitive shape is not supported.";
        return false;
    }
    zegfx::ProductionSceneNode node;
    node.name = name;
    node.type = zegfx::ProductionNodeType::Mesh;
    nodeIndex = static_cast<uint32_t>(scene.nodes.size());
    scene.nodes.push_back(std::move(node));
    return append_builtin_primitive_geometry(scene, shape, nodeIndex, error);
}

bool append_builtin_primitive_geometry(zegfx::ProductionScene& scene, const std::string& shape,
                                       uint32_t nodeIndex, std::string& error) {
    if (nodeIndex >= scene.nodes.size() || scene.nodes[nodeIndex].type != zegfx::ProductionNodeType::Mesh) {
        error = "The primitive mesh node does not exist.";
        return false;
    }
    if (shape != "plane" && shape != "cube" && shape != "circle" && shape != "cylinder") {
        error = "The requested primitive shape is not supported.";
        return false;
    }
    const uint32_t materialIndex = static_cast<uint32_t>(scene.materials.size());
    zegfx::ProductionMaterial material;
    material.baseColor[0] = material.baseColor[1] = material.baseColor[2] = 0.62f;
    material.baseColor[3] = 1.0f;
    material.roughness = 0.48f;
    scene.materials.push_back(material);

    auto triangle = [&](const float a[3], const float b[3], const float c[3],
                        const float normal[3], const float uvA[2], const float uvB[2], const float uvC[2]) {
        zegfx::ProductionTriangle out;
        out.materialIndex = materialIndex;
        out.nodeIndex = static_cast<int32_t>(nodeIndex);
        const float* positions[3] = {a, b, c};
        const float* uvs[3] = {uvA, uvB, uvC};
        for (int vertex = 0; vertex < 3; ++vertex) {
            auto& destination = out.vertices[vertex];
            std::copy(positions[vertex], positions[vertex] + 3, destination.position);
            std::copy(normal, normal + 3, destination.normal);
            destination.color[0] = destination.color[1] = destination.color[2] = destination.color[3] = 1.0f;
            std::copy(uvs[vertex], uvs[vertex] + 2, destination.uv);
        }
        scene.triangles.push_back(std::move(out));
    };
    static constexpr float uv00[2] = {0.0f, 0.0f}, uv01[2] = {0.0f, 1.0f};
    static constexpr float uv10[2] = {1.0f, 0.0f}, uv11[2] = {1.0f, 1.0f};
    static constexpr float up[3] = {0.0f, 1.0f, 0.0f}, down[3] = {0.0f, -1.0f, 0.0f};

    if (shape == "plane") {
        const float p[4][3] = {{-1, 0, -1}, {-1, 0, 1}, {1, 0, 1}, {1, 0, -1}};
        triangle(p[0], p[1], p[2], up, uv00, uv01, uv11);
        triangle(p[0], p[2], p[3], up, uv00, uv11, uv10);
    } else if (shape == "cube") {
        static constexpr float faces[6][4][3] = {
            {{ .5f,-.5f,-.5f}, { .5f, .5f,-.5f}, { .5f, .5f, .5f}, { .5f,-.5f, .5f}},
            {{-.5f,-.5f, .5f}, {-.5f, .5f, .5f}, {-.5f, .5f,-.5f}, {-.5f,-.5f,-.5f}},
            {{-.5f, .5f,-.5f}, {-.5f, .5f, .5f}, { .5f, .5f, .5f}, { .5f, .5f,-.5f}},
            {{-.5f,-.5f, .5f}, {-.5f,-.5f,-.5f}, { .5f,-.5f,-.5f}, { .5f,-.5f, .5f}},
            {{ .5f,-.5f, .5f}, { .5f, .5f, .5f}, {-.5f, .5f, .5f}, {-.5f,-.5f, .5f}},
            {{-.5f,-.5f,-.5f}, {-.5f, .5f,-.5f}, { .5f, .5f,-.5f}, { .5f,-.5f,-.5f}}
        };
        static constexpr float normals[6][3] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
        for (int face = 0; face < 6; ++face) {
            triangle(faces[face][0], faces[face][1], faces[face][2], normals[face], uv00, uv01, uv11);
            triangle(faces[face][0], faces[face][2], faces[face][3], normals[face], uv00, uv11, uv10);
        }
    } else {
        constexpr int segments = 32;
        constexpr float tau = 6.2831853071795864769f;
        const float centerTop[3] = {0.0f, 0.5f, 0.0f};
        const float centerBottom[3] = {0.0f, -0.5f, 0.0f};
        for (int segment = 0; segment < segments; ++segment) {
            const float a0 = tau * static_cast<float>(segment) / segments;
            const float a1 = tau * static_cast<float>(segment + 1) / segments;
            const float x0 = std::cos(a0), z0 = std::sin(a0);
            const float x1 = std::cos(a1), z1 = std::sin(a1);
            const float ring0[3] = {x0, 0.0f, z0};
            const float ring1[3] = {x1, 0.0f, z1};
            const float uv0[2] = {static_cast<float>(segment) / segments, 0.5f};
            const float uv1[2] = {static_cast<float>(segment + 1) / segments, 0.5f};
            if (shape == "circle") {
                const float center[3] = {0.0f, 0.0f, 0.0f};
                triangle(center, ring0, ring1, up, uv00, uv0, uv1);
            } else {
                const float top0[3] = {x0, 0.5f, z0}, top1[3] = {x1, 0.5f, z1};
                const float bottom0[3] = {x0, -0.5f, z0}, bottom1[3] = {x1, -0.5f, z1};
                const float sideNormal0[3] = {x0, 0.0f, z0}, sideNormal1[3] = {x1, 0.0f, z1};
                triangle(bottom0, top0, top1, sideNormal0, uv00, uv01, uv11);
                triangle(bottom0, top1, bottom1, sideNormal1, uv00, uv11, uv10);
                triangle(centerTop, top1, top0, up, uv00, uv1, uv0);
                triangle(centerBottom, bottom0, bottom1, down, uv00, uv0, uv1);
            }
        }
    }
    error.clear();
    return true;
}

} // namespace medusa

#if defined(MEDUSA_UI_ONLY_BUILD)

namespace medusa {

bool load_production_scene(const std::string& path, zegfx::ProductionScene& scene,
                           std::string& error) {
    if (is_builtin_production_scene(path)) return create_builtin_cube_scene(scene, error);
    scene = {};
    error = "Asset import is unavailable in this UI-only fallback build.";
    return false;
}

} // namespace medusa

#else

#include "zegfx/assets/asset_importer.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <filesystem>

using Microsoft::WRL::ComPtr;

namespace medusa {
namespace {
bool decode_texture(const zegfx::asset::Texture& source, IWICImagingFactory* factory,
                    zegfx::ProductionTexture& destination, std::string& error) {
    ComPtr<IWICBitmapDecoder> decoder;
    HRESULT hr = E_FAIL;
    ComPtr<IWICStream> stream;
    if (!source.embeddedData.empty()) {
        if (source.embeddedData.size() > std::numeric_limits<DWORD>::max()) {
            error = "An embedded texture exceeds the Windows decoder limit.";
            return false;
        }
        hr = factory->CreateStream(stream.GetAddressOf());
        if (SUCCEEDED(hr)) {
            hr = stream->InitializeFromMemory(const_cast<BYTE*>(source.embeddedData.data()),
                                               static_cast<DWORD>(source.embeddedData.size()));
        }
        if (SUCCEEDED(hr))
            hr = factory->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnLoad,
                                                  decoder.GetAddressOf());
    } else if (!source.path.empty()) {
        const std::wstring texturePath = std::filesystem::u8path(source.path).wstring();
        hr = factory->CreateDecoderFromFilename(texturePath.c_str(), nullptr, GENERIC_READ,
                                                WICDecodeMetadataCacheOnLoad, decoder.GetAddressOf());
    }
    if (FAILED(hr)) {
        error = source.path.empty()
            ? "The imported texture has no embedded data or external file path."
            : "Windows Imaging Component could not open texture: " + source.path;
        return false;
    }
    ComPtr<IWICBitmapFrameDecode> frame;
    if (FAILED(decoder->GetFrame(0, frame.GetAddressOf()))) {
        error = "Windows Imaging Component could not decode texture: " + source.path;
        return false;
    }
    UINT sourceWidth = 0, sourceHeight = 0;
    frame->GetSize(&sourceWidth, &sourceHeight);
    if (!sourceWidth || !sourceHeight) { error = "An imported texture has invalid dimensions."; return false; }
    const float scale = std::min(1.0f, 2048.0f / static_cast<float>(std::max(sourceWidth, sourceHeight)));
    destination.width = std::max(1u, static_cast<UINT>(std::lround(sourceWidth * scale)));
    destination.height = std::max(1u, static_cast<UINT>(std::lround(sourceHeight * scale)));
    ComPtr<IWICBitmapSource> input = frame;
    if (destination.width != sourceWidth || destination.height != sourceHeight) {
        ComPtr<IWICBitmapScaler> scaler;
        hr = factory->CreateBitmapScaler(scaler.GetAddressOf());
        if (FAILED(hr) || FAILED(scaler->Initialize(frame.Get(), destination.width, destination.height,
                                                     WICBitmapInterpolationModeFant))) {
            error = "Windows Imaging Component could not scale texture: " + source.path;
            return false;
        }
        input = scaler;
    }
    ComPtr<IWICFormatConverter> converter;
    hr = factory->CreateFormatConverter(converter.GetAddressOf());
    if (FAILED(hr) || FAILED(converter->Initialize(input.Get(), GUID_WICPixelFormat32bppRGBA,
            WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom))) {
        error = "Windows Imaging Component could not convert texture to RGBA: " + source.path;
        return false;
    }
    destination.rgba.resize(static_cast<size_t>(destination.width) * destination.height * 4);
    hr = converter->CopyPixels(nullptr, destination.width * 4,
        static_cast<UINT>(destination.rgba.size()), destination.rgba.data());
    if (FAILED(hr)) { error = "Windows Imaging Component could not copy texture pixels: " + source.path; return false; }
    return true;
}

std::string unique_node_name(const std::vector<zegfx::ProductionSceneNode>& nodes,
                             const std::string& baseName) {
    auto exists = [&nodes](const std::string& name) {
        return std::any_of(nodes.begin(), nodes.end(), [&name](const auto& node) {
            return node.name == name;
        });
    };
    if (!exists(baseName)) return baseName;
    for (uint32_t suffix = 2; suffix != 0; ++suffix) {
        const std::string candidate = baseName + " " + std::to_string(suffix);
        if (!exists(candidate)) return candidate;
    }
    return baseName + " " + std::to_string(nodes.size() + 1);
}

struct InverseNodeTransform {
    // Row/column access is expressed in the same layout used by the production
    // renderer: x' = m[0]x + m[4]y + m[8]z + m[12].
    float linear[3][3]{};
    float translation[3]{};
    bool valid = false;
};

InverseNodeTransform make_inverse_node_transform(const float world[16]) {
    const float a00 = world[0], a01 = world[4], a02 = world[8];
    const float a10 = world[1], a11 = world[5], a12 = world[9];
    const float a20 = world[2], a21 = world[6], a22 = world[10];
    const float determinant = a00 * (a11 * a22 - a12 * a21) -
                              a01 * (a10 * a22 - a12 * a20) +
                              a02 * (a10 * a21 - a11 * a20);
    InverseNodeTransform inverse;
    if (!std::isfinite(determinant) || std::abs(determinant) < 1.0e-10f) return inverse;
    const float reciprocal = 1.0f / determinant;
    inverse.linear[0][0] = (a11 * a22 - a12 * a21) * reciprocal;
    inverse.linear[0][1] = (a02 * a21 - a01 * a22) * reciprocal;
    inverse.linear[0][2] = (a01 * a12 - a02 * a11) * reciprocal;
    inverse.linear[1][0] = (a12 * a20 - a10 * a22) * reciprocal;
    inverse.linear[1][1] = (a00 * a22 - a02 * a20) * reciprocal;
    inverse.linear[1][2] = (a02 * a10 - a00 * a12) * reciprocal;
    inverse.linear[2][0] = (a10 * a21 - a11 * a20) * reciprocal;
    inverse.linear[2][1] = (a01 * a20 - a00 * a21) * reciprocal;
    inverse.linear[2][2] = (a00 * a11 - a01 * a10) * reciprocal;
    std::copy(world + 12, world + 15, inverse.translation);
    inverse.valid = true;
    return inverse;
}

void transform_imported_vertex_to_node_space(const InverseNodeTransform& inverse,
                                              const float world[16], float position[3], float normal[3]) {
    const float offset[3] = {position[0] - inverse.translation[0],
                             position[1] - inverse.translation[1],
                             position[2] - inverse.translation[2]};
    float localPosition[3]{};
    float localNormal[3]{};
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            localPosition[row] += inverse.linear[row][column] * offset[column];
        }
    }
    // The importer applies inverse-transpose to normals. Undo that by applying
    // the transpose of the node's world linear transform.
    localNormal[0] = world[0] * normal[0] + world[1] * normal[1] + world[2] * normal[2];
    localNormal[1] = world[4] * normal[0] + world[5] * normal[1] + world[6] * normal[2];
    localNormal[2] = world[8] * normal[0] + world[9] * normal[1] + world[10] * normal[2];
    const float normalLength = std::sqrt(localNormal[0] * localNormal[0] +
                                         localNormal[1] * localNormal[1] +
                                         localNormal[2] * localNormal[2]);
    if (normalLength > 1.0e-8f && std::isfinite(normalLength)) {
        for (float& component : localNormal) component /= normalLength;
    }
    std::copy(localPosition, localPosition + 3, position);
    std::copy(localNormal, localNormal + 3, normal);
}

void transform_imported_tangent_to_node_space(const float world[16], float tangent[4]) {
    const float x = world[0] * tangent[0] + world[1] * tangent[1] + world[2] * tangent[2];
    const float y = world[4] * tangent[0] + world[5] * tangent[1] + world[6] * tangent[2];
    const float z = world[8] * tangent[0] + world[9] * tangent[1] + world[10] * tangent[2];
    const float length = std::sqrt(x * x + y * y + z * z);
    if (length > 1.0e-8f && std::isfinite(length)) {
        tangent[0] = x / length;
        tangent[1] = y / length;
        tangent[2] = z / length;
    }
}
} // namespace

bool load_production_scene(const std::string& path, zegfx::ProductionScene& scene, std::string& error) {
    if (is_builtin_production_scene(path)) return create_builtin_cube_scene(scene, error);
    auto imported = zegfx::asset::importAsset(path, {});
    if (!imported.success) { error = imported.error; return false; }
    ComPtr<IWICImagingFactory> imagingFactory;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(imagingFactory.GetAddressOf()));
    if (FAILED(hr)) { error = "Windows Imaging Component is unavailable."; return false; }
    auto& asset = imported.asset;
    scene = {};
    scene.sourceName = std::filesystem::path(path).stem().string();
    scene.nodes.reserve(asset.nodes.size() + 1);
    for (const auto& source : asset.nodes) {
        zegfx::ProductionSceneNode node;
        node.name = source.name;
        node.parentIndex = source.parentIndex;
        node.type = source.primitives.empty() ? zegfx::ProductionNodeType::Group
                                               : zegfx::ProductionNodeType::Mesh;
        std::copy(source.worldTransform.begin(), source.worldTransform.end(), node.worldTransform);
        scene.nodes.push_back(std::move(node));
    }
    scene.textures.resize(asset.textures.size());
    for (size_t i = 0; i < asset.textures.size(); ++i) {
        if (!decode_texture(asset.textures[i], imagingFactory.Get(), scene.textures[i], error)) return false;
        std::vector<unsigned char>().swap(asset.textures[i].embeddedData);
    }
    scene.materials.reserve(std::max<size_t>(asset.materials.size(), 1));
    for (const auto& source : asset.materials) {
        zegfx::ProductionMaterial material;
        std::copy(source.baseColorFactor, source.baseColorFactor + 4, material.baseColor);
        std::copy(source.emissiveFactor, source.emissiveFactor + 3, material.emission);
        material.metallic = source.metallicFactor;
        material.roughness = source.roughnessFactor;
        material.baseColorTexture = source.baseColorTexture;
        material.normalTexture = source.normalTexture;
        material.metallicRoughnessTexture = source.metallicRoughnessTexture;
        material.emissiveTexture = source.emissiveTexture;
        material.occlusionTexture = source.occlusionTexture;
        material.baseColorTexCoord = source.baseColorTexCoord;
        material.normalTexCoord = source.normalTexCoord;
        material.metallicRoughnessTexCoord = source.metallicRoughnessTexCoord;
        material.emissiveTexCoord = source.emissiveTexCoord;
        material.occlusionTexCoord = source.occlusionTexCoord;
        std::copy(source.baseColorUvScale, source.baseColorUvScale + 2, material.baseColorUvScale);
        std::copy(source.baseColorUvOffset, source.baseColorUvOffset + 2, material.baseColorUvOffset);
        material.alphaMode = source.alphaMode;
        material.alphaCutoff = source.alphaCutoff;
        scene.materials.push_back(material);
    }
    if (scene.materials.empty()) scene.materials.emplace_back();
    size_t triangleCapacity = 0;
    for (const auto& source : asset.primitives) {
        const size_t sourceTriangles = source.indices.size() / 3;
        if (sourceTriangles > std::numeric_limits<size_t>::max() - triangleCapacity) {
            error = "The imported asset contains too many triangles to stage.";
            return false;
        }
        triangleCapacity += sourceTriangles;
    }
    scene.triangles.reserve(triangleCapacity);
    for (const auto& source : asset.primitives) {
        const uint32_t material = source.materialIndex >= 0 &&
            static_cast<size_t>(source.materialIndex) < scene.materials.size()
                ? static_cast<uint32_t>(source.materialIndex) : 0u;
        const bool validNode = source.nodeIndex >= 0 &&
            static_cast<size_t>(source.nodeIndex) < scene.nodes.size();
        const InverseNodeTransform inverse = validNode
            ? make_inverse_node_transform(scene.nodes[static_cast<size_t>(source.nodeIndex)].worldTransform)
            : InverseNodeTransform{};
        for (size_t i = 0; i + 2 < source.indices.size(); i += 3) {
            const uint32_t indices[3] = {source.indices[i], source.indices[i + 1], source.indices[i + 2]};
            if (indices[0] >= source.vertices.size() || indices[1] >= source.vertices.size() ||
                indices[2] >= source.vertices.size()) continue;
            zegfx::ProductionTriangle triangle;
            triangle.materialIndex = material;
            triangle.nodeIndex = source.nodeIndex;
            for (size_t corner = 0; corner < 3; ++corner) {
                const auto& src = source.vertices[indices[corner]];
                auto& dst = triangle.vertices[corner];
                std::copy(src.position, src.position + 3, dst.position);
                std::copy(src.normal, src.normal + 3, dst.normal);
                // The importer stores vertices after applying the node's full
                // world transform. ProductionScene also retains that transform,
                // so convert the baked vertex data back to node-local space to
                // avoid applying the imported transform a second time.
                if (inverse.valid)
                    transform_imported_vertex_to_node_space(
                        inverse, scene.nodes[static_cast<size_t>(source.nodeIndex)].worldTransform,
                        dst.position, dst.normal);
                std::copy(src.tangent, src.tangent + 4, dst.tangent);
                if (inverse.valid)
                    transform_imported_tangent_to_node_space(
                        scene.nodes[static_cast<size_t>(source.nodeIndex)].worldTransform, dst.tangent);
                std::copy(src.color, src.color + 4, dst.color);
                std::copy(src.uv0, src.uv0 + 2, dst.uv);
                std::copy(src.uv1, src.uv1 + 2, dst.uv1);
            }
            scene.triangles.push_back(triangle);
        }
    }
    if (scene.triangles.empty()) { error = "The imported GLB contains no usable triangles."; return false; }
    float center[3], extent[3];
    for (int axis = 0; axis < 3; ++axis) {
        center[axis] = (asset.boundsMin[axis] + asset.boundsMax[axis]) * 0.5f;
        extent[axis] = std::max(asset.boundsMax[axis] - asset.boundsMin[axis], 0.01f);
    }
    const float largestExtent = std::max({extent[0], extent[1], extent[2]});
    scene.cameraTarget[0] = center[0]; scene.cameraTarget[1] = center[1]; scene.cameraTarget[2] = center[2];
    scene.cameraPosition[0] = center[0];
    scene.cameraPosition[1] = center[1] + largestExtent * 0.12f;
    scene.cameraPosition[2] = asset.boundsMax[2] + largestExtent * 2.4f;
    scene.verticalFovDegrees = 38.0f;
    scene.keyLightPosition[0] = 0.0f;
    scene.keyLightPosition[1] = 0.0f;
    scene.keyLightPosition[2] = 0.0f;
    scene.keyLightColor[0] = 0.0f;
    scene.keyLightColor[1] = 0.0f;
    scene.keyLightColor[2] = 0.0f;
    scene.activeCameraNode = static_cast<int32_t>(scene.nodes.size());
    zegfx::ProductionSceneNode renderCamera;
    renderCamera.name = unique_node_name(scene.nodes, "Render Camera");
    renderCamera.type = zegfx::ProductionNodeType::Camera;
    renderCamera.worldTransform[12] = scene.cameraPosition[0];
    renderCamera.worldTransform[13] = scene.cameraPosition[1];
    renderCamera.worldTransform[14] = scene.cameraPosition[2];
    scene.nodes.push_back(renderCamera);
    scene.keyLightNode = -1;
    error.clear();
    return true;
}

} // namespace medusa

#endif // MEDUSA_UI_ONLY_BUILD
