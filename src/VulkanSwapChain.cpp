//
// Created by Zhuoran Yi on 2026/1/8.
//

#include "YataVK/VulkanSwapChain.h"

namespace YATAVK {
    void VulkanSwapChain::cleanUp() {
        for (auto imageView : vkSwapChainImageViews) {
            vkDestroyImageView(device->getLogicalDevice(), imageView, nullptr);
        }
        if (vkSwapChain != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(device->getLogicalDevice(), vkSwapChain, nullptr);
        }
    }

    void VulkanSwapChain::init(int width, int height) {
        SwapChainSupportDetails swapChainSupport = device->querySwapChainSupport();

        VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
        VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
        VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities, width, height);

        uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
        if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
            imageCount = swapChainSupport.capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = device->getSurface();
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent; // 尝试移除此行会触发错误, 可以用来测试 Validation Layer 是否正确启动
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // 描述交换链中的图像被用于何种操作

        QueueFamilyIndices indices = device->getQueueFamilyIndices();
        uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

        if (indices.graphicsFamily != indices.presentFamily) {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT; // 如果不是同一个队列族, 牺牲部分性能换取无需做所有权转移的跨队列族访问
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        } else {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; // 如果是同一个队列族提供的, 那么要求 image 同时只能被一个队列族所有以获得最佳性能
            createInfo.queueFamilyIndexCount = 0;
            createInfo.pQueueFamilyIndices = nullptr;
        }

        createInfo.preTransform = swapChainSupport.capabilities.currentTransform; // 这个东西可以指定对交换链中的图像做变换(比如旋转, 这里我们不做变换, 使用当前变换)
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE; // 我们不关心被遮挡的像素的颜色
        createInfo.oldSwapchain = VK_NULL_HANDLE; // 交换链的设置是固定的, 因此修改窗口大小等操作会导致交换链失效, 此时需要创建新的交换链, 并在此指定旧的交换链(我猜是为了说明新交换链是为了替换谁)

        if (vkCreateSwapchainKHR(device->getLogicalDevice(), &createInfo, nullptr, &vkSwapChain) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create swap chain!");
        }

        vkGetSwapchainImagesKHR(device->getLogicalDevice(), vkSwapChain, &imageCount, nullptr);
        vkSwapChainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(device->getLogicalDevice(), vkSwapChain, &imageCount, vkSwapChainImages.data());

        vkSwapChainImageFormat = surfaceFormat.format;
        vkSwapChainExtent = extent;

        createImageViews();
    }

    void VulkanSwapChain::createImageViews() {
        vkSwapChainImageViews.resize(vkSwapChainImages.size());

        for (size_t i = 0; i < vkSwapChainImages.size(); i++) {
            VkImageViewCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            createInfo.image = vkSwapChainImages[i];
            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            createInfo.format = vkSwapChainImageFormat;

            // 这组字段允许我们任意的映射颜色通道(如单色纹理的四个通道可以映射到一个通道), 此处我们不需要, 因此都设置为默认
            createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

            // 描述图像的用途以及需要访问的部分(例如, 如果是VR等立体应用, 对于左右眼而言同一张image对应不同的ImageView, 此时需要多层的交换链来分别创建)
            createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            createInfo.subresourceRange.baseMipLevel = 0;
            createInfo.subresourceRange.levelCount = 1;
            createInfo.subresourceRange.baseArrayLayer = 0;
            createInfo.subresourceRange.layerCount = 1;

            if (vkCreateImageView(device->getLogicalDevice(), &createInfo, nullptr, &vkSwapChainImageViews[i]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create image view!");
            }
        }
    }

    VkSurfaceFormatKHR VulkanSwapChain::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats) { // 设置交换链中图像的格式
        for (const auto &availableFormat: availableFormats) {
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
                availableFormat.colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR) {
                return availableFormat;
                }
        }
        return availableFormats[0];
    }

    VkPresentModeKHR VulkanSwapChain::chooseSwapPresentMode(
            const std::vector<VkPresentModeKHR> &availablePresentModes) { // 设置交换链与窗口交互的规则(比如fallback的方案是交换链为一个队列, 写入队尾, 展示队头, 如果满了就阻塞等待)
        for (const auto &availablePresentMode: availablePresentModes) {
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
                return availablePresentMode;
            }
        }

        return VK_PRESENT_MODE_FIFO_KHR; // 只有这个模式是强制要求所有设备都支持的, 因此作为最终的 fallback
    }

    VkExtent2D VulkanSwapChain::chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities, int width, int height) { // 设置交换范围(交换链中图像的分辨率)
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            return capabilities.currentExtent;
        } else {
            VkExtent2D actualExtent = {
                static_cast<uint32_t>(width),
                static_cast<uint32_t>(height)
            };

            actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width,
                                            capabilities.maxImageExtent.width);
            actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height,
                                             capabilities.maxImageExtent.height);

            return actualExtent;
        }
    }
} // YATAVK