#include "YataVK/VulkanImage.hpp"

#include "YataVK/VulkanError.hpp"

#include <stdexcept>
#include <utility>

namespace YATAVK {

    VulkanImage::VulkanImage(VulkanDevice& device, const VulkanImageConfig& config)
        : device_(&device), config_(config) {
        if (config.format == VK_FORMAT_UNDEFINED || config.extent.width == 0 || config.extent.height == 0 ||
            config.extent.depth == 0 || config.usage == 0) {
            throw std::invalid_argument("VulkanImageConfig is incomplete");
        }
        VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        imageInfo.imageType = config.extent.depth > 1 ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D;
        imageInfo.extent = config.extent;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = config.format;
        imageInfo.tiling = config.tiling;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = config.usage;
        imageInfo.samples = config.samples;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        checkVk(vkCreateImage(device_->getLogicalDevice(), &imageInfo, nullptr, &image_), "vkCreateImage");
        try {
            VkMemoryRequirements requirements{};
            vkGetImageMemoryRequirements(device_->getLogicalDevice(), image_, &requirements);
            VkMemoryAllocateInfo allocationInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
            allocationInfo.allocationSize = requirements.size;
            allocationInfo.memoryTypeIndex =
                device_->findMemoryType(requirements.memoryTypeBits, config.memoryProperties);
            checkVk(vkAllocateMemory(device_->getLogicalDevice(), &allocationInfo, nullptr, &memory_),
                    "vkAllocateMemory(image)");
            checkVk(vkBindImageMemory(device_->getLogicalDevice(), image_, memory_, 0), "vkBindImageMemory");

            VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
            viewInfo.image = image_;
            viewInfo.viewType = config.extent.depth > 1 ? VK_IMAGE_VIEW_TYPE_3D : VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = config.format;
            viewInfo.subresourceRange.aspectMask = config.aspect;
            viewInfo.subresourceRange.levelCount = 1;
            viewInfo.subresourceRange.layerCount = 1;
            checkVk(vkCreateImageView(device_->getLogicalDevice(), &viewInfo, nullptr, &view_), "vkCreateImageView");
        } catch (...) {
            destroy();
            throw;
        }
    }

    VulkanImage::~VulkanImage() {
        destroy();
    }

    VulkanImage::VulkanImage(VulkanImage&& other) noexcept
        : device_(std::exchange(other.device_, nullptr)), config_(other.config_),
          image_(std::exchange(other.image_, VK_NULL_HANDLE)), memory_(std::exchange(other.memory_, VK_NULL_HANDLE)),
          view_(std::exchange(other.view_, VK_NULL_HANDLE)) {
    }

    VulkanImage& VulkanImage::operator=(VulkanImage&& other) noexcept {
        if (this != &other) {
            destroy();
            device_ = std::exchange(other.device_, nullptr);
            config_ = other.config_;
            image_ = std::exchange(other.image_, VK_NULL_HANDLE);
            memory_ = std::exchange(other.memory_, VK_NULL_HANDLE);
            view_ = std::exchange(other.view_, VK_NULL_HANDLE);
        }
        return *this;
    }

    void VulkanImage::destroy() noexcept {
        if (device_ == nullptr) {
            return;
        }
        if (view_ != VK_NULL_HANDLE) {
            vkDestroyImageView(device_->getLogicalDevice(), view_, nullptr);
        }
        if (image_ != VK_NULL_HANDLE) {
            vkDestroyImage(device_->getLogicalDevice(), image_, nullptr);
        }
        if (memory_ != VK_NULL_HANDLE) {
            vkFreeMemory(device_->getLogicalDevice(), memory_, nullptr);
        }
        view_ = VK_NULL_HANDLE;
        image_ = VK_NULL_HANDLE;
        memory_ = VK_NULL_HANDLE;
        device_ = nullptr;
    }

} // namespace YATAVK
