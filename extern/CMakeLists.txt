# Entity Component System (ECS)
# https://github.com/skypjack/entt
add_subdirectory("entt")
target_link_libraries(wyre PUBLIC EnTT)

# Graphics APIs
add_subdirectory("gapi")

# SDL Windowing and Input
# https://github.com/libsdl-org/SDL
set(SDL_SHARED OFF)
set(SDL_STATIC ON)
set(SDL_TEST_LIBRARY OFF)
set(SDL_TESTS OFF)
set(SDL_DISABLE_INSTALL ON)
set(SDL_DISABLE_INSTALL_DOCS ON)
set(SDL_INSTALL_TESTS OFF)
add_subdirectory("sdl")
target_link_libraries(wyre PRIVATE SDL3-static)

# GLM
# https://github.com/g-truc/glm
add_subdirectory("glm")
target_link_libraries(wyre PUBLIC glm)

# STB IMAGE
target_include_directories(wyre PRIVATE "./stb/")

# Fast GLTF (GLTF model file parsing)
# https://github.com/spnda/fastgltf
add_subdirectory("fastgltf")
target_link_libraries(wyre PRIVATE fastgltf)

# IMGUI
# https://github.com/ocornut/imgui
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/imgui/)
target_sources(wyre
    PRIVATE ${IMGUI_DIR}/imgui_demo.cpp
    PRIVATE ${IMGUI_DIR}/imgui_draw.cpp
    PRIVATE ${IMGUI_DIR}/imgui_tables.cpp
    PRIVATE ${IMGUI_DIR}/imgui_widgets.cpp
    PRIVATE ${IMGUI_DIR}/imgui.cpp
    PRIVATE ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
    PRIVATE ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
    PRIVATE ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
)
target_include_directories(wyre
    PRIVATE ${IMGUI_DIR}
    PRIVATE ${IMGUI_DIR}/backends
)
