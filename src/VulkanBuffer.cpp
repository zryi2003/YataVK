#include "YataVK/VulkanBuffer.hpp"

#include "YataVK/VulkanError.hpp"

#include <cstddef>
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
        try {
#ifdef YATAVK_ENABLE_VMA
            VmaAllocationCreateInfo allocationInfo{};
            allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;
            allocationInfo.requiredFlags = memoryProperties;
            if ((memoryProperties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0) {
                allocationInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
            }
            checkVk(vmaCreateBuffer(device_->getVmaAllocator(), &bufferInfo, &allocationInfo, &buffer_, &allocation_,
                                    nullptr),
                    "vmaCreateBuffer");
#else
            checkVk(vkCreateBuffer(device_->getLogicalDevice(), &bufferInfo, nullptr, &buffer_), "vkCreateBuffer");
            // Vulkan 将资源和内存分离创建，实际分配大小由驱动给出的 requirements 决定。
            VkMemoryRequirements requirements{};
            vkGetBufferMemoryRequirements(device_->getLogicalDevice(), buffer_, &requirements);
            allocationSize_ = requirements.size;
            VkMemoryAllocateInfo allocationInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
            allocationInfo.allocationSize = requirements.size;
            allocationInfo.memoryTypeIndex = device_->findMemoryType(requirements.memoryTypeBits, memoryProperties);
            checkVk(vkAllocateMemory(device_->getLogicalDevice(), &allocationInfo, nullptr, &memory_),
                    "vkAllocateMemory(buffer)");
            checkVk(vkBindBufferMemory(device_->getLogicalDevice(), buffer_, memory_, 0), "vkBindBufferMemory");
#endif
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
#ifdef YATAVK_ENABLE_VMA
          allocation_(std::exchange(other.allocation_, VK_NULL_HANDLE)),
#else
          memory_(std::exchange(other.memory_, VK_NULL_HANDLE)),
          allocationSize_(std::exchange(other.allocationSize_, 0)),
#endif
          size_(std::exchange(other.size_, 0)), memoryProperties_(other.memoryProperties_),
          mapped_(std::exchange(other.mapped_, nullptr)), mappedOffset_(std::exchange(other.mappedOffset_, 0)),
          mappedSize_(std::exchange(other.mappedSize_, 0)) {
    }

    VulkanBuffer& VulkanBuffer::operator=(VulkanBuffer&& other) noexcept {
        if (this != &other) {
            destroy();
            device_ = std::exchange(other.device_, nullptr);
            buffer_ = std::exchange(other.buffer_, VK_NULL_HANDLE);
#ifdef YATAVK_ENABLE_VMA
            allocation_ = std::exchange(other.allocation_, VK_NULL_HANDLE);
#else
            memory_ = std::exchange(other.memory_, VK_NULL_HANDLE);
            allocationSize_ = std::exchange(other.allocationSize_, 0);
#endif
            size_ = std::exchange(other.size_, 0);
            memoryProperties_ = other.memoryProperties_;
            mapped_ = std::exchange(other.mapped_, nullptr);
            mappedOffset_ = std::exchange(other.mappedOffset_, 0);
            mappedSize_ = std::exchange(other.mappedSize_, 0);
        }
        return *this;
    }

    VkDeviceMemory VulkanBuffer::getDeviceMemory() const {
#ifdef YATAVK_ENABLE_VMA
        if (allocation_ == VK_NULL_HANDLE) {
            return VK_NULL_HANDLE;
        }
        VmaAllocationInfo info{};
        vmaGetAllocationInfo(device_->getVmaAllocator(), allocation_, &info);
        return info.deviceMemory;
#else
        return memory_;
#endif
    }

    VkDeviceSize VulkanBuffer::getMemoryOffset() const {
#ifdef YATAVK_ENABLE_VMA
        if (allocation_ == VK_NULL_HANDLE) {
            return 0;
        }
        VmaAllocationInfo info{};
        vmaGetAllocationInfo(device_->getVmaAllocator(), allocation_, &info);
        return info.offset;
#else
        return 0;
#endif
    }

    void VulkanBuffer::map(VkDeviceSize size, VkDeviceSize offset) {
        if ((memoryProperties_ & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == 0) {
            throw std::logic_error("Cannot map a non-host-visible VulkanBuffer");
        }
        if (offset >= size_ || (size != VK_WHOLE_SIZE && (size == 0 || size > size_ - offset))) {
            throw std::invalid_argument("VulkanBuffer::map range is invalid");
        }
        const VkDeviceSize mappedSize = size == VK_WHOLE_SIZE ? size_ - offset : size;
        if (mapped_ != nullptr) {
            if (offset < mappedOffset_ || offset - mappedOffset_ > mappedSize_ ||
                mappedSize > mappedSize_ - (offset - mappedOffset_)) {
                throw std::logic_error("VulkanBuffer is already mapped to a different range");
            }
            return;
        }
        void* base = nullptr;
#ifdef YATAVK_ENABLE_VMA
        checkVk(vmaMapMemory(device_->getVmaAllocator(), allocation_, &base), "vmaMapMemory");
#else
        // 映射整块内存，便于 non-coherent flush 覆盖对齐后的完整区间。
        checkVk(vkMapMemory(device_->getLogicalDevice(), memory_, 0, VK_WHOLE_SIZE, 0, &base), "vkMapMemory");
#endif
        mapped_ = static_cast<std::byte*>(base) + offset;
        mappedOffset_ = offset;
        mappedSize_ = mappedSize;
    }

    void VulkanBuffer::unmap() noexcept {
        if (mapped_ != nullptr) {
#ifdef YATAVK_ENABLE_VMA
            vmaUnmapMemory(device_->getVmaAllocator(), allocation_);
#else
            vkUnmapMemory(device_->getLogicalDevice(), memory_);
#endif
            mapped_ = nullptr;
            mappedOffset_ = 0;
            mappedSize_ = 0;
        }
    }

    void VulkanBuffer::write(const void* data, VkDeviceSize size, VkDeviceSize offset) {
        if (data == nullptr || size == 0 || offset > size_ || size > size_ - offset) {
            throw std::invalid_argument("VulkanBuffer::write range is invalid");
        }
        const bool temporaryMapping = mapped_ == nullptr;
        if (temporaryMapping) {
            map();
        }
        try {
            if (offset < mappedOffset_ || offset - mappedOffset_ > mappedSize_ ||
                size > mappedSize_ - (offset - mappedOffset_)) {
                throw std::invalid_argument("VulkanBuffer::write exceeds the mapped range");
            }
            std::memcpy(static_cast<std::byte*>(mapped_) + (offset - mappedOffset_), data, static_cast<size_t>(size));
            // 非 coherent 内存需要显式把 CPU 写入刷新给设备可见。
            if ((memoryProperties_ & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0) {
                flush(size, offset);
            }
        } catch (...) {
            if (temporaryMapping) {
                unmap();
            }
            throw;
        }
        if (temporaryMapping) {
            unmap();
        }
    }

    void VulkanBuffer::flush(VkDeviceSize size, VkDeviceSize offset) {
        if (mapped_ == nullptr || offset < mappedOffset_ || offset >= size_ ||
            (size != VK_WHOLE_SIZE && (size == 0 || size > size_ - offset))) {
            throw std::invalid_argument("VulkanBuffer::flush range is invalid");
        }
        const VkDeviceSize flushSize = size == VK_WHOLE_SIZE ? size_ - offset : size;
        if (offset - mappedOffset_ > mappedSize_ || flushSize > mappedSize_ - (offset - mappedOffset_)) {
            throw std::invalid_argument("VulkanBuffer::flush exceeds the mapped range");
        }
#ifdef YATAVK_ENABLE_VMA
        checkVk(vmaFlushAllocation(device_->getVmaAllocator(), allocation_, offset, flushSize),
                "vmaFlushAllocation");
#else
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(device_->getPhysicalDevice(), &properties);
        const VkDeviceSize atom = properties.limits.nonCoherentAtomSize;
        const VkDeviceSize alignedOffset = offset - offset % atom;
        const VkDeviceSize end = offset + flushSize;
        const VkDeviceSize padding = (atom - end % atom) % atom;
        const VkDeviceSize alignedEnd = padding > allocationSize_ - end ? allocationSize_ : end + padding;
        VkMappedMemoryRange range{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};
        range.memory = memory_;
        range.offset = alignedOffset;
        range.size = alignedEnd - alignedOffset;
        checkVk(vkFlushMappedMemoryRanges(device_->getLogicalDevice(), 1, &range), "vkFlushMappedMemoryRanges");
#endif
    }

    void VulkanBuffer::destroy() noexcept {
        if (device_ == nullptr) {
            return;
        }
        unmap();
#ifdef YATAVK_ENABLE_VMA
        if (allocation_ != VK_NULL_HANDLE) {
            vmaDestroyBuffer(device_->getVmaAllocator(), buffer_, allocation_);
        } else if (buffer_ != VK_NULL_HANDLE) {
            vkDestroyBuffer(device_->getLogicalDevice(), buffer_, nullptr);
        }
        allocation_ = VK_NULL_HANDLE;
#else
        // 先销毁绑定资源，再释放其底层内存。
        if (buffer_ != VK_NULL_HANDLE) {
            vkDestroyBuffer(device_->getLogicalDevice(), buffer_, nullptr);
        }
        if (memory_ != VK_NULL_HANDLE) {
            vkFreeMemory(device_->getLogicalDevice(), memory_, nullptr);
        }
        memory_ = VK_NULL_HANDLE;
        allocationSize_ = 0;
#endif
        buffer_ = VK_NULL_HANDLE;
        device_ = nullptr;
    }

} // namespace YATAVK
