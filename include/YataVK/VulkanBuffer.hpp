#pragma once

#include "YataVK/VulkanDevice.h"

#include <vulkan/vulkan.h>

namespace YATAVK {

    class VulkanBuffer final {
    public:
        VulkanBuffer(VulkanDevice& device, VkDeviceSize size, VkBufferUsageFlags usage,
                     VkMemoryPropertyFlags memoryProperties);
        VulkanBuffer(VulkanDevice* device, VkDeviceSize size, VkBufferUsageFlags usage,
                     VkMemoryPropertyFlags memoryProperties)
            : VulkanBuffer(*device, size, usage, memoryProperties) {}
        ~VulkanBuffer();

        VulkanBuffer(const VulkanBuffer&) = delete;
        VulkanBuffer& operator=(const VulkanBuffer&) = delete;
        VulkanBuffer(VulkanBuffer&& other) noexcept;
        VulkanBuffer& operator=(VulkanBuffer&& other) noexcept;

        void map(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
        void unmap() noexcept;
        void write(const void* data, VkDeviceSize size, VkDeviceSize offset = 0);
        void flush(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);

        // 兼容旧调用方式；新代码优先使用 write()。
        void writeToBuffer(void* data, VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0) {
            write(data, size == VK_WHOLE_SIZE && offset <= size_ ? size_ - offset : size, offset);
        }

        [[nodiscard]] VkBuffer getHandle() const { return buffer_; }
        // VMA 模式下底层内存可能被共享，需连同 getMemoryOffset() 一起使用。
        [[nodiscard]] VkDeviceMemory getDeviceMemory() const;
        [[nodiscard]] VkDeviceSize getMemoryOffset() const;
        [[nodiscard]] VkDeviceSize getSize() const { return size_; }
        [[nodiscard]] void* getMappedMemory() const { return mapped_; }

    private:
        void destroy() noexcept;

        VulkanDevice* device_ = nullptr;
        VkBuffer buffer_ = VK_NULL_HANDLE;
#ifdef YATAVK_ENABLE_VMA
        VmaAllocation allocation_ = VK_NULL_HANDLE;
#else
        VkDeviceMemory memory_ = VK_NULL_HANDLE;
        VkDeviceSize allocationSize_ = 0;
#endif
        VkDeviceSize size_ = 0;
        VkMemoryPropertyFlags memoryProperties_ = 0;
        void* mapped_ = nullptr;
        VkDeviceSize mappedOffset_ = 0;
        VkDeviceSize mappedSize_ = 0;
    };

} // namespace YATAVK
