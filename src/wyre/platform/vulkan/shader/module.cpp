/**
 * @file shader/module.cpp
 * @brief Vulkan shader module helper functions.
 */
#include "module.h"

#include <fstream> /* std::ifstream */

namespace wyre::shader {

using Bytes = std::vector<std::byte>;

/**
 * @brief Read the binary data of a file into a vector.
 */
inline Result<Bytes> read_binary_file(std::string_view path) {
    /* Try to open the file */
    std::ifstream file{path.data(), std::ios::ate | std::ios::binary};
    if (!file.is_open()) return Err("failed to open binary file. '%s'", path.data());

    /* Get the file size & make a buffer */
    const size_t size = file.tellg();
    Bytes buffer(size);

    /* Reset the read position & read into the buffer */
    file.seekg(0);
    file.read(reinterpret_cast<char*>(buffer.data()), size);

    file.close();
    return Ok(buffer);
}

Result<vk::ShaderModule> from_file(vk::Device device, std::string_view path) {
    /* Try to load the shader as binary */
    const Result<Bytes> r_bytes = read_binary_file(path);
    if (r_bytes.is_err()) return Err(r_bytes.unwrap_err());
    const Bytes bytes = r_bytes.unwrap();

    /* Shader module create info */
    vk::ShaderModuleCreateInfo create_info = {};
    create_info.codeSize = bytes.size();
    create_info.pCode = reinterpret_cast<const uint32_t*>(bytes.data());

    /* Create the shader module */
    vk::ShaderModule module = {};
    if (device.createShaderModule(&create_info, nullptr, &module) != vk::Result::eSuccess) {
        return Err("failed to create shader module. '%s'", path.data());
    }
    return Ok(module);
}

}  // namespace wyre::shader
