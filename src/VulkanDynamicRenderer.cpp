#include "YataVK/VulkanDynamicRenderer.hpp"

#include <array>
#include <utility>

namespace YATAVK {

    VulkanDynamicRenderer::VulkanDynamicRenderer(VulkanDevice& device, const VulkanSwapChainConfig& swapChainConfig,
                                                 uint32_t framesInFlight, VkFormat depthFormat)
        : device_(&device), swapChain_(device, swapChainConfig), scheduler_(device, framesInFlight),
          depthFormat_(depthFormat) {
        rebuildDepthImages();
    }

    VulkanDynamicRenderer::~VulkanDynamicRenderer() {
        // 成员按声明逆序析构：depthImages_ 会先于 scheduler_（其析构才 waitIdle）被销毁。
        // 这里先等设备空闲，保证在途帧不再引用深度图。析构不能抛异常，故直接调用并忽略返回值。
        if (device_ != nullptr && device_->getLogicalDevice() != VK_NULL_HANDLE) {
            vkDeviceWaitIdle(device_->getLogicalDevice());
        }
    }

    std::optional<VulkanRenderingFrame> VulkanDynamicRenderer::beginFrame(const VkClearColorValue& clearColor) {
        // 上次 recreate 中途失败时深度图与 swapchain 不匹配；必须在 acquire 之前拒绝，
        // 否则已 acquire 的图像和 imageAvailable semaphore 会悬空。
        if (depthImages_.size() != swapChain_.imageCount()) {
            return std::nullopt;
        }
        VulkanBeginFrameResult result = scheduler_.beginFrame(swapChain_);
        if (result.status != VulkanFrameStatus::Ready || !result.frame.has_value()) {
            return std::nullopt;
        }
        const VulkanFrameToken token = *result.frame;
        const bool initialized = imageInitialized_[token.imageIndex];

        // 第一次取得的图像可丢弃旧内容；之后则从展示布局切回附件布局。
        VkImageMemoryBarrier2 colorBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
        // srcStage 必须覆盖提交时等待 imageAvailable 的阶段（COLOR_ATTACHMENT_OUTPUT），
        // 布局转换才会排在 acquire 之后；srcAccess 为 NONE（无需可用性操作）。
        colorBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
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

        // 深度每帧都会 clear，使用 UNDEFINED 表示无需保留上一帧内容。
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
        // beginFrame 返回时已经处于 dynamic rendering 区间内。
        vkCmdBeginRendering(token.commandBuffer, &renderingInfo);

        imageInitialized_[token.imageIndex] = true;
        return VulkanRenderingFrame{token, swapChain_.getExtent(), swapChain_.getImageFormat(), depthFormat_};
    }

    VulkanFrameStatus VulkanDynamicRenderer::endFrame(const VulkanRenderingFrame& frame) {
        vkCmdEndRendering(frame.token.commandBuffer);

        // 展示引擎只接受 PRESENT_SRC_KHR，提交前显式完成布局交接。
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
        // 深度图尺寸依赖 swapchain extent，必须随交换链一起重建。
        depthImages_.clear();
        swapChain_.recreate(config);
        rebuildDepthImages();
    }

    void VulkanDynamicRenderer::rebuildDepthImages() {
        std::vector<VulkanImage> images;
        images.reserve(swapChain_.imageCount());
        const VkExtent2D extent = swapChain_.getExtent();
        for (uint32_t index = 0; index < swapChain_.imageCount(); ++index) {
            images.emplace_back(*device_, VulkanImageConfig{.extent = VkExtent3D{extent.width, extent.height, 1},
                                                                  .format = depthFormat_,
                                                                  .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                                                                  .aspect = VK_IMAGE_ASPECT_DEPTH_BIT});
        }
        // 全部创建成功后一次性提交；中途失败则 depthImages_ 保持为空，beginFrame 会拒绝开始新帧。
        depthImages_ = std::move(images);
        imageInitialized_.assign(swapChain_.imageCount(), false);
    }

} // namespace YATAVK
