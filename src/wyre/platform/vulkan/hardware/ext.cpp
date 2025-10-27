/**
 * @file hardware/ext.cpp
 * @brief All vulkan extension/layer querying related code.
 */
#include "ext.h"

#include <SDL3/SDL_vulkan.h> /* SDL_Vulkan_GetInstanceExtensions */

#include "../api.h"

namespace wyre {

/* Vulkan device extensions */
const std::vector<const char*> DEVICE_EXT = {
    /* Swapchain */
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    /* Dynamic Rendering */
    VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
    VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME,
    VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME,
    VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME};

bool validate_extensions(const std::vector<const char*>& required, const std::vector<vk::ExtensionProperties>& available) {
    /* Check if all required extensions are available */
    for (const char* ext : required) {
        bool found = false;
        for (const vk::ExtensionProperties& available_ext : available) {
            if (strcmp(available_ext.extensionName, ext)) {
                found = true;
            }
        }
        /* Extension not available */
        if (found == false) {
            printf("required extension '{}' not available!", ext);
            return false;
        }
    }
    return true; /* All layers available */
}

bool validate_layers(const std::vector<const char*>& required, const std::vector<vk::LayerProperties>& available) {
    /* Check if all required layers are available */
    for (const char* layer : required) {
        bool found = false;
        for (const vk::LayerProperties& available_layer : available) {
            if (strcmp(available_layer.layerName, layer)) {
                found = true;
            }
        }
        /* Layer not available */
        if (found == false) {
            printf("required validation layer '{}' not available!", layer);
            return false;
        }
    }
    return true; /* All layers available */
}

std::vector<const char*> get_optimal_validation_layers(const std::vector<vk::LayerProperties>& supported_instance_layers) {
    /* Validation layer priority list <https://github.com/KhronosGroup/Vulkan-Samples> */
    std::vector<std::vector<const char*>> prio_list = {
        {"VK_LAYER_KHRONOS_validation"},         /* (default) preferred validation layer */
        {"VK_LAYER_LUNARG_standard_validation"}, /* (fallback) LunarG meta layer */
        {
            "VK_LAYER_GOOGLE_threading",
            "VK_LAYER_LUNARG_parameter_validation",
            "VK_LAYER_LUNARG_object_tracker",
            "VK_LAYER_LUNARG_core_validation",
            "VK_LAYER_GOOGLE_unique_objects",
        },                                  /* (fallback) Individual layers that compose LunarG meta layer */
        {"VK_LAYER_LUNARG_core_validation"} /* (last resort) LunarG core layer */
    };

    /* Go through the priority list */
    for (auto& layers : prio_list) {
        if (validate_layers(layers, supported_instance_layers)) {
            return layers;
        }
    }

    /* Worst case */
    printf("could not find any available validation layers!");
    return {};
}

std::vector<const char*> get_sdl_extensions() {
    uint32_t cnt = 0; /* Get SDL Vulkan extensions */
    auto ext = SDL_Vulkan_GetInstanceExtensions(&cnt);
    const std::vector<const char*> extensions = std::vector<const char*>(ext, ext + cnt);
    return extensions;
}

}  // namespace wyre
