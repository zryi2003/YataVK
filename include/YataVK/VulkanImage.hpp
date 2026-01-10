//
// Created by Zhuoran Yi on 25-3-4.
//

#ifndef VULKANIMAGE_HPP
#define VULKANIMAGE_HPP

#include <iostream>
#include <vulkan/vulkan_core.h>

#include "VulkanDevice.h"

namespace YATAVK {
    class VulkanImage final {
    public:
        VulkanImage(VulkanDevice* device, VkExtent3D extent, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkImageAspectFlags aspect) : device(device), extent(extent), format(format), tiling(tiling), usage(usage), aspect(aspect) {
            try {
                init();
            } catch (const std::exception& e) {
                cleanUp();
                std::cerr << e.what() << std::endl;
                throw std::runtime_error("Failed to initialize Vulkan image!");
            }
        };
        ~VulkanImage() { cleanUp(); };
        VulkanImage(const VulkanImage&) = delete;
        VulkanImage& operator=(const VulkanImage&) = delete;

        VkImageView getImageView() { return vkImageView;}
        VkSampler getSampler() { return vkSampler;}
        void transitionImageLayout(VkImageLayout oldLayout, VkImageLayout newLayout) const;

        // void loadImage(std::string path);

        // Eigen::Vector4f sample(Eigen::Vector2f uv);

    private:
        void init();
        void cleanUp();

        VulkanDevice* device;

        void* imageData = nullptr; // CPU端数据, 可以用在 CPU 侧采样 TODO: 留意这个东西有没有内存泄漏
        int texWidth, texHeight, texChannels;

        VkExtent3D extent;
        VkFormat format;
        VkImageTiling tiling;
        VkImageUsageFlags usage;
        VkImageAspectFlags aspect;

        VkImage vkImage = VK_NULL_HANDLE;
#ifdef YATAVK_ENABLE_VMA
        VmaAllocation vmaAllocation = VK_NULL_HANDLE;
#else
        VkDeviceMemory vkImageMemory = VK_NULL_HANDLE;
#endif
        VkImageView vkImageView = VK_NULL_HANDLE;

        VkSampler vkSampler = VK_NULL_HANDLE;
    };
}

#endif //VULKANIMAGE_HPP
