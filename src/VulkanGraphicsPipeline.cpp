#include "YataVK/VulkanGraphicsPipeline.hpp"

#include "YataVK/VulkanError.hpp"

#include <array>
#include <stdexcept>
#include <utility>

namespace YATAVK {

    VulkanGraphicsPipeline::VulkanGraphicsPipeline(VulkanDevice& device, const VulkanGraphicsPipelineConfig& config)
        : device_(&device) {
        if (config.colorFormat == VK_FORMAT_UNDEFINED || config.vertexShader == nullptr ||
            config.fragmentShader == nullptr) {
            throw std::invalid_argument("VulkanGraphicsPipelineConfig is incomplete");
        }
        // Descriptor set layouts 与 push constants 共同定义 shader 可见的资源接口。
        VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        layoutInfo.setLayoutCount = static_cast<uint32_t>(config.descriptorSetLayouts.size());
        layoutInfo.pSetLayouts = config.descriptorSetLayouts.data();
        layoutInfo.pushConstantRangeCount = static_cast<uint32_t>(config.pushConstants.size());
        layoutInfo.pPushConstantRanges = config.pushConstants.data();
        checkVk(vkCreatePipelineLayout(device_->getLogicalDevice(), &layoutInfo, nullptr, &layout_),
                "vkCreatePipelineLayout");

        try {
            const std::array<VkPipelineShaderStageCreateInfo, 2> stages{
                VkPipelineShaderStageCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
                                                VK_SHADER_STAGE_VERTEX_BIT, config.vertexShader->getHandle(), "main",
                                                nullptr},
                VkPipelineShaderStageCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
                                                VK_SHADER_STAGE_FRAGMENT_BIT, config.fragmentShader->getHandle(),
                                                "main", nullptr}};

            VkPipelineVertexInputStateCreateInfo vertexInput{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
            vertexInput.vertexBindingDescriptionCount = static_cast<uint32_t>(config.vertexBindings.size());
            vertexInput.pVertexBindingDescriptions = config.vertexBindings.data();
            vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(config.vertexAttributes.size());
            vertexInput.pVertexAttributeDescriptions = config.vertexAttributes.data();

            VkPipelineInputAssemblyStateCreateInfo assembly{
                VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
            assembly.topology = config.topology;

            VkPipelineViewportStateCreateInfo viewport{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
            viewport.viewportCount = 1;
            viewport.scissorCount = 1;

            VkPipelineRasterizationStateCreateInfo rasterization{
                VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
            rasterization.polygonMode = VK_POLYGON_MODE_FILL;
            rasterization.cullMode = config.cullMode;
            rasterization.frontFace = config.frontFace;
            rasterization.lineWidth = 1.0f;

            VkPipelineMultisampleStateCreateInfo multisample{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
            multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

            VkPipelineDepthStencilStateCreateInfo depth{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
            depth.depthTestEnable = config.depthTest ? VK_TRUE : VK_FALSE;
            depth.depthWriteEnable = config.depthWrite ? VK_TRUE : VK_FALSE;
            depth.depthCompareOp = VK_COMPARE_OP_LESS;

            VkPipelineColorBlendAttachmentState blendAttachment{};
            blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                             VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            if (config.alphaBlend) {
                blendAttachment.blendEnable = VK_TRUE;
                blendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
                blendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
                blendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
                blendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
                blendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
                blendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
            }
            VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
            blend.attachmentCount = 1;
            blend.pAttachments = &blendAttachment;

            // 尺寸随 swapchain 变化，viewport/scissor 留到录制命令时设置。
            const std::array<VkDynamicState, 2> dynamicStates{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
            VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
            dynamic.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
            dynamic.pDynamicStates = dynamicStates.data();

            // Dynamic Rendering 用 attachment formats 代替传统 VkRenderPass 兼容信息。
            VkPipelineRenderingCreateInfo rendering{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
            rendering.colorAttachmentCount = 1;
            rendering.pColorAttachmentFormats = &config.colorFormat;
            rendering.depthAttachmentFormat = config.depthFormat;

            VkGraphicsPipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
            pipelineInfo.pNext = &rendering;
            pipelineInfo.stageCount = static_cast<uint32_t>(stages.size());
            pipelineInfo.pStages = stages.data();
            pipelineInfo.pVertexInputState = &vertexInput;
            pipelineInfo.pInputAssemblyState = &assembly;
            pipelineInfo.pViewportState = &viewport;
            pipelineInfo.pRasterizationState = &rasterization;
            pipelineInfo.pMultisampleState = &multisample;
            // 未提供深度格式时完全省略深度状态，适用于纯颜色 pass。
            pipelineInfo.pDepthStencilState = config.depthFormat == VK_FORMAT_UNDEFINED ? nullptr : &depth;
            pipelineInfo.pColorBlendState = &blend;
            pipelineInfo.pDynamicState = &dynamic;
            pipelineInfo.layout = layout_;
            checkVk(vkCreateGraphicsPipelines(device_->getLogicalDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
                                              &pipeline_),
                    "vkCreateGraphicsPipelines(dynamic rendering)");
        } catch (...) {
            destroy();
            throw;
        }
    }

    VulkanGraphicsPipeline::~VulkanGraphicsPipeline() {
        destroy();
    }

    VulkanGraphicsPipeline::VulkanGraphicsPipeline(VulkanGraphicsPipeline&& other) noexcept
        : device_(std::exchange(other.device_, nullptr)), layout_(std::exchange(other.layout_, VK_NULL_HANDLE)),
          pipeline_(std::exchange(other.pipeline_, VK_NULL_HANDLE)) {
    }

    VulkanGraphicsPipeline& VulkanGraphicsPipeline::operator=(VulkanGraphicsPipeline&& other) noexcept {
        if (this != &other) {
            destroy();
            device_ = std::exchange(other.device_, nullptr);
            layout_ = std::exchange(other.layout_, VK_NULL_HANDLE);
            pipeline_ = std::exchange(other.pipeline_, VK_NULL_HANDLE);
        }
        return *this;
    }

    void VulkanGraphicsPipeline::destroy() noexcept {
        if (device_ != nullptr) {
            if (pipeline_ != VK_NULL_HANDLE) {
                vkDestroyPipeline(device_->getLogicalDevice(), pipeline_, nullptr);
            }
            if (layout_ != VK_NULL_HANDLE) {
                vkDestroyPipelineLayout(device_->getLogicalDevice(), layout_, nullptr);
            }
        }
        pipeline_ = VK_NULL_HANDLE;
        layout_ = VK_NULL_HANDLE;
        device_ = nullptr;
    }

} // namespace YATAVK
