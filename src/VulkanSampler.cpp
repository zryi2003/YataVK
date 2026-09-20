//
// Created by Zhuoran Yi on 2026/1/9.
//

#include "YataVK/VulkanSampler.h"

namespace YATAVK::Legacy {
    void VulkanSampler::init(VkFilter magFilter, VkFilter minFilter, VkSamplerAddressMode addressMode,
                             VkBorderColor borderColor, float maxAnisotropy) {
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = magFilter;
        samplerInfo.minFilter = minFilter;
        samplerInfo.addressModeU = addressMode;
        samplerInfo.addressModeV = addressMode;
        samplerInfo.addressModeW = addressMode;
        samplerInfo.borderColor = borderColor; // 这里设定返回什么颜色

        samplerInfo.anisotropyEnable = (maxAnisotropy > 1.0f) ? VK_TRUE : VK_FALSE;
        samplerInfo.maxAnisotropy = maxAnisotropy;

        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 1.0f;

        if (vkCreateSampler(device->getLogicalDevice(), &samplerInfo, nullptr, &vkSampler) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create texture sampler!");
        }
    }

    void VulkanSampler::cleanUp() const {
        if (vkSampler != VK_NULL_HANDLE) {
            vkDestroySampler(device->getLogicalDevice(), vkSampler, nullptr);
        }
    }

} // namespace YATAVK::Legacy
