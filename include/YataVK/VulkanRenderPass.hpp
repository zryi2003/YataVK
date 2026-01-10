//
// Created by Zhuoran Yi on 2026/1/9.
//

#ifndef YATA_VULKANRENDERPASS_HPP
#define YATA_VULKANRENDERPASS_HPP

#include <concepts>

#include <vulkan/vulkan_core.h>

#include "VulkanDevice.h"

namespace YATAVK {
    template<typename T>
    concept RenderPassProvider = requires(T t)
    {
        { t.getCreateInfo() } -> std::convertible_to<VkRenderPassCreateInfo>;
    };

    template <RenderPassProvider T>
    class VulkanRenderPass final {
    public:
        VulkanRenderPass(VulkanDevice* device, T&& provider) : device(device) {
            VkRenderPassCreateInfo createInfo = provider.getCreateInfo();
            if (vkCreateRenderPass(device->getLogicalDevice(), &createInfo, nullptr, &vkRenderPass) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create render pass!");
            }
        }

        ~VulkanRenderPass() {
            if (vkRenderPass != VK_NULL_HANDLE) {
                vkDestroyRenderPass(device->getLogicalDevice(), vkRenderPass, nullptr);
            }
        }

        VkRenderPass getHandle() const { return vkRenderPass; }

        VulkanRenderPass(const VulkanRenderPass&) = delete;
        VulkanRenderPass& operator=(const VulkanRenderPass&) = delete;
    private:
        VulkanDevice* device;

        VkRenderPass vkRenderPass = VK_NULL_HANDLE;
    };
}

#endif //YATA_VULKANRENDERPASS_HPP