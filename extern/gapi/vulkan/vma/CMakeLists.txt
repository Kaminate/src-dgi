cmake_minimum_required(VERSION 3.15)
project(vma)

# Create vulkan SDK interface library
add_library(vma INTERFACE)

# Add directories
target_include_directories(vma INTERFACE ".")
