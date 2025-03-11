/**
 * @file api.h
 * @brief Vulkan API includes.
 */
#pragma once

/* Don't assert on result automatically */
#pragma warning(suppress : 4005)
#define VULKAN_HPP_ASSERT_ON_RESULT(expression) ((void)0)

/* Main header */
#include <vulkan/vulkan.hpp>

/* Vulkan Memory Allocator */
#include <vk_mem_alloc.h>

/* Debug functions */
#include <vulkan/vk_enum_string_helper.h>
