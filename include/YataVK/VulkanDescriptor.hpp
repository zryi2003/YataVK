#pragma once

#include "YataVK/VulkanDevice.h"

#include <cstddef>
#include <memory>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.h>

namespace YATAVK {

    struct VulkanDescriptorPoolConfig {
        uint32_t maxSets = 64;
        VkDescriptorPoolCreateFlags flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        std::vector<VkDescriptorPoolSize> sizes;
    };

    class VulkanDescriptorPool final {
    public:
        VulkanDescriptorPool(VulkanDevice& device, const VulkanDescriptorPoolConfig& config);
        ~VulkanDescriptorPool();

        VulkanDescriptorPool(const VulkanDescriptorPool&) = delete;
        VulkanDescriptorPool& operator=(const VulkanDescriptorPool&) = delete;
        VulkanDescriptorPool(VulkanDescriptorPool&& other) noexcept;
        VulkanDescriptorPool& operator=(VulkanDescriptorPool&& other) noexcept;

        [[nodiscard]] VkDescriptorPool getHandle() const { return pool_; }
        [[nodiscard]] bool allocate(VkDescriptorSetLayout layout, VkDescriptorSet& set) const;
        void reset() const;

    private:
        void destroy() noexcept;

        VulkanDevice* device_ = nullptr;
        VkDescriptorPool pool_ = VK_NULL_HANDLE;
    };

    class VulkanDescriptorSetLayout final {
    public:
        class Builder {
        public:
            explicit Builder(VulkanDevice& device) : device_(&device) {}
            explicit Builder(VulkanDevice* device) : device_(device) {}
            Builder& addBinding(uint32_t binding, VkDescriptorType type, VkShaderStageFlags stages, uint32_t count = 1);
            [[nodiscard]] std::unique_ptr<VulkanDescriptorSetLayout> build() const;

        private:
            VulkanDevice* device_;
            std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings_;
        };

        VulkanDescriptorSetLayout(VulkanDevice& device,
                                  const std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding>& bindings);
        ~VulkanDescriptorSetLayout();

        VulkanDescriptorSetLayout(const VulkanDescriptorSetLayout&) = delete;
        VulkanDescriptorSetLayout& operator=(const VulkanDescriptorSetLayout&) = delete;
        VulkanDescriptorSetLayout(VulkanDescriptorSetLayout&& other) noexcept;
        VulkanDescriptorSetLayout& operator=(VulkanDescriptorSetLayout&& other) noexcept;

        [[nodiscard]] VkDescriptorSetLayout getHandle() const { return layout_; }

    private:
        void destroy() noexcept;

        VulkanDevice* device_ = nullptr;
        VkDescriptorSetLayout layout_ = VK_NULL_HANDLE;
        std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings_;

        friend class VulkanDescriptorWriter;
    };

    class VulkanDescriptorWriter final {
    public:
        VulkanDescriptorWriter(VulkanDevice& device, const VulkanDescriptorPool& pool,
                               VulkanDescriptorSetLayout& layout);
        VulkanDescriptorWriter& writeBuffer(uint32_t binding, const VkDescriptorBufferInfo& bufferInfo);
        VulkanDescriptorWriter& writeImage(uint32_t binding, const VkDescriptorImageInfo& imageInfo);
        [[nodiscard]] bool build(VkDescriptorSet& set);
        void overwrite(VkDescriptorSet set);

    private:
        struct PendingWrite {
            VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            size_t infoIndex = 0;
            bool usesImageInfo = false;
        };

        VulkanDevice* device_;
        const VulkanDescriptorPool* pool_;
        VulkanDescriptorSetLayout* layout_;
        std::vector<VkDescriptorBufferInfo> bufferInfos_;
        std::vector<VkDescriptorImageInfo> imageInfos_;
        std::vector<PendingWrite> pendingWrites_;
    };

} // namespace YATAVK
