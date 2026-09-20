# YataVK

YataVK is a small C++20 Vulkan 1.4 support library for research applications. Its supported path is based on core dynamic rendering and explicit application ownership of platform surfaces.

## Supported path

The default `YataVK` CMake target builds only:

- `VulkanInstance`;
- capability-driven `VulkanDevice`;
- `VulkanSwapChain` and `VulkanFrameScheduler`;
- `VulkanDynamicRenderer` with per-swapchain depth images;
- move-only buffer, image, shader module, descriptor, and graphics-pipeline wrappers.

The library requires Vulkan 1.4. `VulkanDynamicRenderer` uses `vkCmdBeginRendering()` and synchronization2; it does not create a `VkRenderPass` or `VkFramebuffer`.

## Surface ownership

YataVK does not create, destroy, or own a `VkSurfaceKHR`. The application creates the platform surface and keeps it alive until all YataVK devices and swapchains using it are destroyed. Pass the non-owning handle through `VulkanDeviceRequirements::presentationSurface`:

```cpp
YATAVK::VulkanInstance instance(instanceConfig);

// Application-owned platform code.
VkSurfaceKHR surface = createApplicationSurface(instance.getHandle(), nativeWindow);

YATAVK::VulkanDeviceRequirements requirements;
requirements.presentationSurface = surface;
requirements.requireDynamicRendering = true;
YATAVK::VulkanDevice device(instance.getHandle(), requirements);

YATAVK::VulkanDynamicRenderer renderer(
    device,
    YATAVK::VulkanSwapChainConfig{width, height, true, 3});
```

## Graphics pipelines and descriptors

`VulkanGraphicsPipelineConfig::descriptorSetLayouts` is copied into `VkPipelineLayoutCreateInfo`; push-constant ranges remain independent. `VulkanDescriptorWriter` stores stable indices while writes are assembled and materializes `VkWriteDescriptorSet` pointers only in `overwrite()`, after its backing vectors have reached their final size. Vector growth therefore cannot leave dangling descriptor-info pointers.

## Legacy code

The following files belong to the old render-pass path and live in `YATAVK::Legacy`:

- `VulkanCommon.h`;
- `VulkanFrameBuffer.h/.cpp`;
- `VulkanPipeline.hpp`;
- `VulkanRenderPass.hpp`;
- `VulkanSampler.h/.cpp`.

This path is known broken, unsupported, and intentionally excluded from the default target. Setting `YATAVK_BUILD_LEGACY=ON` fails configuration instead of suggesting that the code is usable. `YataVK.cpp`, the old source-aggregation entry point, is also unsupported and must not be compiled. The legacy sources are retained only for archaeology until they are deleted or rewritten.

## ImGui

Consumers must initialize Dear ImGui with `UseDynamicRendering = true` and a matching `VkPipelineRenderingCreateInfo`. YataVK does not provide a legacy ImGui render-pass compatibility path.
