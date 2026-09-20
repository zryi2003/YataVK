#include "YataVK/VulkanBuffer.hpp"

#include "YataVK/VulkanError.hpp"

#include <cstring>
#include <stdexcept>
#include <utility>

namespace YATAVK {

    VulkanBuffer::VulkanBuffer(VulkanDevice& device, VkDeviceSize size, VkBufferUsageFlags usage,
                               VkMemoryPropertyFlags memoryProperties)
        : device_(&device), size_(size), memoryProperties_(memoryProperties) {
        if (size == 0) {
            throw std::invalid_argument("VulkanBuffer size must be non-zero");
        }
        VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        checkVk(vkCreateBuffer(device_->getLogicalDevice(), &bufferInfo, nullptr, &buffer_), "vkCreateBuffer");
        try {
            VkMemoryRequirements requirements{};
            vkGetBufferMemoryRequirements(device_->getLogicalDevice(), buffer_, &requirements);
            VkMemoryAllocateInfo allocationInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
            allocationInfo.allocationSize = requirements.size;
            allocationInfo.memoryTypeIndex = device_->findMemoryType(requirements.memoryTypeBits, memoryProperties);
            checkVk(vkAllocateMemory(device_->getLogicalDevice(), &allocationInfo, nullptr, &memory_),
                    "vkAllocateMemory(buffer)");
            checkVk(vkBindBufferMemory(device_->getLogicalDevice(), buffer_, memory_, 0), "vkBindBufferMemory");
        } catch (...) {
            destroy();
            throw;
        }
    }

    VulkanBuffer::~VulkanBuffer() {
        destroy();
    }

    VulkanBuffer::VulkanBuffer(VulkanBuffer&& other) noexcept
        : device_(std::exchange(other.device_, nullptr)), buffer_(std::exchange(other.buffer_, VK_NULL_HANDLE)),
          memory_(std::exchange(other.memory_, VK_NULL_HANDLE)), size_(std::exchange(other.size_, 0)),
          memoryProperties_(other.memoryProperties_), mapped_(std::exchange(other.mapped_, nullptr)) {
    }

    VulkanBuffer& VulkanBuffer::operator=(VulkanBuffer&& other) noexcept {
        if (this != &other) {
            destroy();
            device_ = std::exchange(other.device_, nullptr);
            buffer_ = std::exchange(other.buffer_, VK_NULL_HANDLE);
            memory_ = std::exchange(other.memory_, VK_NULL_HANDLE);
            size_ = std::exchange(other.size_, 0);
            memoryProperties_ = other.memoryProperties_;
            mapped_ = std::exchange(other.mapped_, nullptr);
        }
        return *this;
    }

    void VulkanBuffer::map(VkDeviceSize size, VkDeviceSize offset) {
        if ((memoryProperties_ & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == 0) {
            throw std::logic_error("Cannot map a non-host-visible VulkanBuffer");
        }
        if (mapped_ != nullptr) {
            return;
        }
        checkVk(vkMapMemory(device_->getLogicalDevice(), memory_, offset, size, 0, &mapped_), "vkMapMemory");
    }

    void VulkanBuffer::unmap() noexcept {
        if (mapped_ != nullptr) {
            vkUnmapMemory(device_->getLogicalDevice(), memory_);
            mapped_ = nullptr;
        }
    }

    void VulkanBuffer::write(const void* data, VkDeviceSize size, VkDeviceSize offset) {
        if (data == nullptr || size == 0 || offset + size > size_) {
            throw std::invalid_argument("VulkanBuffer::write range is invalid");
        }
        const bool temporaryMapping = mapped_ == nullptr;
        if (temporaryMapping) {
            map();
        }
        std::memcpy(static_cast<std::byte*>(mapped_) + offset, data, static_cast<size_t>(size));
        if ((memoryProperties_ & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0) {
            flush(size, offset);
        }
        if (temporaryMapping) {
            unmap();
        }
    }

    void VulkanBuffer::flush(VkDeviceSize size, VkDeviceSize offset) {
        VkMappedMemoryRange range{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};
        range.memory = memory_;
        range.offset = offset;
        range.size = size;
        checkVk(vkFlushMappedMemoryRanges(device_->getLogicalDevice(), 1, &range), "vkFlushMappedMemoryRanges");
    }

    void VulkanBuffer::destroy() noexcept {
        if (device_ == nullptr) {
            return;
        }
        unmap();
        if (buffer_ != VK_NULL_HANDLE) {
            vkDestroyBuffer(device_->getLogicalDevice(), buffer_, nullptr);
        }
        if (memory_ != VK_NULL_HANDLE) {
            vkFreeMemory(device_->getLogicalDevice(), memory_, nullptr);
        }
        buffer_ = VK_NULL_HANDLE;
        memory_ = VK_NULL_HANDLE;
        device_ = nullptr;
    }

} // namespace YATAVK
