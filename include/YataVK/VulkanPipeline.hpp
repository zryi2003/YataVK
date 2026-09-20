//
// Created by Zhuoran Yi on 2026/1/9.
//

#ifndef YATA_VULKANPIPELINE_HPP
#define YATA_VULKANPIPELINE_HPP

#include "VulkanDevice.h"

#include <concepts>
#include <vulkan/vulkan_core.h>

namespace YATAVK::Legacy {
    template <typename T>
    concept PipelineProvider = requires(T t, VkPipelineLayout layout) {
        { t.getLayoutCreateInfo() } -> std::convertible_to<VkPipelineLayoutCreateInfo>;
        { t.getPipelineCreateInfo(layout) };
    };

    class IVulkanPipeline {
    public:
        virtual ~IVulkanPipeline() = default;
        [[nodiscard]] virtual VkPipeline getHandle() const = 0;
        [[nodiscard]] virtual VkPipelineLayout getLayout() const = 0;
    };

    template <PipelineProvider T> class VulkanPipeline final : public IVulkanPipeline {
    public:
        VulkanPipeline(VulkanDevice* device, T&& provider) : device(device) {
            VkPipelineLayoutCreateInfo layoutInfo = provider.getLayoutCreateInfo();
            if (vkCreatePipelineLayout(device->getLogicalDevice(), &layoutInfo, nullptr, &vkPipelineLayout) !=
                VK_SUCCESS) {
                throw std::runtime_error("failed to create pipeline layout");
            }

            auto pipelineInfo = provider.getPipelineCreateInfo(vkPipelineLayout);

            if constexpr (std::is_same_v<decltype(pipelineInfo), VkGraphicsPipelineCreateInfo>) {
                if (vkCreateGraphicsPipelines(device->getLogicalDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
                                              &vkPipeline) != VK_SUCCESS) {
                    throw std::runtime_error("Failed to create graphics pipeline");
                }
            } else if constexpr (std::is_same_v<decltype(pipelineInfo), VkComputePipelineCreateInfo>) {
                if (vkCreateComputePipelines(device->getLogicalDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
                                             &vkPipeline) != VK_SUCCESS) {
                    throw std::runtime_error("Failed to create compute pipeline");
                }
            } else {
                static_assert(
                    std::is_same_v<decltype(pipelineInfo), VkGraphicsPipelineCreateInfo> ||
                        std::is_same_v<decltype(pipelineInfo), VkComputePipelineCreateInfo>,
                    "PipelineProvider must return either VkGraphicsPipelineCreateInfo or VkComputePipelineCreateInfo");
            }
        }

        ~VulkanPipeline() {
            if (vkPipeline != VK_NULL_HANDLE) {
                vkDestroyPipeline(device->getLogicalDevice(), vkPipeline, nullptr);
            }
            if (vkPipelineLayout != VK_NULL_HANDLE) {
                vkDestroyPipelineLayout(device->getLogicalDevice(), vkPipelineLayout, nullptr);
            }
        }

        VulkanPipeline(const VulkanPipeline&) = delete;
        VulkanPipeline& operator=(const VulkanPipeline&) = delete;

        [[nodiscard]] VkPipeline getHandle() const { return vkPipeline; }
        [[nodiscard]] VkPipelineLayout getLayout() const { return vkPipelineLayout; }

    private:
        VulkanDevice* device;

        VkPipelineLayout vkPipelineLayout = VK_NULL_HANDLE;
        VkPipeline vkPipeline = VK_NULL_HANDLE;
    };
} // namespace YATAVK::Legacy

#endif // YATA_VULKANPIPELINE_HPP
