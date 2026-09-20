#pragma once

#include <stdexcept>
#include <string>
#include <vulkan/vulkan.h>

namespace YATAVK {

    inline void checkVk(VkResult result, const char* operation) {
        if (result != VK_SUCCESS) {
            throw std::runtime_error(std::string(operation) + " failed with VkResult " +
                                     std::to_string(static_cast<int32_t>(result)));
        }
    }

} // namespace YATAVK
