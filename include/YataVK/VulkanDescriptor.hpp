//
// Created by Zhuoran Yi on 2026/2/11.
//

#ifndef YATA_VULKANDESCRIPTOR_HPP
#define YATA_VULKANDESCRIPTOR_HPP

#include "VulkanDevice.h"
#include <memory>
#include <unordered_map>

namespace YATAVK {
    class VulkanDescriptorSetLayout final {
    public:
        class Builder {
        public:
            Builder(VulkanDevice* device) : device(device) {}

            Builder& addBinding(uint32_t binding, VkDescriptorType descriptorType, VkShaderStageFlags stageFlags, uint32_t count = 1);
            std::unique_ptr<VulkanDescriptorSetLayout> build() const;
        private:
            VulkanDevice* device;
            std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings{};
        };

        VulkanDescriptorSetLayout(VulkanDevice* device, std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings);
        ~VulkanDescriptorSetLayout();

        VulkanDescriptorSetLayout(const VulkanDescriptorSetLayout&) = delete;
        VulkanDescriptorSetLayout& operator=(const VulkanDescriptorSetLayout&) = delete;

        [[nodiscard]] VkDescriptorSetLayout getHandle() const { return vkDescriptorSetLayout; }

    private:
        VulkanDevice* device;
        VkDescriptorSetLayout vkDescriptorSetLayout = VK_NULL_HANDLE;
        std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings;

        friend class VulkanDescriptorWriter;
    };

    class VulkanDescriptorWriter final {
    public:
        VulkanDescriptorWriter(VulkanDevice* device, VulkanDescriptorSetLayout& setLayout);

        VulkanDescriptorWriter& writeBuffer(uint32_t binding, VkDescriptorBufferInfo* bufferInfo);
        VulkanDescriptorWriter& writeImage(uint32_t binding, VkDescriptorImageInfo* imageInfo);

        bool build(VkDescriptorSet& set);
        void overwrite(VkDescriptorSet& set);

    private:
        VulkanDevice* device;
        VulkanDescriptorSetLayout& descriptorSetLayout;

        std::vector<VkWriteDescriptorSet> writes;
    };
}

#endif //YATA_VULKANDESCRIPTOR_HPP