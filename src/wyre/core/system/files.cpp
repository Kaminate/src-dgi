#include "files.h"

#include <fstream> /* std::ifstream */

namespace wyre {

Result<std::string> Files::read_text_file(const std::string& path) const {
    std::ifstream file(path);
    if (!file.is_open()) return Err("file '%s' was not found.", path.c_str());
    file.seekg(0, std::ios::end);
    const std::streamsize size = file.tellg();
    std::string buffer(size, '\0');
    file.seekg(0);
    file.read(buffer.data(), size); 
    return Ok(buffer);
}

Result<std::vector<std::byte>> Files::read_binary_file(const std::string& path) const {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return Err("file '%s' was not found.", path.c_str());
    const std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<std::byte> buffer(size);
    file.read(reinterpret_cast<char*>(buffer.data()), size);
    return Ok(buffer);
}

}  // namespace wyre
