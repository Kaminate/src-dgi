# Find the Vulkan SDK
find_package(Vulkan REQUIRED)

set(MIN_VERSION "1.3.296") # Minimum required version

# Minimum Version 1.3.296 (earliest version when slangc was available)
if(${Vulkan_VERSION} VERSION_LESS ${MIN_VERSION})
    message(FATAL_ERROR 
        "Vulkan version '${Vulkan_VERSION}' was found."
        "Minimum required version is 1.3.296!"
    )
endif()

# Vulkan hpp library
add_subdirectory("vulkan-hpp")

add_library(vulkan INTERFACE)
target_compile_definitions(vulkan INTERFACE
    VULKAN_HPP_NO_EXCEPTIONS
    VULKAN_HPP_NO_SMART_HANDLE
    # VULKAN_HPP_NO_CONSTRUCTORS
)
target_include_directories(vulkan INTERFACE ${vulkanhpp_SOURCE_DIR})
target_link_libraries(vulkan INTERFACE Vulkan::Vulkan)
target_link_libraries(wyre PRIVATE vulkan)

# VMA - Vulkan Memory Allocator
add_subdirectory("vma")
target_link_libraries(wyre PRIVATE vma)
