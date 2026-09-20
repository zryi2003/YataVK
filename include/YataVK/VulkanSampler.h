//
// Created by Zhuoran Yi on 2026/1/9.
//

#ifndef YATA_VULKANSAMPLER_H
#define YATA_VULKANSAMPLER_H

#include "VulkanDevice.h"

namespace YATAVK::Legacy {
    class VulkanSampler final {
    public:
        VulkanSampler(VulkanDevice* device, VkFilter magFilter, VkFilter minFilter, VkSamplerAddressMode addressMode,
                      VkBorderColor borderColor, float maxAnisotropy)
            : device(device) {
            try {
                init(magFilter, minFilter, addressMode, borderColor, maxAnisotropy);
            } catch (const std::exception& e) {
                cleanUp();
                throw std::runtime_error("Failed to initialize Vulkan Swap Chain!");
            }
        }
        ~VulkanSampler() { cleanUp(); }
        VulkanSampler(const VulkanSampler&) = delete;
        VulkanSampler& operator=(const VulkanSampler&) = delete;

        [[nodiscard]] VkSampler getHandle() const { return vkSampler; }

    private:
        void init(VkFilter magFilter, VkFilter minFilter, VkSamplerAddressMode addressMode, VkBorderColor borderColor,
                  float maxAnisotropy);
        void cleanUp() const;

        VulkanDevice* device;
        VkSampler vkSampler;
    };
} // namespace YATAVK::Legacy

#endif // YATA_VULKANSAMPLER_H
