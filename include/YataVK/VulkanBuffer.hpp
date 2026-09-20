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

        // Compatibility alias retained for existing YataVK users.
        void writeToBuffer(void* data, VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0) {
            write(data, size == VK_WHOLE_SIZE ? size_ - offset : size, offset);
        }

        [[nodiscard]] VkBuffer getHandle() const { return buffer_; }
        [[nodiscard]] VkDeviceMemory getDeviceMemory() const { return memory_; }
        [[nodiscard]] VkDeviceSize getSize() const { return size_; }
        [[nodiscard]] void* getMappedMemory() const { return mapped_; }

    private:
        void destroy() noexcept;

        VulkanDevice* device_ = nullptr;
        VkBuffer buffer_ = VK_NULL_HANDLE;
        VkDeviceMemory memory_ = VK_NULL_HANDLE;
        VkDeviceSize size_ = 0;
        VkMemoryPropertyFlags memoryProperties_ = 0;
        void* mapped_ = nullptr;
    };

} // namespace YATAVK
