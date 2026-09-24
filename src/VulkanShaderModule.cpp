#include "YataVK/VulkanShaderModule.hpp"

#include "YataVK/VulkanError.hpp"

#include <fstream>
#include <stdexcept>
#include <utility>

namespace YATAVK {

    VulkanShaderModule::VulkanShaderModule(VulkanDevice& device, std::span<const uint32_t> spirv) : device_(&device) {
        create(spirv);
    }

    VulkanShaderModule::VulkanShaderModule(VulkanDevice& device, const std::filesystem::path& spirvFile)
        : device_(&device) {
        std::ifstream stream(spirvFile, std::ios::binary | std::ios::ate);
        if (!stream) {
            throw std::runtime_error("Unable to open SPIR-V file: " + spirvFile.string());
        }
        const std::streamsize byteCount = stream.tellg();
        if (byteCount <= 0 || byteCount % static_cast<std::streamsize>(sizeof(uint32_t)) != 0) {
            throw std::runtime_error("Invalid SPIR-V byte count: " + spirvFile.string());
        }
        stream.seekg(0);
        // 以 words 存储可同时满足 VkShaderModule 对大小和对齐的要求。
        std::vector<uint32_t> words(static_cast<size_t>(byteCount) / sizeof(uint32_t));
        stream.read(reinterpret_cast<char*>(words.data()), byteCount);
        if (!stream) {
            throw std::runtime_error("Unable to read SPIR-V file: " + spirvFile.string());
        }
        create(words);
    }

    VulkanShaderModule::~VulkanShaderModule() {
        destroy();
    }

    VulkanShaderModule::VulkanShaderModule(VulkanShaderModule&& other) noexcept
        : device_(std::exchange(other.device_, nullptr)), module_(std::exchange(other.module_, VK_NULL_HANDLE)) {
    }

    VulkanShaderModule& VulkanShaderModule::operator=(VulkanShaderModule&& other) noexcept {
        if (this != &other) {
            destroy();
            device_ = std::exchange(other.device_, nullptr);
            module_ = std::exchange(other.module_, VK_NULL_HANDLE);
        }
        return *this;
    }

    void VulkanShaderModule::create(std::span<const uint32_t> spirv) {
        if (spirv.empty()) {
            throw std::invalid_argument("VulkanShaderModule requires non-empty SPIR-V");
        }
        VkShaderModuleCreateInfo createInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        createInfo.codeSize = spirv.size_bytes();
        createInfo.pCode = spirv.data();
        checkVk(vkCreateShaderModule(device_->getLogicalDevice(), &createInfo, nullptr, &module_),
                "vkCreateShaderModule");
    }

    void VulkanShaderModule::destroy() noexcept {
        if (device_ != nullptr && module_ != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device_->getLogicalDevice(), module_, nullptr);
        }
        module_ = VK_NULL_HANDLE;
        device_ = nullptr;
    }

} // namespace YATAVK
