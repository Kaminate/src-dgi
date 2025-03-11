#include "mesh.h"

#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/glm_element_traits.hpp>

#include "wyre/core/scene/triangle.h"
#include "wyre/core/system/files.h"

namespace wyre {

/* Shorten namespace */
namespace gltf {
using namespace fastgltf;
}

/** @brief Basic triangle mesh. */
struct TriMesh {
    std::vector<glm::vec3> vertices{};
    std::vector<glm::vec3> normals{};
    std::vector<uint32_t> indices{};
    size_t tri_count = 0u;
};

/* TODO: Loading & storing of meshes should be done using Assets!!! */

Mesh::Mesh(const Triangle* triangles, const size_t triangle_count) {
    /* Create vertex array */
    vertices = std::vector<glm::vec3>(triangle_count * 3);
    normals = std::vector<glm::vec3>(triangle_count * 3);
    indices = std::vector<uint32_t>();

    /* Push the triangles into the vertex array */
    for (size_t i = 0; i < triangle_count; ++i) {
        vertices[i * 3 + 0] = triangles[i].v0;
        vertices[i * 3 + 1] = triangles[i].v1;
        vertices[i * 3 + 2] = triangles[i].v2;
        normals[i * 3 + 0] = glm::vec3(0.0f);
        normals[i * 3 + 1] = glm::vec3(0.0f);
        normals[i * 3 + 2] = glm::vec3(0.0f);
    }
    tri_count = triangle_count;
}

/** @brief Parse a GLTF mesh node into a triangle mesh. */
TriMesh parse_mesh_node(const gltf::Node& node, const fastgltf::Asset& model);

/** @brief Parse the indices of a mesh node into a triangle mesh. */
void parse_indices(TriMesh& out_mesh, const gltf::Accessor& accessor, const fastgltf::Asset& model);

Mesh::Mesh(const Files& files, const std::string& path, const glm::vec3 mat, const size_t mesh_idx) {
    const wyre::Result result = files.read_binary_file(path);

    /* TODO: Improve error handling? */
    assert(!result.is_err() && "failed to read binary file from disk!");

    const std::vector<std::byte> data = result.unwrap();
    const std::string dir_path = path.substr(0, path.find_last_of("\\/"));

    /* Create a GLTF parser and GLTF data buffer */
    gltf::Parser parser;
    gltf::Expected buffer = gltf::GltfDataBuffer::FromBytes(data.data(), data.size());

    /* Parse the GLTF file */
    gltf::Expected model = parser.loadGltf(buffer.get(), dir_path);
    if (auto error = model.error(); error != gltf::Error::None) {
        assert(false && "failed to parse GLTF model file!");
    }

    /* Parse the first mesh in the GLTF file */
    /* TODO: parse all meshes instead of just one... */
    TriMesh mesh {};
    size_t idx = 0u;
    for (const gltf::Node& node : model->nodes) {
        if (node.meshIndex.has_value()) {
            if (idx != mesh_idx) { 
                idx++;
                continue;
            }
            printf("loading mesh: %s\n", node.name.c_str());
            mesh = parse_mesh_node(node, model.get());
            break;
        }
    }

    /* Store the mesh */
    vertices = mesh.vertices;
    normals = mesh.normals;
    indices = mesh.indices;
    tri_count = mesh.tri_count;
    material = mat;
}

TriMesh parse_mesh_node(const gltf::Node& node, const fastgltf::Asset& model) {
    TriMesh tris{};

    /* Get the mesh */
    const gltf::Mesh& mesh = model.meshes[node.meshIndex.value()];

    /* Return if there's nothing to parse */
    if (mesh.primitives.size() == 0u) return tris;

    /* Process all the primitives */
    for (const gltf::Primitive& prim : mesh.primitives) {
        /* Only handle some attributes for now */
        const gltf::Attribute* attr_pos = prim.findAttribute("POSITION");
        const gltf::Attribute* attr_nor = prim.findAttribute("NORMAL");

        /* Skip if the POSITION or NORMAL attribute was not found */
        if (attr_pos == nullptr || attr_nor == nullptr) continue;

        /* Get indices if present */
        if (prim.indicesAccessor.has_value()) {
            /* Get the indices accessor */
            const gltf::Accessor& accessor = model.accessors[prim.indicesAccessor.value()];
            parse_indices(tris, accessor, model);
        }

        /* Get the vertices */
        const gltf::Accessor& acc_pos = model.accessors[attr_pos->accessorIndex];
        const gltf::Accessor& acc_nor = model.accessors[attr_nor->accessorIndex];

        /* Do nothing if the buffer view is not present */
        if (acc_pos.bufferViewIndex.has_value() == false || acc_nor.bufferViewIndex.has_value() == false) continue;
        tris.vertices.resize(acc_pos.count);
        tris.normals.resize(acc_nor.count);
        if (tris.tri_count == 0) tris.tri_count = acc_pos.count / 3;

        /* Push the vertices from the buffers into our output mesh */
        gltf::iterateAccessorWithIndex<glm::vec3>(model, acc_pos, [&](glm::vec3 vert, size_t i) { tris.vertices[i] = vert; });
        gltf::iterateAccessorWithIndex<glm::vec3>(model, acc_nor, [&](glm::vec3 norm, size_t i) { tris.normals[i] = norm; });
    }

    return tris;
}

void parse_indices(TriMesh& out_mesh, const gltf::Accessor& accessor, const fastgltf::Asset& model) {
    /* Do nothing if the buffer view is not present */
    if (accessor.bufferViewIndex.has_value() == false) return;

    /* Set the size of the indices array */
    out_mesh.indices.resize(accessor.count);
    out_mesh.tri_count = accessor.count / 3;

    /* Push the indices from the buffers into our output mesh */
    gltf::iterateAccessorWithIndex<uint32_t>(model, accessor, [&](uint32_t index, size_t i) { out_mesh.indices[i] = index; });
}

}  // namespace wyre
