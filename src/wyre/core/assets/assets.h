#pragma once

#include <unordered_map>
#include <string>
#include <memory> /* std::shared_ptr */

#include "asset.h" /* wyre::Asset */

namespace wyre {

/**
 * @brief Assets manager, responsible for loading & tracking game assets.
 */
class Assets {
    /* Map of all assets, bound to their handle. */
    std::unordered_map<size_t, std::shared_ptr<Asset>> assets;

   public:
    /**
     * @brief Load an asset from a file. (returns the asset if already loaded)
     */
    template <typename T, typename... Args>
    std::shared_ptr<T> load(const std::string_view path, Args&&... args);

    /**
     * @brief Create an asset. (returns nullptr if the asset already exists)
     */
    template <typename T, typename... Args>
    std::shared_ptr<T> create(const std::string_view name, Args&&... args);

    /**
     * @brief Get an already loaded asset. (nullptr if the asset is not loaded)
     */
    template <typename T>
    std::shared_ptr<T> get(size_t id);

    /**
     * @brief Execute garbage collection, will unload any unused assets.  
     * Should be run when loading a new scene for example.
     */
    void collect_garbage();
};

template <typename T, typename... Args>
inline std::shared_ptr<T> Assets::load(const std::string_view path, Args&&... args) {
    /* Use the hashed path as the asset id */
    const auto id = std::hash<std::string>()(path);

    /* Check if the asset is already loaded */
    auto asset = get<T>(id);
    if (asset) return asset;

    /* Otherwise, load the asset */
    assets[id] = std::make_shared<T>(path, std::forward<Args>(args)...);
    assets[id]->id = id;

    return std::dynamic_pointer_cast<T>(assets[id]);
}

template <typename T, typename... Args>
inline std::shared_ptr<T> Assets::create(const std::string_view name, Args&&... args) {
    /* Use the hashed name as the asset id */
    const auto id = std::hash<std::string>()(name);

    /* Check if the asset already exists */
    auto asset = get<T>(id);
    if (asset) return nullptr;

    /* Otherwise, create the asset */
    assets[id] = std::make_shared<T>(std::forward<Args>(args)...);
    assets[id]->id = id;

    return std::dynamic_pointer_cast<T>(assets[id]);
}

template <typename T>
inline std::shared_ptr<T> Assets::get(size_t id) {
    auto it = assets.find(id);
    if (it != assets.end()) return std::dynamic_pointer_cast<T>(it->second);
    return std::shared_ptr<T>();
}

}  // namespace wyre
