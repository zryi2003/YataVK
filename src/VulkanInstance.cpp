#include "YataVK/VulkanInstance.hpp"

#include "YataVK/VulkanError.hpp"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <utility>

namespace YATAVK {

    namespace {

        constexpr const char* kValidationLayer = "VK_LAYER_KHRONOS_validation";

        VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                                     VkDebugUtilsMessageTypeFlagsEXT,
                                                     const VkDebugUtilsMessengerCallbackDataEXT* callbackData, void*) {
            if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
                std::cerr << "[Vulkan] " << callbackData->pMessage << '\n';
            }
            return VK_FALSE;
        }

        bool hasLayer(const char* requested) {
            uint32_t count = 0;
            checkVk(vkEnumerateInstanceLayerProperties(&count, nullptr), "vkEnumerateInstanceLayerProperties");
            std::vector<VkLayerProperties> layers(count);
            checkVk(vkEnumerateInstanceLayerProperties(&count, layers.data()), "vkEnumerateInstanceLayerProperties");
            return std::any_of(layers.begin(), layers.end(), [requested](const VkLayerProperties& layer) {
                return std::strcmp(layer.layerName, requested) == 0;
            });
        }

    } // namespace

    VulkanInstance::VulkanInstance(const VulkanInstanceConfig& config) {
        uint32_t loaderVersion = VK_API_VERSION_1_0;
        if (vkEnumerateInstanceVersion != nullptr) {
            checkVk(vkEnumerateInstanceVersion(&loaderVersion), "vkEnumerateInstanceVersion");
        }
        if (loaderVersion < VK_API_VERSION_1_4) {
            throw std::runtime_error("YataVK requires a Vulkan 1.4 loader");
        }

        std::vector<const char*> extensions = config.requiredExtensions;
        std::vector<const char*> layers;
        if (config.enableValidation) {
            if (!hasLayer(kValidationLayer)) {
                throw std::runtime_error("Vulkan validation requested but VK_LAYER_KHRONOS_validation is unavailable");
            }
            layers.push_back(kValidationLayer);
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        VkApplicationInfo applicationInfo{VK_STRUCTURE_TYPE_APPLICATION_INFO};
        applicationInfo.pApplicationName = config.applicationName.c_str();
        applicationInfo.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
        applicationInfo.pEngineName = "YataVK";
        applicationInfo.engineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
        applicationInfo.apiVersion = apiVersion_;

        VkDebugUtilsMessengerCreateInfoEXT debugInfo{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
        debugInfo.messageSeverity =
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debugInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debugInfo.pfnUserCallback = debugCallback;

        VkInstanceCreateInfo createInfo{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
        createInfo.pApplicationInfo = &applicationInfo;
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();
        createInfo.enabledLayerCount = static_cast<uint32_t>(layers.size());
        createInfo.ppEnabledLayerNames = layers.data();
        createInfo.pNext = config.enableValidation ? &debugInfo : nullptr;
        checkVk(vkCreateInstance(&createInfo, nullptr, &instance_), "vkCreateInstance");

        if (config.enableValidation) {
            const auto createDebugMessenger = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
                vkGetInstanceProcAddr(instance_, "vkCreateDebugUtilsMessengerEXT"));
            if (createDebugMessenger == nullptr) {
                destroy();
                throw std::runtime_error("vkCreateDebugUtilsMessengerEXT is unavailable");
            }
            checkVk(createDebugMessenger(instance_, &debugInfo, nullptr, &debugMessenger_),
                    "vkCreateDebugUtilsMessengerEXT");
        }
    }

    VulkanInstance::~VulkanInstance() {
        destroy();
    }

    VulkanInstance::VulkanInstance(VulkanInstance&& other) noexcept
        : instance_(std::exchange(other.instance_, VK_NULL_HANDLE)),
          debugMessenger_(std::exchange(other.debugMessenger_, VK_NULL_HANDLE)), apiVersion_(other.apiVersion_) {
    }

    VulkanInstance& VulkanInstance::operator=(VulkanInstance&& other) noexcept {
        if (this != &other) {
            destroy();
            instance_ = std::exchange(other.instance_, VK_NULL_HANDLE);
            debugMessenger_ = std::exchange(other.debugMessenger_, VK_NULL_HANDLE);
            apiVersion_ = other.apiVersion_;
        }
        return *this;
    }

    void VulkanInstance::destroy() noexcept {
        if (instance_ == VK_NULL_HANDLE) {
            return;
        }
        if (debugMessenger_ != VK_NULL_HANDLE) {
            const auto destroyDebugMessenger = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
                vkGetInstanceProcAddr(instance_, "vkDestroyDebugUtilsMessengerEXT"));
            if (destroyDebugMessenger != nullptr) {
                destroyDebugMessenger(instance_, debugMessenger_, nullptr);
            }
        }
        vkDestroyInstance(instance_, nullptr);
        instance_ = VK_NULL_HANDLE;
        debugMessenger_ = VK_NULL_HANDLE;
    }

} // namespace YATAVK
