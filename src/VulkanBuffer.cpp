//
// Created by Zhuoran Yi on 2026/1/9.
//

#include "YataVK/VulkanBuffer.hpp"

namespace YATAVK {

    void VulkanBuffer::init(VkBufferUsageFlags usage, VkMemoryPropertyFlags properties) {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = vkBufferSize;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
#ifdef YATAVK_ENABLE_VMA
        VmaAllocationCreateInfo allocCreateInfo = {};
        allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
        if (properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
            allocCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                                    VMA_ALLOCATION_CREATE_MAPPED_BIT; // 自动持久化映射
        }

        if (vmaCreateBuffer(device->getVmaAllocator(), &bufferInfo, &allocCreateInfo, &vkBuffer, &vmaAllocation, nullptr) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create buffer with VMA!");
        }
#else
        if (vkCreateBuffer(device->getLogicalDevice(), &bufferInfo, nullptr, &vkBuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create buffer!");
        }

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(device->getLogicalDevice(), vkBuffer, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = device->findMemoryType(memRequirements.memoryTypeBits, properties);

        if (vkAllocateMemory(device->getLogicalDevice(), &allocInfo, nullptr, &vkBufferMemory) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate buffer memory!");
        }

        vkBindBufferMemory(device->getLogicalDevice(), vkBuffer, vkBufferMemory, 0);
#endif
    }

    void VulkanBuffer::cleanUp() {
        if (bufferData) {
            unmap();
            bufferData = nullptr;
        }
#ifdef YATAVK_ENABLE_VMA
        if (vmaAllocation != nullptr) {
            vmaDestroyBuffer(device->getVmaAllocator(), vkBuffer, vmaAllocation);
            vmaAllocation = nullptr;
            vkBuffer = VK_NULL_HANDLE;
        }
        if (vkBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(device->getLogicalDevice(), vkBuffer, nullptr);
            vkBuffer = VK_NULL_HANDLE;
        }
#else
        if (vkBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(device->getLogicalDevice(), vkBuffer, nullptr);
        }
        if (vkBufferMemory != VK_NULL_HANDLE) {
            vkFreeMemory(device->getLogicalDevice(), vkBufferMemory, nullptr);
        }
#endif
    }

    void VulkanBuffer::map(VkDeviceSize size, VkDeviceSize offset) {
#ifdef YATAVK_ENABLE_VMA
        void* ptr = nullptr;
        if (vmaMapMemory(device->getVmaAllocator(), vmaAllocation, &ptr) != VK_SUCCESS) {
            throw std::runtime_error("Failed to map buffer memory with VMA!");
        }
        bufferData = static_cast<char*>(ptr) + offset;
#else
        if (vkMapMemory(device->getLogicalDevice(), vkBufferMemory, offset, size, 0, &bufferData) != VK_SUCCESS) {
            throw std::runtime_error("Failed to map buffer memory!");
        }
#endif
    }

    void VulkanBuffer::unmap() {
        if (bufferData) {
#ifdef YATAVK_ENABLE_VMA
            vmaUnmapMemory(device->getVmaAllocator(), vmaAllocation);
#else
            vkUnmapMemory(device->getLogicalDevice(), vkBufferMemory);
#endif
            bufferData = nullptr;
        }
    }

    void VulkanBuffer::flush(VkDeviceSize size, VkDeviceSize offset) {
#ifdef YATAVK_ENABLE_VMA
        if (vmaAllocation) {
            vmaFlushAllocation(device->getVmaAllocator(), vmaAllocation, offset, size);
        }
#else
        if (vkBufferMemory != VK_NULL_HANDLE) {
            VkMappedMemoryRange mappedRange{};
            mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
            mappedRange.memory = vkBufferMemory;
            mappedRange.offset = offset;
            mappedRange.size = size;
            vkFlushMappedMemoryRanges(device->getLogicalDevice(), 1, &mappedRange);
        }
#endif
    }


    void VulkanBuffer::writeToBuffer(void *data, VkDeviceSize size, VkDeviceSize offset) {
        bool unmapAfter = false;
        if (!bufferData) {
            map(VK_WHOLE_SIZE, 0);
            unmapAfter = true;
        }
        std::memcpy(static_cast<char*>(bufferData) + offset, data, size == VK_WHOLE_SIZE ? vkBufferSize - offset : size);
        flush(size, offset);
        if (unmapAfter) {
            unmap();
        }
    }

}