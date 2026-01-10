//
// Created by Zhuoran Yi on 2026/1/8.
//

#ifndef YATA_VULKANSWAPCHAIN_H
#define YATA_VULKANSWAPCHAIN_H

#include <stdexcept>
#include <vector>
#include <optional>

#include <vulkan/vulkan_core.h>

#include "VulkanDevice.h"

namespace YATAVK {

    class VulkanSwapChain final {
    public:
        VulkanSwapChain(VulkanDevice* device, int width, int height) : device(device) {
            try {
                init(width, height);
            } catch (const std::exception& e) {
                cleanUp();
                throw std::runtime_error("Failed to initialize Vulkan Swap Chain!");
            }
        }
        ~VulkanSwapChain() { cleanUp(); }

        VulkanSwapChain(const VulkanSwapChain&) = delete;
        VulkanSwapChain& operator=(const VulkanSwapChain&) = delete;

        [[nodiscard]] VkExtent2D getExtent() const { return vkSwapChainExtent; }
        [[nodiscard]] VkFormat getImageFormat() const { return vkSwapChainImageFormat; }
        [[nodiscard]] const std::vector<VkImageView>& getImageViews() const { return vkSwapChainImageViews; }
        [[nodiscard]] VkSwapchainKHR getHandle() const { return vkSwapChain; }

    private:
        void init(int width, int height);
        void cleanUp();

        void createImageViews();

        VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes);
        VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities, int width, int height);
        VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats);

        VulkanDevice* device = nullptr;

        VkSwapchainKHR vkSwapChain = VK_NULL_HANDLE;

        VkFormat vkSwapChainImageFormat;
        VkExtent2D vkSwapChainExtent{};

        std::vector<VkImage> vkSwapChainImages; // 交换链中图片的句柄(我们需要把渲染结果写到交换链上, 这就是Render Target), 它由交换链进行创建与回收
        std::vector<VkImageView> vkSwapChainImageViews; // 描述如何访问Image
    };

} // YATAVK

#endif //YATA_VULKANSWAPCHAIN_H