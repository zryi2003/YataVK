//
// Created by Zhuoran Yi on 2026/1/9.
//

#ifndef YATA_VULKANFRAMEBUFFER_H
#define YATA_VULKANFRAMEBUFFER_H

#include <vulkan/vulkan_core.h>

#include "VulkanDevice.h"

namespace YATAVK {
    class VulkanFrameBuffer final {
    public:
        VulkanFrameBuffer(VulkanDevice* device, VkRenderPass render_pass, const std::vector<VkImageView>& attachments, VkExtent2D extent) : device(device), extent(extent) {
            try {
                init(render_pass, attachments);
            } catch (const std::exception& e) {
                cleanUp();
                throw std::runtime_error("Failed to initialize Vulkan FrameBuffer!");
            }
        }
        ~VulkanFrameBuffer() { cleanUp(); }
        VulkanFrameBuffer(const VulkanFrameBuffer&) = delete;
        VulkanFrameBuffer& operator=(const VulkanFrameBuffer&) = delete;

        [[nodiscard]] VkFramebuffer getHandle() const { return vkFramebuffer; }

    private:
        void init(VkRenderPass render_pass, const std::vector<VkImageView>& attachments);
        void cleanUp() const;

        VulkanDevice* device;

        VkFramebuffer vkFramebuffer;
        VkExtent2D extent;
    };
} // YATAVK

#endif //YATA_VULKANFRAMEBUFFER_H