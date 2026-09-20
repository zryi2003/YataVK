#pragma once

#include "YataVK/VulkanDevice.h"

#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>
#include <vulkan/vulkan.h>

namespace YATAVK {

    class VulkanShaderModule final {
    public:
        VulkanShaderModule(VulkanDevice& device, std::span<const uint32_t> spirv);
        VulkanShaderModule(VulkanDevice& device, const std::filesystem::path& spirvFile);
        ~VulkanShaderModule();

        VulkanShaderModule(const VulkanShaderModule&) = delete;
        VulkanShaderModule& operator=(const VulkanShaderModule&) = delete;
        VulkanShaderModule(VulkanShaderModule&& other) noexcept;
        VulkanShaderModule& operator=(VulkanShaderModule&& other) noexcept;

        [[nodiscard]] VkShaderModule getHandle() const { return module_; }

    private:
        void create(std::span<const uint32_t> spirv);
        void destroy() noexcept;

        VulkanDevice* device_ = nullptr;
        VkShaderModule module_ = VK_NULL_HANDLE;
    };

} // namespace YATAVK
