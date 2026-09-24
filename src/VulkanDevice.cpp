#include "YataVK/VulkanDevice.h"

#include "YataVK/VulkanError.hpp"

#include <algorithm>
#include <cstring>
#include <set>
#include <stdexcept>
#include <utility>

namespace YATAVK {

    namespace {

        std::vector<const char*> normalizedExtensions(const VulkanDeviceRequirements& requirements) {
            std::vector<const char*> result = requirements.requiredExtensions;
            // 有展示 surface 时，swapchain 扩展是隐含的最低需求。
            if (requirements.presentationSurface != VK_NULL_HANDLE &&
                std::none_of(result.begin(), result.end(), [](const char* extension) {
                    return std::strcmp(extension, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0;
                })) {
                result.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
            }
            return result;
        }

    } // namespace

    VulkanDevice::VulkanDevice(VkInstance instance, const VulkanDeviceRequirements& requirements)
        : instance_(instance), surface_(requirements.presentationSurface) {
        const std::vector<const char*> extensions = normalizedExtensions(requirements);
        selectPhysicalDevice(requirements, extensions);
        createLogicalDevice(requirements, extensions);
    }

    VulkanDevice::VulkanDevice(VkInstance instance, VkSurfaceKHR surface)
        : VulkanDevice(instance, VulkanDeviceRequirements{.presentationSurface = surface}) {
    }

    VulkanDevice::~VulkanDevice() {
        destroy();
    }

    VulkanDevice::VulkanDevice(VulkanDevice&& other) noexcept
        : instance_(std::exchange(other.instance_, VK_NULL_HANDLE)),
          physicalDevice_(std::exchange(other.physicalDevice_, VK_NULL_HANDLE)),
          logicalDevice_(std::exchange(other.logicalDevice_, VK_NULL_HANDLE)),
          surface_(std::exchange(other.surface_, VK_NULL_HANDLE)), queueFamilies_(other.queueFamilies_),
          graphicsQueue_(std::exchange(other.graphicsQueue_, VK_NULL_HANDLE)),
          presentQueue_(std::exchange(other.presentQueue_, VK_NULL_HANDLE)),
          dynamicRenderingEnabled_(other.dynamicRenderingEnabled_) {
    }

    VulkanDevice& VulkanDevice::operator=(VulkanDevice&& other) noexcept {
        if (this != &other) {
            destroy();
            instance_ = std::exchange(other.instance_, VK_NULL_HANDLE);
            physicalDevice_ = std::exchange(other.physicalDevice_, VK_NULL_HANDLE);
            logicalDevice_ = std::exchange(other.logicalDevice_, VK_NULL_HANDLE);
            surface_ = std::exchange(other.surface_, VK_NULL_HANDLE);
            queueFamilies_ = other.queueFamilies_;
            graphicsQueue_ = std::exchange(other.graphicsQueue_, VK_NULL_HANDLE);
            presentQueue_ = std::exchange(other.presentQueue_, VK_NULL_HANDLE);
            dynamicRenderingEnabled_ = other.dynamicRenderingEnabled_;
        }
        return *this;
    }

    void VulkanDevice::destroy() noexcept {
        if (logicalDevice_ != VK_NULL_HANDLE) {
            // Device 是其余封装资源的父对象，销毁前确保 GPU 不再引用它们。
            vkDeviceWaitIdle(logicalDevice_);
            vkDestroyDevice(logicalDevice_, nullptr);
        }
        logicalDevice_ = VK_NULL_HANDLE;
        physicalDevice_ = VK_NULL_HANDLE;
        graphicsQueue_ = VK_NULL_HANDLE;
        presentQueue_ = VK_NULL_HANDLE;
    }

    QueueFamilyIndices VulkanDevice::findQueueFamilies(VkPhysicalDevice physicalDevice) const {
        uint32_t count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &count, nullptr);
        std::vector<VkQueueFamilyProperties> properties(count);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &count, properties.data());

