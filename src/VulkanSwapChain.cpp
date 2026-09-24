#include "YataVK/VulkanSwapChain.h"

#include "YataVK/VulkanError.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace YATAVK {

    VulkanSwapChain::VulkanSwapChain(VulkanDevice& device, const VulkanSwapChainConfig& config) : device_(&device) {
        create(config, VK_NULL_HANDLE);
    }

    VulkanSwapChain::VulkanSwapChain(VulkanDevice* device, int width, int height)
        : VulkanSwapChain(*device, VulkanSwapChainConfig{.width = static_cast<uint32_t>(std::max(width, 1)),
                                                         .height = static_cast<uint32_t>(std::max(height, 1))}) {
    }

    VulkanSwapChain::~VulkanSwapChain() {
        destroy();
    }

    VulkanSwapChain::VulkanSwapChain(VulkanSwapChain&& other) noexcept
        : device_(std::exchange(other.device_, nullptr)), swapChain_(std::exchange(other.swapChain_, VK_NULL_HANDLE)),
          imageFormat_(other.imageFormat_), colorSpace_(other.colorSpace_), extent_(other.extent_),
          images_(std::move(other.images_)), imageViews_(std::move(other.imageViews_)), generation_(other.generation_) {
    }

    VulkanSwapChain& VulkanSwapChain::operator=(VulkanSwapChain&& other) noexcept {
        if (this != &other) {
            destroy();
            device_ = std::exchange(other.device_, nullptr);
            swapChain_ = std::exchange(other.swapChain_, VK_NULL_HANDLE);
            imageFormat_ = other.imageFormat_;
            colorSpace_ = other.colorSpace_;
            extent_ = other.extent_;
            images_ = std::move(other.images_);
            imageViews_ = std::move(other.imageViews_);
            generation_ = other.generation_;
        }
        return *this;
    }

    void VulkanSwapChain::recreate(const VulkanSwapChainConfig& config) {
        // 窗口最小化时常得到零尺寸，此时保留旧 swapchain 等待后续重建。
        if (config.width == 0 || config.height == 0) {
            return;
        }
        device_->waitIdle();
        const VkSwapchainKHR oldSwapchain = swapChain_;
        destroyImageViews();
        swapChain_ = VK_NULL_HANDLE;
        try {
            // oldSwapchain 让驱动复用资源；创建失败时仍恢复旧句柄供析构。
            create(config, oldSwapchain);
        } catch (...) {
            swapChain_ = oldSwapchain;
            throw;
        }
        vkDestroySwapchainKHR(device_->getLogicalDevice(), oldSwapchain, nullptr);
    }

    VkResult VulkanSwapChain::acquireNextImage(VkSemaphore imageAvailable, uint32_t& imageIndex) const {
        return vkAcquireNextImageKHR(device_->getLogicalDevice(), swapChain_, UINT64_MAX, imageAvailable,
                                     VK_NULL_HANDLE, &imageIndex);
    }

    VkResult VulkanSwapChain::present(uint32_t imageIndex, VkSemaphore renderFinished) const {
        VkPresentInfoKHR presentInfo{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &renderFinished;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &swapChain_;
        presentInfo.pImageIndices = &imageIndex;
        return vkQueuePresentKHR(device_->getPresentQueue(), &presentInfo);
    }

    void VulkanSwapChain::create(const VulkanSwapChainConfig& config, VkSwapchainKHR oldSwapchain) {
        if (config.width == 0 || config.height == 0) {
            throw std::invalid_argument("VulkanSwapChain extent must be non-zero");
        }
        const SwapChainSupportDetails support = device_->querySwapChainSupport();
        const VkSurfaceFormatKHR surfaceFormat = chooseSurfaceFormat(support);
        const VkPresentModeKHR presentMode = choosePresentMode(support, config.preferMailbox);
        const VkExtent2D extent = chooseExtent(support.capabilities, config);
        uint32_t imageCount = std::max(config.preferredImageCount, support.capabilities.minImageCount);
        if (support.capabilities.maxImageCount > 0) {
            imageCount = std::min(imageCount, support.capabilities.maxImageCount);
        }

        const uint32_t families[]{device_->getGraphicsQueueFamily(), device_->getPresentQueueFamily()};
        VkSwapchainCreateInfoKHR createInfo{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
        createInfo.surface = device_->getSurface();
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        if (families[0] != families[1]) {
            // 跨队列族共享可避免每帧显式转移 swapchain image 的所有权。
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = families;
        } else {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }
        createInfo.preTransform = support.capabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE;
        createInfo.oldSwapchain = oldSwapchain;
        checkVk(vkCreateSwapchainKHR(device_->getLogicalDevice(), &createInfo, nullptr, &swapChain_),
                "vkCreateSwapchainKHR");

        imageFormat_ = surfaceFormat.format;
        colorSpace_ = surfaceFormat.colorSpace;
        extent_ = extent;
        checkVk(vkGetSwapchainImagesKHR(device_->getLogicalDevice(), swapChain_, &imageCount, nullptr),
                "vkGetSwapchainImagesKHR");
        images_.resize(imageCount);
        checkVk(vkGetSwapchainImagesKHR(device_->getLogicalDevice(), swapChain_, &imageCount, images_.data()),
                "vkGetSwapchainImagesKHR");
        imageViews_.resize(images_.size(), VK_NULL_HANDLE);
        for (size_t index = 0; index < images_.size(); ++index) {
            VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
            viewInfo.image = images_[index];
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = imageFormat_;
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            viewInfo.subresourceRange.levelCount = 1;
            viewInfo.subresourceRange.layerCount = 1;
            checkVk(vkCreateImageView(device_->getLogicalDevice(), &viewInfo, nullptr, &imageViews_[index]),
                    "vkCreateImageView(swapchain)");
        }
        ++generation_;
    }

    void VulkanSwapChain::destroyImageViews() noexcept {
        if (device_ != nullptr) {
            for (VkImageView view : imageViews_) {
                if (view != VK_NULL_HANDLE) {
                    vkDestroyImageView(device_->getLogicalDevice(), view, nullptr);
                }
            }
        }
        imageViews_.clear();
        images_.clear();
    }

    void VulkanSwapChain::destroy() noexcept {
        destroyImageViews();
        if (device_ != nullptr && swapChain_ != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(device_->getLogicalDevice(), swapChain_, nullptr);
        }
        swapChain_ = VK_NULL_HANDLE;
    }

    VkSurfaceFormatKHR VulkanSwapChain::chooseSurfaceFormat(const SwapChainSupportDetails& support) const {
        const auto preferred = std::find_if(support.formats.begin(), support.formats.end(), [](const auto& format) {
            return format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        });
        return preferred != support.formats.end() ? *preferred : support.formats.front();
    }

    VkPresentModeKHR VulkanSwapChain::choosePresentMode(const SwapChainSupportDetails& support,
                                                        bool preferMailbox) const {
        if (preferMailbox && std::find(support.presentModes.begin(), support.presentModes.end(),
                                       VK_PRESENT_MODE_MAILBOX_KHR) != support.presentModes.end()) {
            return VK_PRESENT_MODE_MAILBOX_KHR;
        }
        // FIFO 是规范保证所有 surface 都支持的回退模式。
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D VulkanSwapChain::chooseExtent(const VkSurfaceCapabilitiesKHR& capabilities,
                                             const VulkanSwapChainConfig& config) const {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            // 部分窗口系统直接规定 surface 尺寸，不允许应用自行选择。
            return capabilities.currentExtent;
        }
        return VkExtent2D{
            std::clamp(config.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
            std::clamp(config.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)};
    }

} // namespace YATAVK
