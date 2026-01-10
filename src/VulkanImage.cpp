//
// Created by Zhuoran Yi on 25-3-4.
//
// #define STB_IMAGE_IMPLEMENTATION
// #include <stb_image.h>

#include "YataVK/VulkanImage.hpp"

namespace YATAVK {
    void VulkanImage::init() {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent = extent;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = format;//VK_FORMAT_D32_SFLOAT;
        imageInfo.tiling = tiling;//VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = usage; // VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

#ifdef YATAVK_ENABLE_VMA
        VmaAllocationCreateInfo allocCreateInfo = {};
        allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;

        if (vmaCreateImage(device->getVmaAllocator(), &imageInfo, &allocCreateInfo, &vkImage, &vmaAllocation, nullptr) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create image with VMA!");
        }
#else
        if (vkCreateImage(device->getLogicalDevice(), &imageInfo, nullptr, &vkImage) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create image!");
        }

        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(device->getLogicalDevice(), vkImage, &memRequirements);

        VkMemoryAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = device->findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        if (vkAllocateMemory(device->getLogicalDevice(), &allocInfo, nullptr, &vkImageMemory) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate image memory!");
        }
        vkBindImageMemory(device->getLogicalDevice(), vkImage, vkImageMemory, 0);
#endif
        /*-------------------------------------------------------------------------------------------*/

        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.subresourceRange = {};
        viewInfo.subresourceRange.aspectMask = aspect;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;
        viewInfo.image = vkImage;

        if (vkCreateImageView(device->getLogicalDevice(), &viewInfo, nullptr, &vkImageView) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create shadow map view!");
        }
    }

    void VulkanImage::cleanUp() {
        // if(imageData != nullptr) {
        //     delete[] static_cast<stbi_uc*>(imageData);
        // }
        if (vkImageView != VK_NULL_HANDLE) {
            vkDestroyImageView(device->getLogicalDevice(), vkImageView, nullptr);
        }
#ifdef YATAVK_ENABLE_VMA
        if (vmaAllocation != VK_NULL_HANDLE) {
            vmaDestroyImage(device->getVmaAllocator(), vkImage, vmaAllocation);
            vmaAllocation = nullptr;
            vkImage = VK_NULL_HANDLE;
        }
        if (vkImage != VK_NULL_HANDLE) {
            vkDestroyImage(device->getLogicalDevice(), vkImage, nullptr);
            vkImage = VK_NULL_HANDLE;
        }
#else
        if (vkImage != VK_NULL_HANDLE) {
            vkDestroyImage(device->getLogicalDevice(), vkImage, nullptr);
        }
        if (vkImageMemory != VK_NULL_HANDLE) {
            vkFreeMemory(device->getLogicalDevice(), vkImageMemory, nullptr);
        }
#endif
    }

