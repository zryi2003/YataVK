#pragma once

#include "YataVK/VulkanFrameScheduler.hpp"
#include "YataVK/VulkanImage.hpp"

#include <optional>
#include <vector>

namespace YATAVK {

    struct VulkanRenderingFrame {
        VulkanFrameToken token;
        VkExtent2D extent{};
        VkFormat colorFormat = VK_FORMAT_UNDEFINED;
        VkFormat depthFormat = VK_FORMAT_UNDEFINED;
    };

    class VulkanDynamicRenderer final {
    public:
        VulkanDynamicRenderer(VulkanDevice& device, const VulkanSwapChainConfig& swapChainConfig,
                              uint32_t framesInFlight = 2, VkFormat depthFormat = VK_FORMAT_D32_SFLOAT);
        ~VulkanDynamicRenderer() = default;

        VulkanDynamicRenderer(const VulkanDynamicRenderer&) = delete;
        VulkanDynamicRenderer& operator=(const VulkanDynamicRenderer&) = delete;

        // 成功后由调用者在返回的 command buffer 中录制绘制命令。
        [[nodiscard]] std::optional<VulkanRenderingFrame> beginFrame(const VkClearColorValue& clearColor);
        [[nodiscard]] VulkanFrameStatus endFrame(const VulkanRenderingFrame& frame);
        void recreate(const VulkanSwapChainConfig& config);
        void waitIdle() const { scheduler_.waitIdle(); }

        [[nodiscard]] VulkanSwapChain& swapChain() { return swapChain_; }
        [[nodiscard]] const VulkanSwapChain& swapChain() const { return swapChain_; }
        [[nodiscard]] VkFormat depthFormat() const { return depthFormat_; }

    private:
        void rebuildDepthImages();

        VulkanDevice* device_;
        VulkanSwapChain swapChain_;
        VulkanFrameScheduler scheduler_;
        VkFormat depthFormat_;
        std::vector<VulkanImage> depthImages_;
        std::vector<bool> imageInitialized_;
    };

} // namespace YATAVK
