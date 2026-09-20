#include "YataVK/VulkanDynamicRenderer.hpp"

#include <array>

namespace YATAVK {

    VulkanDynamicRenderer::VulkanDynamicRenderer(VulkanDevice& device, const VulkanSwapChainConfig& swapChainConfig,
                                                 uint32_t framesInFlight, VkFormat depthFormat)
        : device_(&device), swapChain_(device, swapChainConfig), scheduler_(device, framesInFlight),
          depthFormat_(depthFormat) {
        rebuildDepthImages();
    }

    std::optional<VulkanRenderingFrame> VulkanDynamicRenderer::beginFrame(const VkClearColorValue& clearColor) {
        VulkanBeginFrameResult result = scheduler_.beginFrame(swapChain_);
        if (result.status != VulkanFrameStatus::Ready || !result.frame.has_value()) {
            return std::nullopt;
        }
        const VulkanFrameToken token = *result.frame;
        const bool initialized = imageInitialized_[token.imageIndex];

        VkImageMemoryBarrier2 colorBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
        colorBarrier.srcStageMask = initialized ? VK_PIPELINE_STAGE_2_NONE : VK_PIPELINE_STAGE_2_NONE;
        colorBarrier.srcAccessMask = VK_ACCESS_2_NONE;
        colorBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        colorBarrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
        colorBarrier.oldLayout = initialized ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : VK_IMAGE_LAYOUT_UNDEFINED;
        colorBarrier.newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
        colorBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        colorBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        colorBarrier.image = swapChain_.getImages()[token.imageIndex];
        colorBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        colorBarrier.subresourceRange.levelCount = 1;
        colorBarrier.subresourceRange.layerCount = 1;

        VkImageMemoryBarrier2 depthBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
        depthBarrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
        depthBarrier.srcAccessMask = VK_ACCESS_2_NONE;
        depthBarrier.dstStageMask =
            VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
        depthBarrier.dstAccessMask =
            VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        depthBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
        depthBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        depthBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        depthBarrier.image = depthImages_[token.imageIndex].getHandle();
        depthBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        depthBarrier.subresourceRange.levelCount = 1;
        depthBarrier.subresourceRange.layerCount = 1;

        const std::array barriers{colorBarrier, depthBarrier};
        VkDependencyInfo dependency{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
        dependency.imageMemoryBarrierCount = static_cast<uint32_t>(barriers.size());
        dependency.pImageMemoryBarriers = barriers.data();
        vkCmdPipelineBarrier2(token.commandBuffer, &dependency);

        VkRenderingAttachmentInfo colorAttachment{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
        colorAttachment.imageView = swapChain_.getImageViews()[token.imageIndex];
        colorAttachment.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.clearValue.color = clearColor;

        VkRenderingAttachmentInfo depthAttachment{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
        depthAttachment.imageView = depthImages_[token.imageIndex].getImageView();
        depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.clearValue.depthStencil = VkClearDepthStencilValue{1.0f, 0};

        VkRenderingInfo renderingInfo{VK_STRUCTURE_TYPE_RENDERING_INFO};
        renderingInfo.renderArea.extent = swapChain_.getExtent();
        renderingInfo.layerCount = 1;
        renderingInfo.colorAttachmentCount = 1;
        renderingInfo.pColorAttachments = &colorAttachment;
        renderingInfo.pDepthAttachment = &depthAttachment;
        vkCmdBeginRendering(token.commandBuffer, &renderingInfo);

        imageInitialized_[token.imageIndex] = true;
        return VulkanRenderingFrame{token, swapChain_.getExtent(), swapChain_.getImageFormat(), depthFormat_};
    }

    VulkanFrameStatus VulkanDynamicRenderer::endFrame(const VulkanRenderingFrame& frame) {
        vkCmdEndRendering(frame.token.commandBuffer);

        VkImageMemoryBarrier2 barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
        barrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        barrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstStageMask = VK_PIPELINE_STAGE_2_NONE;
        barrier.dstAccessMask = VK_ACCESS_2_NONE;
        barrier.oldLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = swapChain_.getImages()[frame.token.imageIndex];
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.layerCount = 1;
        VkDependencyInfo dependency{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
        dependency.imageMemoryBarrierCount = 1;
        dependency.pImageMemoryBarriers = &barrier;
        vkCmdPipelineBarrier2(frame.token.commandBuffer, &dependency);

        return scheduler_.endFrame(swapChain_, frame.token);
    }

    void VulkanDynamicRenderer::recreate(const VulkanSwapChainConfig& config) {
        scheduler_.waitIdle();
        depthImages_.clear();
        swapChain_.recreate(config);
        rebuildDepthImages();
    }

    void VulkanDynamicRenderer::rebuildDepthImages() {
        depthImages_.clear();
        depthImages_.reserve(swapChain_.imageCount());
        const VkExtent2D extent = swapChain_.getExtent();
        for (uint32_t index = 0; index < swapChain_.imageCount(); ++index) {
            depthImages_.emplace_back(*device_, VulkanImageConfig{.extent = VkExtent3D{extent.width, extent.height, 1},
                                                                  .format = depthFormat_,
                                                                  .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                                                                  .aspect = VK_IMAGE_ASPECT_DEPTH_BIT});
        }
        imageInitialized_.assign(swapChain_.imageCount(), false);
    }

} // namespace YATAVK
