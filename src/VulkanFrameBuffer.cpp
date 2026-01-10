//
// Created by Zhuoran Yi on 2026/1/9.
//

#include "YataVK/VulkanFrameBuffer.h"

namespace YATAVK {
    void VulkanFrameBuffer::init(VkRenderPass render_pass, const std::vector<VkImageView>& attachments) {
        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = render_pass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = extent.width;
        framebufferInfo.height = extent.height;
        framebufferInfo.layers = 1;
        if (vkCreateFramebuffer(device->getLogicalDevice(), &framebufferInfo, nullptr, &vkFramebuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create framebuffer!");
        }
    }

    void VulkanFrameBuffer::cleanUp() const {
        vkDestroyFramebuffer(device->getLogicalDevice(), vkFramebuffer, nullptr);
    }
} // YATAVK