//
// Created by Zhuoran Yi on 2026/2/11.
//

#include "YataVK/VulkanDescriptor.hpp"
#include <stdexcept>
#include <cassert>

namespace YATAVK {

    // ================= Builder Implementation =================

    VulkanDescriptorSetLayout::Builder& VulkanDescriptorSetLayout::Builder::addBinding(
        uint32_t binding,
        VkDescriptorType descriptorType,
        VkShaderStageFlags stageFlags,
        uint32_t count) {

        assert(bindings.count(binding) == 0 && "Binding already in use");

        VkDescriptorSetLayoutBinding layoutBinding{};
        layoutBinding.binding = binding;
        layoutBinding.descriptorType = descriptorType;
        layoutBinding.descriptorCount = count;
        layoutBinding.stageFlags = stageFlags;

        bindings[binding] = layoutBinding;
        return *this;
    }

    std::unique_ptr<VulkanDescriptorSetLayout> VulkanDescriptorSetLayout::Builder::build() const {
        return std::make_unique<VulkanDescriptorSetLayout>(device, bindings);
    }

    // ================= Layout Implementation =================

    VulkanDescriptorSetLayout::VulkanDescriptorSetLayout(VulkanDevice* device, std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings)
        : device{device}, bindings{bindings} {

        std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings{};
        for (auto kv : bindings) {
            setLayoutBindings.push_back(kv.second);
        }

        VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo{};
        descriptorSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descriptorSetLayoutInfo.bindingCount = static_cast<uint32_t>(setLayoutBindings.size());
        descriptorSetLayoutInfo.pBindings = setLayoutBindings.data();

        if (vkCreateDescriptorSetLayout(device->getLogicalDevice(), &descriptorSetLayoutInfo, nullptr, &vkDescriptorSetLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create descriptor set layout!");
        }
    }

    VulkanDescriptorSetLayout::~VulkanDescriptorSetLayout() {
        vkDestroyDescriptorSetLayout(device->getLogicalDevice(), vkDescriptorSetLayout, nullptr);
    }

    // ================= Writer Implementation =================

    VulkanDescriptorWriter::VulkanDescriptorWriter(VulkanDevice* device, VulkanDescriptorSetLayout& setLayout)
        : device{device}, descriptorSetLayout{setLayout}{}

    VulkanDescriptorWriter& VulkanDescriptorWriter::writeBuffer(uint32_t binding, VkDescriptorBufferInfo* bufferInfo) {
        assert(descriptorSetLayout.bindings.count(binding) == 1 && "Layout does not contain specified binding");

        auto& bindingDescription = descriptorSetLayout.bindings[binding];

        assert(bindingDescription.descriptorCount == 1 && "Binding single descriptor info, but binding expects multiple");

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.descriptorType = bindingDescription.descriptorType;
        write.dstBinding = binding;
        write.pBufferInfo = bufferInfo;
        write.descriptorCount = 1;

        writes.push_back(write);
        return *this;
    }

    VulkanDescriptorWriter& VulkanDescriptorWriter::writeImage(uint32_t binding, VkDescriptorImageInfo* imageInfo) {
        assert(descriptorSetLayout.bindings.count(binding) == 1 && "Layout does not contain specified binding");

        auto& bindingDescription = descriptorSetLayout.bindings[binding];

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.descriptorType = bindingDescription.descriptorType;
        write.dstBinding = binding;
        write.pImageInfo = imageInfo;
        write.descriptorCount = 1;

        writes.push_back(write);
        return *this;
    }

    bool VulkanDescriptorWriter::build(VkDescriptorSet& set) {
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = device->getDescriptorPool(); // 使用 Device 中的全局 Pool
        allocInfo.pSetLayouts = &descriptorSetLayout.vkDescriptorSetLayout;
        allocInfo.descriptorSetCount = 1;

        if (vkAllocateDescriptorSets(device->getLogicalDevice(), &allocInfo, &set) != VK_SUCCESS) {
            return false;
        }
        overwrite(set);
        return true;
    }

    void VulkanDescriptorWriter::overwrite(VkDescriptorSet& set) {
        for (auto& write : writes) {
            write.dstSet = set;
        }
        vkUpdateDescriptorSets(device->getLogicalDevice(), writes.size(), writes.data(), 0, nullptr);
    }
}
