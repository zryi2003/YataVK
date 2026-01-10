//
// Created by Zhuoran Yi on 2026/1/8.
//

#ifndef YATA_VULKANCOMMON_H
#define YATA_VULKANCOMMON_H

#include <cstring>
#include <vector>
#include <optional>
#include <fstream>

#include <vulkan/vulkan_core.h>

namespace YATAVK {
#ifdef DEBUG
    const bool enableValidationLayers = true;
#else
    constexpr bool enableValidationLayers = false;
#endif

    /*---------------------Set Validation Layers and Device Extensions We Need-------------------*/
    inline const std::vector<const char *> validationLayers = { // 启用的验证层
        "VK_LAYER_KHRONOS_validation"
    };
    // 如果启用了 validation layer 但是在 Vulkan Configurator 中的 Debug Action 里面没打开 Debug Output, 那么 validation layer 不会输出任何东西

    inline const std::vector<const char *> deviceExtensions = { // 需要使用的设备扩展
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
        VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME,
        VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME
    };

    // https://docs.vulkan.org/samples/latest/samples/extensions/dynamic_rendering/README.html
    inline constexpr VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamic_rendering_feature {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR,
        .dynamicRendering = VK_TRUE,
    };
    /*-------------------------------------------------------------------------------------------*/

    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;

        [[nodiscard]] bool isComplete() const {
            return graphicsFamily.has_value() && presentFamily.has_value();
        }
    };

    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities{};
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    // 如果启用了 validation layer 但是在 Vulkan Configurator 中的 Debug Action 里面没打开 Debug Output, 那么 validation layer 不会输出任何东西

    inline bool checkValidationLayerSupport() {
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        for (const char *layerName: validationLayers) {
            bool layerFound = false;

            for (const auto &layerProperties: availableLayers) {
                if (std::strcmp(layerName, layerProperties.layerName) == 0) {
                    layerFound = true;
                    break;
                }
            }

            if (!layerFound) {
                return false;
            }
        }

        return true;
    }

    inline std::vector<char> readFile(const std::string& filename) {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);
        if (!file.is_open()) {
            throw std::runtime_error("failed to open file: " + filename);
        }
        size_t fileSize = (size_t)file.tellg();
        std::vector<char> buffer(fileSize);
        file.seekg(0);
        file.read(buffer.data(), fileSize);
        file.close();
        return buffer;
    }

    inline VkShaderModule createShaderModule(VkDevice logicalDevice, const std::vector<char>& code) {
        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

        VkShaderModule shaderModule;
        if (vkCreateShaderModule(logicalDevice, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create shader module!");
        }
        return shaderModule;
    }
}

#endif //YATA_VULKANCOMMON_H