        QueueFamilyIndices result;
        for (uint32_t index = 0; index < count; ++index) {
            if ((properties[index].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) {
                result.graphicsFamily = index;
            }
            if (surface_ == VK_NULL_HANDLE) {
                // 无展示目标时复用 graphics family，仍保持索引接口完整。
                result.presentFamily = result.graphicsFamily;
            } else {
                VkBool32 supported = VK_FALSE;
                checkVk(vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, index, surface_, &supported),
                        "vkGetPhysicalDeviceSurfaceSupportKHR");
                if (supported == VK_TRUE) {
                    result.presentFamily = index;
                }
            }
            if (result.isComplete()) {
                break;
            }
        }
        return result;
    }

    bool VulkanDevice::supportsExtensions(VkPhysicalDevice physicalDevice,
                                          const std::vector<const char*>& extensions) const {
        uint32_t count = 0;
        checkVk(vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &count, nullptr),
                "vkEnumerateDeviceExtensionProperties");
        std::vector<VkExtensionProperties> available(count);
        checkVk(vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &count, available.data()),
                "vkEnumerateDeviceExtensionProperties");
        return std::all_of(extensions.begin(), extensions.end(), [&](const char* required) {
            return std::any_of(available.begin(), available.end(), [&](const VkExtensionProperties& candidate) {
                return std::strcmp(required, candidate.extensionName) == 0;
            });
        });
    }

    bool VulkanDevice::supportsRequiredFeatures(VkPhysicalDevice physicalDevice,
                                                const VkPhysicalDeviceFeatures& required) const {
        VkPhysicalDeviceFeatures available{};
        vkGetPhysicalDeviceFeatures(physicalDevice, &available);
        // VkPhysicalDeviceFeatures 由连续的 VkBool32 字段组成，可逐项比较请求位。
        const auto* requiredValues = reinterpret_cast<const VkBool32*>(&required);
        const auto* availableValues = reinterpret_cast<const VkBool32*>(&available);
        constexpr size_t featureCount = sizeof(VkPhysicalDeviceFeatures) / sizeof(VkBool32);
        for (size_t index = 0; index < featureCount; ++index) {
            if (requiredValues[index] == VK_TRUE && availableValues[index] != VK_TRUE) {
                return false;
            }
        }
        return true;
    }

    void VulkanDevice::selectPhysicalDevice(const VulkanDeviceRequirements& requirements,
                                            const std::vector<const char*>& extensions) {
        uint32_t count = 0;
        checkVk(vkEnumeratePhysicalDevices(instance_, &count, nullptr), "vkEnumeratePhysicalDevices");
        if (count == 0) {
            throw std::runtime_error("No Vulkan physical devices are available");
        }
        std::vector<VkPhysicalDevice> devices(count);
        checkVk(vkEnumeratePhysicalDevices(instance_, &count, devices.data()), "vkEnumeratePhysicalDevices");

        // 当前策略选择第一个满足全部硬性条件的设备，不额外进行性能评分。
        for (VkPhysicalDevice candidate : devices) {
            VkPhysicalDeviceProperties properties{};
            vkGetPhysicalDeviceProperties(candidate, &properties);
            if (properties.apiVersion < VK_API_VERSION_1_4) {
                continue;
            }
            const QueueFamilyIndices families = findQueueFamilies(candidate);
            if (!families.isComplete() || !supportsExtensions(candidate, extensions) ||
                !supportsRequiredFeatures(candidate, requirements.requiredFeatures)) {
                continue;
            }
            VkPhysicalDeviceVulkan13Features features13{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
            VkPhysicalDeviceFeatures2 features2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
            features2.pNext = &features13;
            vkGetPhysicalDeviceFeatures2(candidate, &features2);
            if ((requirements.requireDynamicRendering && features13.dynamicRendering != VK_TRUE) ||
                features13.synchronization2 != VK_TRUE) {
                continue;
            }
            if (surface_ != VK_NULL_HANDLE) {
                // querySwapChainSupport 使用成员句柄，这里只在探测期间临时绑定候选设备。
                physicalDevice_ = candidate;
                const SwapChainSupportDetails support = querySwapChainSupport();
                physicalDevice_ = VK_NULL_HANDLE;
                if (support.formats.empty() || support.presentModes.empty()) {
                    continue;
                }
            }
            physicalDevice_ = candidate;
            queueFamilies_ = families;
            return;
        }
        throw std::runtime_error("No Vulkan 1.4 device satisfies the requested YataVK capabilities");
    }

    void VulkanDevice::createLogicalDevice(const VulkanDeviceRequirements& requirements,
                                           const std::vector<const char*>& extensions) {
        // graphics/present 相同时只创建一条 queue create info。
        const std::set<uint32_t> uniqueFamilies{queueFamilies_.graphicsFamily.value(),
                                                queueFamilies_.presentFamily.value()};
        constexpr float priority = 1.0f;
        std::vector<VkDeviceQueueCreateInfo> queueInfos;
        queueInfos.reserve(uniqueFamilies.size());
        for (uint32_t family : uniqueFamilies) {
            VkDeviceQueueCreateInfo queueInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
            queueInfo.queueFamilyIndex = family;
            queueInfo.queueCount = 1;
            queueInfo.pQueuePriorities = &priority;
            queueInfos.push_back(queueInfo);
        }

        VkPhysicalDeviceVulkan13Features features13{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
        features13.dynamicRendering = requirements.requireDynamicRendering ? VK_TRUE : VK_FALSE;
        features13.synchronization2 = VK_TRUE;

        VkDeviceCreateInfo createInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
        createInfo.pNext = &features13;
        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueInfos.size());
        createInfo.pQueueCreateInfos = queueInfos.data();
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();
        createInfo.pEnabledFeatures = &requirements.requiredFeatures;
        checkVk(vkCreateDevice(physicalDevice_, &createInfo, nullptr, &logicalDevice_), "vkCreateDevice");

        vkGetDeviceQueue(logicalDevice_, queueFamilies_.graphicsFamily.value(), 0, &graphicsQueue_);
        vkGetDeviceQueue(logicalDevice_, queueFamilies_.presentFamily.value(), 0, &presentQueue_);
        dynamicRenderingEnabled_ = requirements.requireDynamicRendering;
    }

    uint32_t VulkanDevice::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const {
        VkPhysicalDeviceMemoryProperties memoryProperties{};
        vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &memoryProperties);
        for (uint32_t index = 0; index < memoryProperties.memoryTypeCount; ++index) {
            if ((typeFilter & (1U << index)) != 0 &&
                (memoryProperties.memoryTypes[index].propertyFlags & properties) == properties) {
                return index;
            }
        }
        throw std::runtime_error("No compatible Vulkan memory type was found");
    }

    SwapChainSupportDetails VulkanDevice::querySwapChainSupport() const {
        SwapChainSupportDetails details;
        checkVk(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice_, surface_, &details.capabilities),
                "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
        uint32_t count = 0;
        checkVk(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice_, surface_, &count, nullptr),
                "vkGetPhysicalDeviceSurfaceFormatsKHR");
        details.formats.resize(count);
        if (count > 0) {
            checkVk(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice_, surface_, &count, details.formats.data()),
                    "vkGetPhysicalDeviceSurfaceFormatsKHR");
        }
        count = 0;
        checkVk(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice_, surface_, &count, nullptr),
                "vkGetPhysicalDeviceSurfacePresentModesKHR");
        details.presentModes.resize(count);
        if (count > 0) {
            checkVk(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice_, surface_, &count,
                                                              details.presentModes.data()),
                    "vkGetPhysicalDeviceSurfacePresentModesKHR");
        }
        return details;
    }

    void VulkanDevice::waitIdle() const {
        checkVk(vkDeviceWaitIdle(logicalDevice_), "vkDeviceWaitIdle");
    }

} // namespace YATAVK
