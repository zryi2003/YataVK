cmake_minimum_required(VERSION 3.20)

find_package(Vulkan 1.4 REQUIRED)

add_library(YataVK STATIC
    src/VulkanBuffer.cpp
    src/VulkanDescriptor.cpp
    src/VulkanDevice.cpp
    src/VulkanDynamicRenderer.cpp
    src/VulkanFrameScheduler.cpp
    src/VulkanGraphicsPipeline.cpp
    src/VulkanImage.cpp
    src/VulkanInstance.cpp
    src/VulkanShaderModule.cpp
    src/VulkanSwapChain.cpp
)

option(YATAVK_BUILD_LEGACY "Build the unsupported legacy render-pass path" OFF)
if (YATAVK_BUILD_LEGACY)
    message(FATAL_ERROR "YataVK Legacy is retained only for source archaeology and is not buildable")
endif()

target_compile_features(YataVK PUBLIC cxx_std_20)
target_include_directories(YataVK PUBLIC include)
target_link_libraries(YataVK PUBLIC Vulkan::Vulkan)

if (WIN32)
    target_compile_definitions(YataVK PUBLIC VK_USE_PLATFORM_WIN32_KHR WIN32_LEAN_AND_MEAN NOMINMAX)
endif()

add_library(YataVK::YataVK ALIAS YataVK)
