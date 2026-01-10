//
// Created by Zhuoran Yi on 25-2-13.
//

#ifndef VULKANDEVICE_H
#define VULKANDEVICE_H
// 创建顺序: VulkanConfig->VulkanDevice->VulkanCompute->Mesh
// 回收顺序相反
#include <stdexcept>
#include <vector>
#include <optional>
#include <algorithm>
#include <limits>
#include <set>

#include <vulkan/vulkan_core.h>

#ifdef YATAVK_ENABLE_VMA
#include <vma/vk_mem_alloc.h>
#endif

#include "VulkanCommon.h"


namespace YATAVK {


    class VulkanDevice final {
    public:
        VulkanDevice(VkInstance instance, VkSurfaceKHR surface) {
            try {
                init(instance, surface);
                printDeviceInfo();
            } catch (const std::exception& e) {
                cleanUp();
                throw std::runtime_error("Failed to initialize Vulkan device!");
            }
        }
        ~VulkanDevice() { cleanUp(); }
        VulkanDevice(const VulkanDevice&) = delete;
        VulkanDevice& operator=(const VulkanDevice&) = delete;

        void init(VkInstance instance, VkSurfaceKHR surface);
        void cleanUp();

        VkQueue getGraphicsQueue() { return vkGraphicsQueue; }
        VkQueue getPresentQueue() { return vkPresentQueue; }
        VkDevice getLogicalDevice() { return vkLogicalDevice; }
        VkPhysicalDevice getPhysicalDevice() { return vkPhysicalDevice; }
        VkSurfaceKHR getSurface() { return vkSurface; }
        VkCommandBuffer getCommandBuffer() { return vkCommandBuffer; }
        VkCommandBuffer& getpCommandBuffer() { return vkCommandBuffer; }
        VkDescriptorPool getDescriptorPool() { return vkDescriptorPool; }
        QueueFamilyIndices getQueueFamilyIndices() { return vkQueueFamilyIndices; }
        uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
        SwapChainSupportDetails querySwapChainSupport() {
            return querySwapChainSupport(vkPhysicalDevice, vkSurface);
        }

#ifdef YATAVK_ENABLE_VMA
        VmaAllocator getVmaAllocator() { return vmaAllocator; }
#endif

    private:
        VkInstance vkInstance = VK_NULL_HANDLE;
        VkDevice vkLogicalDevice = VK_NULL_HANDLE;
        VkPhysicalDevice vkPhysicalDevice = VK_NULL_HANDLE;


        QueueFamilyIndices vkQueueFamilyIndices{};
        VkQueue vkGraphicsQueue = VK_NULL_HANDLE;
        VkQueue vkPresentQueue = VK_NULL_HANDLE;

        VkSurfaceKHR vkSurface = VK_NULL_HANDLE;

        VkCommandPool vkCommandPool{};
        VkCommandBuffer vkCommandBuffer{};

        VkDescriptorPool vkDescriptorPool{};

#ifdef YATAVK_ENABLE_VMA
        VmaAllocator vmaAllocator = VK_NULL_HANDLE;
#endif

        void pickPhysicalDevice();
        void createLogicalDevice();
        void createCommandPool();
        void createCommandBuffers();
        void createDescriptorPool();
        QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface);
        SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);
        bool isDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface);

        void printDeviceInfo();
    };
}

#endif //VULKANDEVICE_H
