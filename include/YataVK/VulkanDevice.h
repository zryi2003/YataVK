#pragma once

#include <cstdint>
#include <optional>
#include <vector>
#include <vulkan/vulkan.h>

namespace YATAVK {

    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;

        [[nodiscard]] bool isComplete() const { return graphicsFamily.has_value() && presentFamily.has_value(); }
    };

    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities{};
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    struct VulkanDeviceRequirements {
        // 非拥有句柄；必须比 device 及其创建的所有 swapchain 活得更久。
        VkSurfaceKHR presentationSurface = VK_NULL_HANDLE;
        bool requireDynamicRendering = true;
        std::vector<const char*> requiredExtensions;
        VkPhysicalDeviceFeatures requiredFeatures{};
    };

    class VulkanDevice final {
    public:
        VulkanDevice(VkInstance instance, const VulkanDeviceRequirements& requirements);
        VulkanDevice(VkInstance instance, VkSurfaceKHR surface);
        ~VulkanDevice();

        VulkanDevice(const VulkanDevice&) = delete;
        VulkanDevice& operator=(const VulkanDevice&) = delete;
        VulkanDevice(VulkanDevice&& other) noexcept;
        VulkanDevice& operator=(VulkanDevice&& other) noexcept;

        [[nodiscard]] VkDevice getLogicalDevice() const { return logicalDevice_; }
        [[nodiscard]] VkPhysicalDevice getPhysicalDevice() const { return physicalDevice_; }
        [[nodiscard]] VkSurfaceKHR getSurface() const { return surface_; }
        [[nodiscard]] VkQueue getGraphicsQueue() const { return graphicsQueue_; }
        [[nodiscard]] VkQueue getPresentQueue() const { return presentQueue_; }
        [[nodiscard]] uint32_t getGraphicsQueueFamily() const { return queueFamilies_.graphicsFamily.value(); }
        [[nodiscard]] uint32_t getPresentQueueFamily() const { return queueFamilies_.presentFamily.value(); }
        [[nodiscard]] QueueFamilyIndices getQueueFamilyIndices() const { return queueFamilies_; }
        [[nodiscard]] bool dynamicRenderingEnabled() const { return dynamicRenderingEnabled_; }

        [[nodiscard]] uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;
        [[nodiscard]] SwapChainSupportDetails querySwapChainSupport() const;
        void waitIdle() const;

    private:
        void destroy() noexcept;
        [[nodiscard]] QueueFamilyIndices findQueueFamilies(VkPhysicalDevice physicalDevice) const;
        [[nodiscard]] bool supportsExtensions(VkPhysicalDevice physicalDevice,
                                              const std::vector<const char*>& extensions) const;
        [[nodiscard]] bool supportsRequiredFeatures(VkPhysicalDevice physicalDevice,
                                                    const VkPhysicalDeviceFeatures& required) const;
        void selectPhysicalDevice(const VulkanDeviceRequirements& requirements,
                                  const std::vector<const char*>& extensions);
        void createLogicalDevice(const VulkanDeviceRequirements& requirements,
                                 const std::vector<const char*>& extensions);

        VkInstance instance_ = VK_NULL_HANDLE;
        VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
        VkDevice logicalDevice_ = VK_NULL_HANDLE;
        VkSurfaceKHR surface_ = VK_NULL_HANDLE; // 由应用持有。
        QueueFamilyIndices queueFamilies_{};
        VkQueue graphicsQueue_ = VK_NULL_HANDLE;
        VkQueue presentQueue_ = VK_NULL_HANDLE;
        bool dynamicRenderingEnabled_ = false;
    };

} // namespace YATAVK
