#include "YataVK/VulkanFrameScheduler.hpp"

#include "YataVK/VulkanError.hpp"

#include <stdexcept>
#include <utility>

namespace YATAVK {

    VulkanFrameScheduler::VulkanFrameScheduler(VulkanDevice& device, uint32_t framesInFlight) : device_(&device) {
        if (framesInFlight == 0) {
            throw std::invalid_argument("VulkanFrameScheduler requires at least one frame in flight");
        }
        frames_.resize(framesInFlight);
        try {
            for (FrameResources& frame : frames_) {
                VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
                poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
                poolInfo.queueFamilyIndex = device_->getGraphicsQueueFamily();
                checkVk(vkCreateCommandPool(device_->getLogicalDevice(), &poolInfo, nullptr, &frame.commandPool),
                        "vkCreateCommandPool");

                VkCommandBufferAllocateInfo allocationInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
                allocationInfo.commandPool = frame.commandPool;
                allocationInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
                allocationInfo.commandBufferCount = 1;
                checkVk(vkAllocateCommandBuffers(device_->getLogicalDevice(), &allocationInfo, &frame.commandBuffer),
                        "vkAllocateCommandBuffers");

                VkSemaphoreCreateInfo semaphoreInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
                checkVk(vkCreateSemaphore(device_->getLogicalDevice(), &semaphoreInfo, nullptr, &frame.imageAvailable),
                        "vkCreateSemaphore(imageAvailable)");
                checkVk(vkCreateSemaphore(device_->getLogicalDevice(), &semaphoreInfo, nullptr, &frame.renderFinished),
                        "vkCreateSemaphore(renderFinished)");

                VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
                fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
                checkVk(vkCreateFence(device_->getLogicalDevice(), &fenceInfo, nullptr, &frame.inFlight),
                        "vkCreateFence");
            }
        } catch (...) {
            destroy();
            throw;
        }
    }

    VulkanFrameScheduler::~VulkanFrameScheduler() {
        destroy();
    }

    VulkanFrameScheduler::VulkanFrameScheduler(VulkanFrameScheduler&& other) noexcept
        : device_(std::exchange(other.device_, nullptr)), frames_(std::move(other.frames_)),
          currentFrame_(other.currentFrame_) {
    }

    VulkanFrameScheduler& VulkanFrameScheduler::operator=(VulkanFrameScheduler&& other) noexcept {
        if (this != &other) {
            destroy();
            device_ = std::exchange(other.device_, nullptr);
            frames_ = std::move(other.frames_);
            currentFrame_ = other.currentFrame_;
        }
        return *this;
    }

    VulkanBeginFrameResult VulkanFrameScheduler::beginFrame(const VulkanSwapChain& swapChain) {
        FrameResources& frame = frames_[currentFrame_];
        checkVk(vkWaitForFences(device_->getLogicalDevice(), 1, &frame.inFlight, VK_TRUE, UINT64_MAX),
                "vkWaitForFences");

        uint32_t imageIndex = 0;
        const VkResult acquireResult = swapChain.acquireNextImage(frame.imageAvailable, imageIndex);
        if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
            return {};
        }
        if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR) {
            checkVk(acquireResult, "vkAcquireNextImageKHR");
        }

        checkVk(vkResetFences(device_->getLogicalDevice(), 1, &frame.inFlight), "vkResetFences");
        checkVk(vkResetCommandPool(device_->getLogicalDevice(), frame.commandPool, 0), "vkResetCommandPool");
        VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        checkVk(vkBeginCommandBuffer(frame.commandBuffer, &beginInfo), "vkBeginCommandBuffer");

        VulkanBeginFrameResult result;
        result.status = VulkanFrameStatus::Ready;
        result.frame = VulkanFrameToken{frame.commandBuffer, imageIndex, currentFrame_};
        return result;
    }

    VulkanFrameStatus VulkanFrameScheduler::endFrame(const VulkanSwapChain& swapChain, const VulkanFrameToken& token) {
        if (token.frameIndex != currentFrame_) {
            throw std::logic_error("VulkanFrameToken does not belong to the active frame");
        }
        FrameResources& frame = frames_[currentFrame_];
        checkVk(vkEndCommandBuffer(frame.commandBuffer), "vkEndCommandBuffer");

        constexpr VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &frame.imageAvailable;
        submitInfo.pWaitDstStageMask = &waitStage;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &frame.commandBuffer;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &frame.renderFinished;
        checkVk(vkQueueSubmit(device_->getGraphicsQueue(), 1, &submitInfo, frame.inFlight), "vkQueueSubmit");

        const VkResult presentResult = swapChain.present(token.imageIndex, frame.renderFinished);
        currentFrame_ = (currentFrame_ + 1) % static_cast<uint32_t>(frames_.size());
        if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR) {
            return VulkanFrameStatus::SwapChainOutOfDate;
        }
        checkVk(presentResult, "vkQueuePresentKHR");
        return VulkanFrameStatus::Ready;
    }

    void VulkanFrameScheduler::waitIdle() const {
        device_->waitIdle();
    }

    void VulkanFrameScheduler::destroy() noexcept {
        if (device_ == nullptr) {
            return;
        }
        vkDeviceWaitIdle(device_->getLogicalDevice());
        for (FrameResources& frame : frames_) {
            if (frame.inFlight != VK_NULL_HANDLE) {
                vkDestroyFence(device_->getLogicalDevice(), frame.inFlight, nullptr);
            }
            if (frame.renderFinished != VK_NULL_HANDLE) {
                vkDestroySemaphore(device_->getLogicalDevice(), frame.renderFinished, nullptr);
            }
            if (frame.imageAvailable != VK_NULL_HANDLE) {
                vkDestroySemaphore(device_->getLogicalDevice(), frame.imageAvailable, nullptr);
            }
            if (frame.commandPool != VK_NULL_HANDLE) {
                vkDestroyCommandPool(device_->getLogicalDevice(), frame.commandPool, nullptr);
            }
        }
        frames_.clear();
        device_ = nullptr;
    }

} // namespace YATAVK
