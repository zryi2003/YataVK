//
// Created by Zhuoran Yi on 25-2-13.
//

#include "YataVK/VulkanDevice.h"

#include <array>
#include <iostream>
#include <iomanip>

namespace YATAVK {
    void VulkanDevice::init(VkInstance instance, VkSurfaceKHR surface) {
        vkInstance = instance;
        vkSurface = surface;

        pickPhysicalDevice();
        createLogicalDevice();

#ifdef YATAVK_ENABLE_VMA
        VmaAllocatorCreateInfo allocatorInfo = {};
        allocatorInfo.physicalDevice = vkPhysicalDevice;
        allocatorInfo.device = vkLogicalDevice;
        allocatorInfo.instance = vkInstance;
        allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_3;
        vmaCreateAllocator(&allocatorInfo, &vmaAllocator);
#endif

        createCommandPool();
        createCommandBuffers();
        createDescriptorPool();
    }


    void VulkanDevice::cleanUp() {
#ifdef YATAVK_ENABLE_VMA
        if (vmaAllocator != VK_NULL_HANDLE) {
            vmaDestroyAllocator(vmaAllocator);
        }
#endif

        if (vkCommandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(vkLogicalDevice, vkCommandPool, nullptr);
        }
        if (vkDescriptorPool != VK_NULL_HANDLE) { // 注意, Descriptor Set 是从 Descriptor Pool 分配出来的, 释放描述符池会自动释放所有的描述符集
            vkDestroyDescriptorPool(vkLogicalDevice, vkDescriptorPool, nullptr);
        }
        /*------------------------------------------------------------*/
        if (vkLogicalDevice != VK_NULL_HANDLE) {
            vkDestroyDevice(vkLogicalDevice, nullptr);
        }
    }

    /*-----------------------------------Check And Select Device---------------------------------*/

    bool checkDeviceExtensionSupport(VkPhysicalDevice device) {
        uint32_t extensionCount;

        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

        std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

        for (const auto &extension: availableExtensions) {
            requiredExtensions.erase(extension.extensionName);
        }

        return requiredExtensions.empty();
    }

