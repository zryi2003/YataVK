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
        // depth 大于 1 时按体纹理创建，否则使用常规 2D image。
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
        try {
#ifdef YATAVK_ENABLE_VMA
            VmaAllocationCreateInfo allocationInfo{};
            allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;
            allocationInfo.requiredFlags = config.memoryProperties;
            if ((config.memoryProperties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0) {
                allocationInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
            }
            checkVk(vmaCreateImage(device_->getVmaAllocator(), &imageInfo, &allocationInfo, &image_, &allocation_,
                                   nullptr),
                    "vmaCreateImage");
#else
            checkVk(vkCreateImage(device_->getLogicalDevice(), &imageInfo, nullptr, &image_), "vkCreateImage");
            // Image 仅声明用途，内存类型仍需根据驱动 requirements 选择。
            VkMemoryRequirements requirements{};
            vkGetImageMemoryRequirements(device_->getLogicalDevice(), image_, &requirements);
            VkMemoryAllocateInfo allocationInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
            allocationInfo.allocationSize = requirements.size;
            allocationInfo.memoryTypeIndex =
                device_->findMemoryType(requirements.memoryTypeBits, config.memoryProperties);
            checkVk(vkAllocateMemory(device_->getLogicalDevice(), &allocationInfo, nullptr, &memory_),
                    "vkAllocateMemory(image)");
            checkVk(vkBindImageMemory(device_->getLogicalDevice(), image_, memory_, 0), "vkBindImageMemory");
#endif

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
          image_(std::exchange(other.image_, VK_NULL_HANDLE)),
#ifdef YATAVK_ENABLE_VMA
          allocation_(std::exchange(other.allocation_, VK_NULL_HANDLE)),
#else
          memory_(std::exchange(other.memory_, VK_NULL_HANDLE)),
#endif
          view_(std::exchange(other.view_, VK_NULL_HANDLE)) {
    }

    VulkanImage& VulkanImage::operator=(VulkanImage&& other) noexcept {
        if (this != &other) {
            destroy();
            device_ = std::exchange(other.device_, nullptr);
            config_ = other.config_;
            image_ = std::exchange(other.image_, VK_NULL_HANDLE);
#ifdef YATAVK_ENABLE_VMA
            allocation_ = std::exchange(other.allocation_, VK_NULL_HANDLE);
#else
            memory_ = std::exchange(other.memory_, VK_NULL_HANDLE);
#endif
            view_ = std::exchange(other.view_, VK_NULL_HANDLE);
        }
        return *this;
    }

    void VulkanImage::destroy() noexcept {
        if (device_ == nullptr) {
            return;
        }
        // view 引用 image，image 又引用 memory，按依赖顺序逆序释放。
        if (view_ != VK_NULL_HANDLE) {
            vkDestroyImageView(device_->getLogicalDevice(), view_, nullptr);
        }
#ifdef YATAVK_ENABLE_VMA
        if (allocation_ != VK_NULL_HANDLE) {
            vmaDestroyImage(device_->getVmaAllocator(), image_, allocation_);
        } else if (image_ != VK_NULL_HANDLE) {
            vkDestroyImage(device_->getLogicalDevice(), image_, nullptr);
        }
        allocation_ = VK_NULL_HANDLE;
#else
        if (image_ != VK_NULL_HANDLE) {
            vkDestroyImage(device_->getLogicalDevice(), image_, nullptr);
        }
        if (memory_ != VK_NULL_HANDLE) {
            vkFreeMemory(device_->getLogicalDevice(), memory_, nullptr);
        }
        memory_ = VK_NULL_HANDLE;
#endif
        view_ = VK_NULL_HANDLE;
        image_ = VK_NULL_HANDLE;
        device_ = nullptr;
    }

} // namespace YATAVK
