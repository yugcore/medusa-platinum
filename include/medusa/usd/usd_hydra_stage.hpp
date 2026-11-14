#pragma once
#include <medusa/medusa_types.hpp>
#include <zegfx/profiles/production/production_pipeline.h>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

namespace medusa::usd {

enum class USDPrimType {
    Unknown,
    Xform,
    Scope,
    Mesh,
    Camera,
    DistantLight,
    SphereLight,
    RectLight,
    DomeLight,
    Material,
    Shader
};

struct USDTransform {
    float matrix[16] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
    float translation[3] = {0.0f, 0.0f, 0.0f};
    float rotationEuler[3] = {0.0f, 0.0f, 0.0f}; // in degrees
    float scale[3] = {1.0f, 1.0f, 1.0f};
    bool hasMatrix = false;

    void compute_matrix();
};

struct USDMeshPrim {
    std::vector<float> points;            // 3 floats per vertex
    std::vector<int> faceVertexIndices;   // indices into points
    std::vector<int> faceVertexCounts;    // polygon sizes (3 for tri, 4 for quad, etc.)
    std::vector<float> normals;           // 3 floats per normal
    std::vector<float> uvs;               // 2 floats per uv (primvars:st)
    std::vector<float> displayColors;     // 3 floats per color (primvars:displayColor)
    std::string materialBinding;          // path to bound UsdMaterial
    bool doubleSided = false;
};

struct USDCameraPrim {
    float focalLength = 50.0f;           // in mm
    float focusDistance = 5.0f;          // in world units
    float horizontalAperture = 36.0f;    // 35mm full-frame width in mm
    float verticalAperture = 24.0f;      // 35mm full-frame height in mm
    float clippingRange[2] = {0.1f, 10000.0f};
};

struct USDLightPrim {
    std::string lightType;               // distant, sphere, rect, dome
    float intensity = 1000.0f;
    float color[3] = {1.0f, 1.0f, 1.0f};
    float exposure = 0.0f;
    float radius = 0.5f;                 // for sphere lights
    float width = 1.0f;                  // for rect lights
    float height = 1.0f;
};

struct USDMaterialPrim {
    float diffuseColor[4] = {0.8f, 0.8f, 0.8f, 1.0f};
    float emissiveColor[3] = {0.0f, 0.0f, 0.0f};
    float roughness = 0.45f;
    float metallic = 0.0f;
    float opacity = 1.0f;
    float ior = 1.5f;
    std::string diffuseTexturePath;
    std::string normalTexturePath;
    std::string roughnessTexturePath;
};

struct USDPrim {
    std::string path;
    std::string name;
    std::string parentPath;
    USDPrimType type = USDPrimType::Unknown;
    USDTransform transform;

    USDMeshPrim mesh;
    USDCameraPrim camera;
    USDLightPrim light;
    USDMaterialPrim material;
    std::vector<std::string> childPaths;
};

class USDHydraStage {
public:
    USDHydraStage() = default;

    bool load_from_file(const std::string& filepath, std::string& error);
    bool load_from_usda_string(const std::string& usda_text, std::string& error);
    bool load_from_usdc_memory(const uint8_t* data, size_t size, std::string& error);
    bool load_from_usdz_package(const std::string& filepath, std::string& error);

    bool to_production_scene(zegfx::ProductionScene& out_scene, std::string& error) const;

    const std::vector<USDPrim>& get_prims() const { return prims_; }
    float get_meters_per_unit() const { return meters_per_unit_; }
    const std::string& get_up_axis() const { return up_axis_; }

private:
    std::vector<USDPrim> prims_;
    std::unordered_map<std::string, size_t> prim_lookup_;
    float meters_per_unit_ = 1.0f;
    std::string up_axis_ = "Y";
    std::string default_prim_;

    bool parse_usda_tokens(const std::string& text, std::string& error);
    void compute_world_transforms(size_t prim_idx, const float parent_xform[16],
                                 std::vector<std::vector<float>>& world_xforms) const;
};

// Global helper functions
bool is_usd_file(const std::string& path);
bool import_usd_stage(const std::string& filepath, zegfx::ProductionScene& scene, std::string& error);

} // namespace medusa::usd
