//
// Created by Zhuoran Yi on 2026/1/9.
//

#ifndef YATA_VULKANBUFFER_HPP
#define YATA_VULKANBUFFER_HPP

#include <iostream>

#include "VulkanDevice.h"

namespace YATAVK {
    class VulkanBuffer final {
    public:
        VulkanBuffer(VulkanDevice* device, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties) : device(device), vkBufferSize(size){
            try {
                init(usage, properties);
            } catch (const std::exception& e) {
                cleanUp();
                std::cerr << e.what() << std::endl;
                throw std::runtime_error("Failed to initialize Vulkan buffer!");
            }
        };
        ~VulkanBuffer() { cleanUp(); };
        VulkanBuffer(const VulkanBuffer&) = delete;
        VulkanBuffer& operator=(const VulkanBuffer&) = delete;

        [[nodiscard]] VkBuffer getHandle() const { return vkBuffer; }
        [[nodiscard]] VkDeviceMemory getDeviceMemory() const {
#ifdef YATAVK_ENABLE_VMA
            if (vmaAllocation) {
                VmaAllocationInfo info;
                vmaGetAllocationInfo(device->getVmaAllocator(), vmaAllocation, &info);
                return info.deviceMemory;
            } else {
                return VK_NULL_HANDLE;
            }
#else
            return vkBufferMemory;
#endif
        }
#ifdef YATAVK_ENABLE_VMA
        [[nodiscard]] VmaAllocation getVmaAllocation() const { return vmaAllocation; }
#endif
        [[nodiscard]] VkDeviceSize getSize() const { return vkBufferSize; }
        [[nodiscard]] void* getMappedMemory() const { return bufferData; } // Returns the host-accessible virtual address of the mapped buffer; returns nullptr if the buffer is not mapped.

        void map(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
        void unmap();
        void flush(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
        void writeToBuffer(void* data, VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);

    private:
        void init(VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);
        void cleanUp();

        VulkanDevice* device;

        VkBuffer vkBuffer = VK_NULL_HANDLE;
#ifdef YATAVK_ENABLE_VMA
        VmaAllocation vmaAllocation = VK_NULL_HANDLE;
#else
        VkDeviceMemory vkBufferMemory = VK_NULL_HANDLE;
#endif
        VkDeviceSize vkBufferSize;

        void* bufferData = nullptr;
    };
}

#endif //YATA_VULKANBUFFER_HPP