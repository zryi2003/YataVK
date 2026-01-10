# YataVK

**YataVK** is a lightweight, modern C++20 Vulkan wrapper designed for rapid prototyping and research. It abstracts the verbosity of Vulkan while maintaining explicit control over the graphics pipeline.

> Extracted from the **Yata** framework.

## Key Features

### RAII Resource Management
All Vulkan resources (`VkDevice`, `VkImage`, `VkBuffer`, etc.) are managed via RAII wrappers, ensuring proper cleanup order and preventing resource leaks.

### Decoupled Architecture
- **Device & SwapChain Separation**: The `VulkanDevice` is a pure resource factory and handle provider, completely decoupled from the windowing system (`VulkanSwapChain`).
- **Factory Pattern**: Centralized resource creation logic within `VulkanDevice` to simplify memory allocation and object creation.

### C++20 Concepts & Templates
Pipeline and RenderPass creation utilizes C++20 Concepts (`PipelineProvider`, `RenderPassProvider`) to enforce compile-time contracts without the overhead of virtual inheritance.

- **Zero-Overhead Abstraction**: Logic is resolved at compile time.
- **Flexible Providers**: Users define their own providers to describe pipeline states and render pass attachments.

### Components
- **VulkanDevice**: Logical device wrapper and resource factory.
- **VulkanSwapChain**: Handles presentation, image views, and resizing logic.

## Usage Example

```cpp
// Initialize Device
auto device = new YATAVK::VulkanDevice(instance, surface);

// Create SwapChain
auto swapChain = new YATAVK::VulkanSwapChain(device, width, height);

// Define a RenderPass using a Provider
YATA::TranglePassProvider passProvider(swapChain->getImageFormat(), VK_FORMAT_D32_SFLOAT);
auto renderPass = new YATAVK::VulkanRenderPass(device, std::move(passProvider));

// Define a Pipeline using a Provider
YATA::TrianglePipelineProvider pipelineProvider(device, renderPass->getHandle(), extent);
auto pipeline = new YATAVK::VulkanPipeline(device, std::move(pipelineProvider));
```

## Special Features

- **Built-in "Ancient Relic" Achievement Detector**: Automatically detects systems that shouldn't exist in 2026.
