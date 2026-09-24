#pragma once

#include "YataVK/VulkanDevice.h"

#include <cstdint>
#include <vector>
#include <vulkan/vulkan.h>

namespace YATAVK {

    struct VulkanSwapChainConfig {
        uint32_t width = 1;
        uint32_t height = 1;
        bool preferMailbox = true;
        uint32_t preferredImageCount = 3;
    };

    class VulkanSwapChain final {
    public:
        VulkanSwapChain(VulkanDevice& device, const VulkanSwapChainConfig& config);
        VulkanSwapChain(VulkanDevice* device, int width, int height);
        ~VulkanSwapChain();

        VulkanSwapChain(const VulkanSwapChain&) = delete;
        VulkanSwapChain& operator=(const VulkanSwapChain&) = delete;
        VulkanSwapChain(VulkanSwapChain&& other) noexcept;
        VulkanSwapChain& operator=(VulkanSwapChain&& other) noexcept;

        void recreate(const VulkanSwapChainConfig& config);
        [[nodiscard]] VkResult acquireNextImage(VkSemaphore imageAvailable, uint32_t& imageIndex) const;
        [[nodiscard]] VkResult present(uint32_t imageIndex, VkSemaphore renderFinished) const;

        [[nodiscard]] VkSwapchainKHR getHandle() const { return swapChain_; }
        [[nodiscard]] VkExtent2D getExtent() const { return extent_; }
        [[nodiscard]] VkFormat getImageFormat() const { return imageFormat_; }
        [[nodiscard]] VkColorSpaceKHR getColorSpace() const { return colorSpace_; }
        [[nodiscard]] const std::vector<VkImage>& getImages() const { return images_; }
        [[nodiscard]] const std::vector<VkImageView>& getImageViews() const { return imageViews_; }
        [[nodiscard]] uint32_t imageCount() const { return static_cast<uint32_t>(images_.size()); }
        // 每次成功创建或重建后递增，可用于识别依赖资源是否过期。
        [[nodiscard]] uint64_t generation() const { return generation_; }

    private:
        void create(const VulkanSwapChainConfig& config, VkSwapchainKHR oldSwapchain);
        void destroyImageViews() noexcept;
        void destroy() noexcept;
        [[nodiscard]] VkSurfaceFormatKHR chooseSurfaceFormat(const SwapChainSupportDetails& support) const;
        [[nodiscard]] VkPresentModeKHR choosePresentMode(const SwapChainSupportDetails& support,
                                                         bool preferMailbox) const;
        [[nodiscard]] VkExtent2D chooseExtent(const VkSurfaceCapabilitiesKHR& capabilities,
                                              const VulkanSwapChainConfig& config) const;

        VulkanDevice* device_ = nullptr;
        VkSwapchainKHR swapChain_ = VK_NULL_HANDLE;
        VkFormat imageFormat_ = VK_FORMAT_UNDEFINED;
        VkColorSpaceKHR colorSpace_ = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        VkExtent2D extent_{};
        std::vector<VkImage> images_;
        std::vector<VkImageView> imageViews_;
        uint64_t generation_ = 0;
    };

} // namespace YATAVK