    QueueFamilyIndices VulkanDevice::findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface) {
        QueueFamilyIndices indices;

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        int i = 0;
        for (const auto &queueFamily: queueFamilies) {
            if (queueFamily.queueFlags &
                VK_QUEUE_GRAPHICS_BIT) { // Flags是做了状态压缩的, 因此与一下的结果就是这个BIT是否为1, 故这里是在找当前物理设备是否支持图形相关的队列
                indices.graphicsFamily = i;
                }

            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

            if (presentSupport) {
                indices.presentFamily = i;
            }

            if (indices.isComplete()) {
                break;
            }

            i++;
        }

        return indices;
    }

    SwapChainSupportDetails VulkanDevice::querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface) {
        SwapChainSupportDetails details;

        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

        if (formatCount != 0) {
            details.formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
        }

        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

        if (presentModeCount != 0) {
            details.presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
        }

        return details;
    }

    bool VulkanDevice::isDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface) {
        vkQueueFamilyIndices = findQueueFamilies(device, surface);

        bool extensionSupported = checkDeviceExtensionSupport(device);

        bool swapChainAdequate = false;
        if (extensionSupported) {
            SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device, surface);
            swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
        }

        return vkQueueFamilyIndices.isComplete() && extensionSupported && swapChainAdequate;
    }

    void VulkanDevice::pickPhysicalDevice() {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(vkInstance, &deviceCount, nullptr);

        if (deviceCount == 0) {
            throw std::runtime_error(
                    "\n"
                    "************************************************************\n"
                    " [ACHIEVEMENT UNLOCKED: THE ANCIENT RELIC] \n"
                    "************************************************************\n"
                    "Error: vkEnumeratePhysicalDevices returned ZERO devices.\n"
                    "\n"
                    "LOGIC PARADOX DETECTED:\n"
                    " - The Vulkan Loader is active and your application has linked to it.\n"
                    " - You are looking at this screen, so a display adapter exists.\n"
                    " - Yet, Vulkan Loader found nothing. Not even a heartbeat.\n"
                    "\n"
                    "DIAGNOSIS:\n"
                    " Your GPU or driver is a fossil from a bygone era.\n"
                    "\n"
                    "BY THE WAY:\n"
                    " If you actually tried to run this GUI app on a headless \n"
                    " compute-only server... stop. It means there are no humans \n"
                    " left to tell you that this is a terrible idea.\n"
                    " \n"
                    " RUN!\n"
                    "************************************************************"
                );
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(vkInstance, &deviceCount, devices.data());

        for (const auto &device: devices) {
            if (isDeviceSuitable(device, vkSurface)) {
                vkPhysicalDevice = device;
                break;
            }
        }

        if (vkPhysicalDevice == VK_NULL_HANDLE) {
            throw std::runtime_error("Failed to find a suitable device!");
        }
    }
    /*-------------------------------------------------------------------------------------------*/

    void VulkanDevice::createLogicalDevice() {
        QueueFamilyIndices indices = findQueueFamilies(vkPhysicalDevice, vkSurface);

        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(), indices.presentFamily.value()};

        float queuePriority = 1.0f;
        for (uint32_t queueFamily: uniqueQueueFamilies) {
            VkDeviceQueueCreateInfo queueCreateInfo{};

            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = indices.graphicsFamily.value();
            queueCreateInfo.queueCount = 1;

            queueCreateInfo.pQueuePriorities = &queuePriority;

            queueCreateInfos.emplace_back(queueCreateInfo);
        }
        VkPhysicalDeviceFeatures deviceFeatures{.geometryShader = VK_TRUE, .tessellationShader = VK_TRUE}; // 我们要用几何着色器和曲面细分着色器, 记得启动

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.queueCreateInfoCount = 1;

        createInfo.pNext = &dynamic_rendering_feature;
        createInfo.pEnabledFeatures = &deviceFeatures;

        createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();

#ifdef DEBUG
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
#else
        createInfo.enabledLayerCount = 0;
#endif

        if (vkCreateDevice(vkPhysicalDevice, &createInfo, nullptr, &vkLogicalDevice) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create logical device!");
        }

        vkGetDeviceQueue(vkLogicalDevice, indices.graphicsFamily.value(), 0, &vkGraphicsQueue);
        vkGetDeviceQueue(vkLogicalDevice, indices.presentFamily.value(), 0, &vkPresentQueue);
    }

    void VulkanDevice::createCommandPool() {
        QueueFamilyIndices queueFamilyIndices = findQueueFamilies(vkPhysicalDevice, vkSurface);

        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; // 每一帧都要重新录制command buffer, 因此我们需要允许单独重置command buffer
        poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

        if (vkCreateCommandPool(vkLogicalDevice, &poolInfo, nullptr, &vkCommandPool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create command pool!");
        }
    }

    void VulkanDevice::createCommandBuffers() {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = vkCommandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        if (vkAllocateCommandBuffers(vkLogicalDevice, &allocInfo, &vkCommandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create command buffers!");
        }
    }

    void VulkanDevice::createDescriptorPool() { // 这玩意到底能开多大啊, 怎么测了下我放100000个Spline进去也没炸
        std::vector<VkDescriptorPoolSize> poolSizes = {
            {.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,.descriptorCount = 409600},
            {.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,.descriptorCount = 409600}
        };

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = 409600; // 目前暂时设定为描述符池支持建立1024个描述符集

        if (vkCreateDescriptorPool(vkLogicalDevice, &poolInfo, nullptr, &vkDescriptorPool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create descriptor pool!");
        }
    }


    /*-------------------------------------------------------------------------------------------*/

    uint32_t VulkanDevice::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(vkPhysicalDevice, &memProperties);

        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
            if ((typeFilter & (1 << i)) && memProperties.memoryTypes[i].propertyFlags & properties) {
                return i;
            }
        }
        throw std::runtime_error("Failed to find suitable memory type!");
    }
    /*-------------------------------------------------------------------------------------------*/


    inline std::string getVendorName(uint32_t vendorID) {
        if (vendorID == 0x10DE) return "NVIDIA";
        if (vendorID == 0x1002) return "AMD";
        if (vendorID == 0x8086) return "Intel";
        if (vendorID == 0x13B5) return "ARM";
        return "Unknown (" + std::to_string(vendorID) + ")";
    }

    // 辅助函数：驱动版本解码 (针对 NVIDIA/Vulkan 规范)
    inline std::string decodeDriverVersion(uint32_t v, uint32_t vendorID) {
        if (vendorID == 0x10DE) { // NVIDIA 专用解码
            return std::to_string(v >> 22) + "." + std::to_string((v >> 14) & 0xFF) + "." + std::to_string((v >> 6) & 0xFF);
        }
        // 标准 Vulkan 解码
        return std::to_string(VK_VERSION_MAJOR(v)) + "." + std::to_string(VK_VERSION_MINOR(v)) + "." + std::to_string(VK_VERSION_PATCH(v));
    }

    void VulkanDevice::printDeviceInfo() {
        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(vkPhysicalDevice, &deviceProperties);

        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(vkPhysicalDevice, &memProperties);

        std::cout << "\n======================================================\n";
        std::cout << "               Vulkan Device Information              \n";
        std::cout << "======================================================\n";

        std::cout << std::left << std::setw(25) << "Device Name:" << deviceProperties.deviceName << "\n";
        std::cout << std::left << std::setw(25) << "Vendor:" << getVendorName(deviceProperties.vendorID) << "\n";
        std::cout << std::left << std::setw(25) << "Driver Version:" << decodeDriverVersion(deviceProperties.driverVersion, deviceProperties.vendorID) << "\n";

        for (uint32_t i = 0; i < memProperties.memoryHeapCount; i++) {
            if (memProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
                float sizeGB = static_cast<float>(memProperties.memoryHeaps[i].size) / (1024.0f * 1024.0f * 1024.0f);
                std::cout << std::left << std::setw(25) << "VRAM (Device Local):" << std::fixed << std::setprecision(2) << sizeGB << " GB\n";
            }
        }

        std::cout << "------------------ Hardware Limits -------------------\n";
        std::cout << std::left << std::setw(25) << "Max Storage Buffer:" << (deviceProperties.limits.maxStorageBufferRange / (1024 * 1024)) << " MB\n";
        std::cout << std::left << std::setw(25) << "Max Per-Stage Samplers:" << deviceProperties.limits.maxPerStageDescriptorSamplers << "\n";
        std::cout << std::left << std::setw(25) << "Timestamp Period:" << deviceProperties.limits.timestampPeriod << " ns\n";

        std::cout << "======================================================\n\n";
    }
}
/*-------------------------------------------------------------------------------------------*/