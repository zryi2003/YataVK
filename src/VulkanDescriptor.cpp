#include "YataVK/VulkanDescriptor.hpp"

#include "YataVK/VulkanError.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace YATAVK {

    VulkanDescriptorPool::VulkanDescriptorPool(VulkanDevice& device, const VulkanDescriptorPoolConfig& config)
        : device_(&device) {
        if (config.maxSets == 0 || config.sizes.empty()) {
            throw std::invalid_argument("VulkanDescriptorPoolConfig requires sizes and a non-zero maxSets");
        }
        VkDescriptorPoolCreateInfo createInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        createInfo.flags = config.flags;
        createInfo.maxSets = config.maxSets;
        createInfo.poolSizeCount = static_cast<uint32_t>(config.sizes.size());
        createInfo.pPoolSizes = config.sizes.data();
        checkVk(vkCreateDescriptorPool(device_->getLogicalDevice(), &createInfo, nullptr, &pool_),
                "vkCreateDescriptorPool");
    }

    VulkanDescriptorPool::~VulkanDescriptorPool() {
        destroy();
    }

    VulkanDescriptorPool::VulkanDescriptorPool(VulkanDescriptorPool&& other) noexcept
        : device_(std::exchange(other.device_, nullptr)), pool_(std::exchange(other.pool_, VK_NULL_HANDLE)) {
    }

    VulkanDescriptorPool& VulkanDescriptorPool::operator=(VulkanDescriptorPool&& other) noexcept {
        if (this != &other) {
            destroy();
            device_ = std::exchange(other.device_, nullptr);
            pool_ = std::exchange(other.pool_, VK_NULL_HANDLE);
        }
        return *this;
    }

    bool VulkanDescriptorPool::allocate(VkDescriptorSetLayout layout, VkDescriptorSet& set) const {
        VkDescriptorSetAllocateInfo allocationInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        allocationInfo.descriptorPool = pool_;
        allocationInfo.descriptorSetCount = 1;
        allocationInfo.pSetLayouts = &layout;
        return vkAllocateDescriptorSets(device_->getLogicalDevice(), &allocationInfo, &set) == VK_SUCCESS;
    }

    void VulkanDescriptorPool::reset() const {
        // reset 会一次性回收该 pool 分配出的所有 descriptor sets。
        checkVk(vkResetDescriptorPool(device_->getLogicalDevice(), pool_, 0), "vkResetDescriptorPool");
    }

    void VulkanDescriptorPool::destroy() noexcept {
        if (device_ != nullptr && pool_ != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(device_->getLogicalDevice(), pool_, nullptr);
        }
        pool_ = VK_NULL_HANDLE;
        device_ = nullptr;
    }

    VulkanDescriptorSetLayout::Builder& VulkanDescriptorSetLayout::Builder::addBinding(uint32_t binding,
                                                                                       VkDescriptorType type,
                                                                                       VkShaderStageFlags stages,
                                                                                       uint32_t count) {
        if (bindings_.contains(binding)) {
            throw std::invalid_argument("Descriptor binding is already defined");
        }
        bindings_.emplace(binding, VkDescriptorSetLayoutBinding{binding, type, count, stages, nullptr});
        return *this;
    }

    std::unique_ptr<VulkanDescriptorSetLayout> VulkanDescriptorSetLayout::Builder::build() const {
        return std::make_unique<VulkanDescriptorSetLayout>(*device_, bindings_);
    }

    VulkanDescriptorSetLayout::VulkanDescriptorSetLayout(
        VulkanDevice& device, const std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding>& bindings)
        : device_(&device), bindings_(bindings) {
        std::vector<VkDescriptorSetLayoutBinding> sorted;
        sorted.reserve(bindings.size());
        for (const auto& [_, binding] : bindings) {
            sorted.push_back(binding);
        }
        // 固定 binding 顺序，便于调试和复现创建参数。
        std::sort(sorted.begin(), sorted.end(),
                  [](const auto& left, const auto& right) { return left.binding < right.binding; });
        VkDescriptorSetLayoutCreateInfo createInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        createInfo.bindingCount = static_cast<uint32_t>(sorted.size());
        createInfo.pBindings = sorted.data();
        checkVk(vkCreateDescriptorSetLayout(device_->getLogicalDevice(), &createInfo, nullptr, &layout_),
                "vkCreateDescriptorSetLayout");
    }

    VulkanDescriptorSetLayout::~VulkanDescriptorSetLayout() {
        destroy();
    }

    VulkanDescriptorSetLayout::VulkanDescriptorSetLayout(VulkanDescriptorSetLayout&& other) noexcept
        : device_(std::exchange(other.device_, nullptr)), layout_(std::exchange(other.layout_, VK_NULL_HANDLE)),
          bindings_(std::move(other.bindings_)) {
    }

    VulkanDescriptorSetLayout& VulkanDescriptorSetLayout::operator=(VulkanDescriptorSetLayout&& other) noexcept {
        if (this != &other) {
            destroy();
            device_ = std::exchange(other.device_, nullptr);
            layout_ = std::exchange(other.layout_, VK_NULL_HANDLE);
            bindings_ = std::move(other.bindings_);
        }
        return *this;
    }

    void VulkanDescriptorSetLayout::destroy() noexcept {
        if (device_ != nullptr && layout_ != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(device_->getLogicalDevice(), layout_, nullptr);
        }
        layout_ = VK_NULL_HANDLE;
        device_ = nullptr;
    }

    VulkanDescriptorWriter::VulkanDescriptorWriter(VulkanDevice& device, const VulkanDescriptorPool& pool,
                                                   VulkanDescriptorSetLayout& layout)
        : device_(&device), pool_(&pool), layout_(&layout) {
    }

    VulkanDescriptorWriter& VulkanDescriptorWriter::writeBuffer(uint32_t binding,
                                                                const VkDescriptorBufferInfo& bufferInfo) {
        const auto found = layout_->bindings_.find(binding);
        if (found == layout_->bindings_.end() || found->second.descriptorCount != 1) {
            throw std::invalid_argument("Descriptor buffer binding is absent or is not scalar");
        }
        switch (found->second.descriptorType) {
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
            break;
        default:
            throw std::invalid_argument("Descriptor binding does not accept buffer info");
        }
        bufferInfos_.push_back(bufferInfo);
        PendingWrite pending;
        pending.write.dstBinding = binding;
        pending.write.descriptorCount = 1;
        pending.write.descriptorType = found->second.descriptorType;
        pending.infoIndex = bufferInfos_.size() - 1;
        pending.usesImageInfo = false;
        pendingWrites_.push_back(pending);
        return *this;
    }

    VulkanDescriptorWriter& VulkanDescriptorWriter::writeImage(uint32_t binding,
                                                               const VkDescriptorImageInfo& imageInfo) {
        const auto found = layout_->bindings_.find(binding);
        if (found == layout_->bindings_.end() || found->second.descriptorCount != 1) {
            throw std::invalid_argument("Descriptor image binding is absent or is not scalar");
        }
        switch (found->second.descriptorType) {
        case VK_DESCRIPTOR_TYPE_SAMPLER:
        case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
        case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
        case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
        case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
            break;
        default:
            throw std::invalid_argument("Descriptor binding does not accept image info");
        }
        imageInfos_.push_back(imageInfo);
        PendingWrite pending;
        pending.write.dstBinding = binding;
        pending.write.descriptorCount = 1;
        pending.write.descriptorType = found->second.descriptorType;
        pending.infoIndex = imageInfos_.size() - 1;
        pending.usesImageInfo = true;
        pendingWrites_.push_back(pending);
        return *this;
    }

    bool VulkanDescriptorWriter::build(VkDescriptorSet& set) {
        if (!pool_->allocate(layout_->getHandle(), set)) {
            return false;
        }
        overwrite(set);
        return true;
    }

    void VulkanDescriptorWriter::overwrite(VkDescriptorSet set) {
        // 组装阶段只存索引；容器稳定后再生成裸指针，避免扩容留下悬空地址。
        std::vector<VkWriteDescriptorSet> writes;
        writes.reserve(pendingWrites_.size());
        for (const PendingWrite& pending : pendingWrites_) {
            VkWriteDescriptorSet write = pending.write;
            write.dstSet = set;
            if (pending.usesImageInfo) {
                write.pImageInfo = &imageInfos_.at(pending.infoIndex);
            } else {
                write.pBufferInfo = &bufferInfos_.at(pending.infoIndex);
            }
            writes.push_back(write);
        }
        vkUpdateDescriptorSets(device_->getLogicalDevice(), static_cast<uint32_t>(writes.size()), writes.data(), 0,
                               nullptr);
    }

} // namespace YATAVK
