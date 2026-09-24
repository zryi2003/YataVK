#pragma once

#include "YataVK/VulkanDevice.h"
#include "YataVK/VulkanSwapChain.h"

#include <cstdint>
#include <optional>
#include <vector>
#include <vulkan/vulkan.h>

namespace YATAVK {

    struct VulkanFrameToken {
        // 仅在对应的 beginFrame/endFrame 区间内有效。
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t imageIndex = 0;
        uint32_t frameIndex = 0;
    };

    enum class VulkanFrameStatus {
        Ready,
        SwapChainOutOfDate,
    };

    struct VulkanBeginFrameResult {
        VulkanFrameStatus status = VulkanFrameStatus::SwapChainOutOfDate;
        std::optional<VulkanFrameToken> frame;
    };

    class VulkanFrameScheduler final {
    public:
        explicit VulkanFrameScheduler(VulkanDevice& device, uint32_t framesInFlight = 2);
        ~VulkanFrameScheduler();

        VulkanFrameScheduler(const VulkanFrameScheduler&) = delete;
        VulkanFrameScheduler& operator=(const VulkanFrameScheduler&) = delete;
        VulkanFrameScheduler(VulkanFrameScheduler&& other) noexcept;
        VulkanFrameScheduler& operator=(VulkanFrameScheduler&& other) noexcept;

        [[nodiscard]] VulkanBeginFrameResult beginFrame(const VulkanSwapChain& swapChain);
        [[nodiscard]] VulkanFrameStatus endFrame(const VulkanSwapChain& swapChain, const VulkanFrameToken& token);
        void waitIdle() const;
        [[nodiscard]] uint32_t framesInFlight() const { return static_cast<uint32_t>(frames_.size()); }

    private:
        struct FrameResources {
            // 每个并行帧独占命令池和同步原语，避免跨帧重置仍在执行的资源。
            VkCommandPool commandPool = VK_NULL_HANDLE;
            VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
            VkSemaphore imageAvailable = VK_NULL_HANDLE;
            VkSemaphore renderFinished = VK_NULL_HANDLE;
            VkFence inFlight = VK_NULL_HANDLE;
        };

        void destroy() noexcept;

        VulkanDevice* device_ = nullptr;
        std::vector<FrameResources> frames_;
        uint32_t currentFrame_ = 0;
    };

} // namespace YATAVK
