#pragma once

#include "YataVK/VulkanDevice.h"
#include "YataVK/VulkanShaderModule.hpp"

#include <vector>
#include <vulkan/vulkan.h>

namespace YATAVK {

    struct VulkanGraphicsPipelineConfig {
        VkFormat colorFormat = VK_FORMAT_UNDEFINED;
        VkFormat depthFormat = VK_FORMAT_UNDEFINED;
        const VulkanShaderModule* vertexShader = nullptr;
        const VulkanShaderModule* fragmentShader = nullptr;
        std::vector<VkVertexInputBindingDescription> vertexBindings;
        std::vector<VkVertexInputAttributeDescription> vertexAttributes;
        // 顺序对应 shader 中的 set 编号；句柄只在创建 pipeline layout 时读取。
        std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
        std::vector<VkPushConstantRange> pushConstants;
        VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT;
        VkFrontFace frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        bool depthTest = true;
        bool depthWrite = true;
        bool alphaBlend = false;
    };

    class VulkanGraphicsPipeline final {
    public:
        VulkanGraphicsPipeline(VulkanDevice& device, const VulkanGraphicsPipelineConfig& config);
        ~VulkanGraphicsPipeline();

        VulkanGraphicsPipeline(const VulkanGraphicsPipeline&) = delete;
        VulkanGraphicsPipeline& operator=(const VulkanGraphicsPipeline&) = delete;
        VulkanGraphicsPipeline(VulkanGraphicsPipeline&& other) noexcept;
        VulkanGraphicsPipeline& operator=(VulkanGraphicsPipeline&& other) noexcept;

        [[nodiscard]] VkPipeline getHandle() const { return pipeline_; }
        [[nodiscard]] VkPipelineLayout getLayout() const { return layout_; }

    private:
        void destroy() noexcept;

        VulkanDevice* device_ = nullptr;
        VkPipelineLayout layout_ = VK_NULL_HANDLE;
        VkPipeline pipeline_ = VK_NULL_HANDLE;
    };

} // namespace YATAVK
