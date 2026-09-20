#pragma once

#include "YataVK/VulkanDevice.h"

#include <vulkan/vulkan.h>

namespace YATAVK {

    struct VulkanImageConfig {
        VkExtent3D extent{1, 1, 1};
        VkFormat format = VK_FORMAT_UNDEFINED;
        VkImageUsageFlags usage = 0;
        VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;
        VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;
        VkMemoryPropertyFlags memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
    };

    class VulkanImage final {
    public:
        VulkanImage(VulkanDevice& device, const VulkanImageConfig& config);
        VulkanImage(VulkanDevice* device, VkExtent3D extent, VkFormat format, VkImageTiling tiling,
                    VkImageUsageFlags usage, VkImageAspectFlags aspect)
            : VulkanImage(*device, VulkanImageConfig{extent, format, usage, aspect, tiling}) {}
        ~VulkanImage();

        VulkanImage(const VulkanImage&) = delete;
        VulkanImage& operator=(const VulkanImage&) = delete;
        VulkanImage(VulkanImage&& other) noexcept;
        VulkanImage& operator=(VulkanImage&& other) noexcept;

        [[nodiscard]] VkImage getHandle() const { return image_; }
        [[nodiscard]] VkImageView getImageView() const { return view_; }
        [[nodiscard]] VkFormat getFormat() const { return config_.format; }
        [[nodiscard]] VkExtent3D getExtent() const { return config_.extent; }

    private:
        void destroy() noexcept;

        VulkanDevice* device_ = nullptr;
        VulkanImageConfig config_{};
        VkImage image_ = VK_NULL_HANDLE;
        VkDeviceMemory memory_ = VK_NULL_HANDLE;
        VkImageView view_ = VK_NULL_HANDLE;
    };

} // namespace YATAVK
