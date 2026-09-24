//
// Created by Zhuoran Yi on 2026/1/9.
//

#ifdef YATAVK_ENABLE_VMA
#define VMA_IMPLEMENTATION
#include <vma/vk_mem_alloc.h>
#endif

#include "src/VulkanBuffer.cpp"
#include "src/VulkanDescriptor.cpp"
#include "src/VulkanDevice.cpp"
#include "src/VulkanDynamicRenderer.cpp"
#include "src/VulkanFrameScheduler.cpp"
#include "src/VulkanGraphicsPipeline.cpp"
#include "src/VulkanImage.cpp"
#include "src/VulkanInstance.cpp"
#include "src/VulkanShaderModule.cpp"
#include "src/VulkanSwapChain.cpp"

#ifdef YATAVK_BUILD_LEGACY
#include "src/VulkanFrameBuffer.cpp"
#include "src/VulkanSampler.cpp"
#endif