    void VulkanImage::transitionImageLayout(VkImageLayout oldLayout, VkImageLayout newLayout) const {
        vkResetCommandBuffer(device->getCommandBuffer(), 0); // 我们还是复用同一个 Command Buffer(因为加载图片只会做一次, 不是每一帧都要干的事)
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        beginInfo.pInheritanceInfo = nullptr;
        /*-------------------------------------------------------------------------------*/
        if (vkBeginCommandBuffer(device->getCommandBuffer(), &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("Failed to begin recording command buffer!");
        }

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = vkImage;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        VkPipelineStageFlags sourceStage;
        VkPipelineStageFlags destinationStage;

        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        } else {
            throw std::invalid_argument("unsupported layout transition!");
        }

        vkCmdPipelineBarrier(
            device->getCommandBuffer(),
            sourceStage, destinationStage,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier
        );

        vkEndCommandBuffer(device->getCommandBuffer());

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &device->getpCommandBuffer();

        vkQueueSubmit(device->getGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(device->getGraphicsQueue());
    }


    // void VulkanImage::loadImage(std::string path) {
    //     stbi_set_flip_vertically_on_load(true); // 这里需要翻转, stbi加载的图片原点和Blender/Unity不一致
    //     stbi_uc* pixels = stbi_load(path.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    //     VkDeviceSize imageSize = texWidth * texHeight * texChannels; // 这里的size应该是宽*高*通道数吧, 通道数不是恒为 4
    //
    //     if (!pixels) {
    //         throw std::runtime_error("failed to load texture image!");
    //     }
    //
    //     VkBuffer stagingBuffer = VK_NULL_HANDLE;
    //     VkDeviceMemory stagingBufferMemory = VK_NULL_HANDLE;
    //     device->createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);
    //     void* data;
    //     vkMapMemory(device->getLogicalDevice(), stagingBufferMemory, 0, imageSize, 0, &data);
    //     memcpy(data, pixels, static_cast<size_t>(imageSize));
    //     vkUnmapMemory(device->getLogicalDevice(), stagingBufferMemory);
    //     imageData = new stbi_uc[imageSize];
    //     memcpy(imageData, pixels, static_cast<size_t>(imageSize));
    //     stbi_image_free(pixels);
    //
    //     vkResetCommandBuffer(device->getCommandBuffer(), 0); // 我们还是复用同一个 Command Buffer(因为加载图片只会做一次, 不是每一帧都要干的事)
    //     VkCommandBufferBeginInfo beginInfo{};
    //     beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    //     beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    //     beginInfo.pInheritanceInfo = nullptr;
    //     /*-------------------------------------------------------------------------------*/
    //     if (vkBeginCommandBuffer(device->getCommandBuffer(), &beginInfo) != VK_SUCCESS) {
    //         throw std::runtime_error("Failed to begin recording command buffer!");
    //     }
    //
    //     VkBufferImageCopy region{};
    //     region.bufferOffset = 0;
    //     region.bufferRowLength = 0;
    //     region.bufferImageHeight = 0;
    //     region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    //     region.imageSubresource.mipLevel = 0;
    //     region.imageSubresource.baseArrayLayer = 0;
    //     region.imageSubresource.layerCount = 1;
    //     region.imageOffset = {0, 0, 0};
    //     region.imageExtent = {
    //         static_cast<uint32_t>(texWidth),
    //         static_cast<uint32_t>(texHeight),
    //         1
    //     };
    //
    //     vkCmdCopyBufferToImage(device->getCommandBuffer(), stagingBuffer, vkImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    //
    //     vkEndCommandBuffer(device->getCommandBuffer());
    //
    //     VkSubmitInfo submitInfo{};
    //     submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    //     submitInfo.commandBufferCount = 1;
    //     submitInfo.pCommandBuffers = &device->getpCommandBuffer();
    //
    //     vkQueueSubmit(device->getGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
    //     vkQueueWaitIdle(device->getGraphicsQueue());
    //     vkDestroyBuffer(device->getLogicalDevice(), stagingBuffer, nullptr); // 复制完成后要把staging buffer清掉
    //     vkFreeMemory(device->getLogicalDevice(), stagingBufferMemory, nullptr); // staging buffer的内存也要清掉
    // }

    // Eigen::Vector4f VulkanImage::sample(Eigen::Vector2f uv){ // 其实要考虑有几个通道, 但我们加载的时候就规定了格式是STBI_rgb_alpha, 所以texChannels一定是4
    //     uv = {std::clamp(uv.x(), 0.0f, 1.0f), std::clamp(uv.y(),0.0f,1.0f)};
    //
    //     float x = uv.x() * (texWidth - 1);
    //     float y = uv.y() * (texHeight - 1);
    //
    //     // 四舍五入到最近像素点
    //     int ix = static_cast<int>(std::round(x));
    //     int iy = static_cast<int>(std::round(y));
    //
    //     // 计算像素索引（每像素4通道，RGBA）
    //     int idx = (iy * texWidth + ix) * texChannels;
    //
    //     return Eigen::Vector4f(
    //         static_cast<stbi_uc*>(imageData)[idx + 0] / 255.0f,
    //         static_cast<stbi_uc*>(imageData)[idx + 1] / 255.0f,
    //         static_cast<stbi_uc*>(imageData)[idx + 2] / 255.0f,
    //         static_cast<stbi_uc*>(imageData)[idx + 3] / 255.0f
    //     );
    // }
}
