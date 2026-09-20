#pragma once

#include <string>
#include <vector>
#include <vulkan/vulkan.h>

namespace YATAVK {

    struct VulkanInstanceConfig {
        std::string applicationName = "YataVK Application";
        bool enableValidation = false;
        std::vector<const char*> requiredExtensions;
    };

    class VulkanInstance final {
    public:
        explicit VulkanInstance(const VulkanInstanceConfig& config);
        ~VulkanInstance();

        VulkanInstance(const VulkanInstance&) = delete;
        VulkanInstance& operator=(const VulkanInstance&) = delete;
        VulkanInstance(VulkanInstance&& other) noexcept;
        VulkanInstance& operator=(VulkanInstance&& other) noexcept;

        [[nodiscard]] VkInstance getHandle() const { return instance_; }
        [[nodiscard]] uint32_t apiVersion() const { return apiVersion_; }

    private:
        void destroy() noexcept;

        VkInstance instance_ = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT debugMessenger_ = VK_NULL_HANDLE;
        uint32_t apiVersion_ = VK_API_VERSION_1_4;
    };

} // namespace YATAVK
