#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include <tuple>
#include <vector>

namespace vulkanctx {

struct SwapChain {
    VkSwapchainKHR handle;
    uint32_t count;
    VkFormat format;
    VkExtent2D extent;
};

struct GraphicsPipeline {
    VkPipelineLayout layout;
    VkPipeline handle;
};

struct SynchronizationObject {
    const uint32_t amount;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkFence> inFlightFences;
    std::vector<VkFence> imagesInFlight;
};

auto setupDebugMessenger(const VkInstance &instance)
    -> VkDebugUtilsMessengerEXT;

auto createInstance(const char *application_name) -> VkInstance;
auto createSurface(const VkInstance &instance, GLFWwindow *window)
    -> VkSurfaceKHR;
auto pickPhysicalDevice(const VkInstance &instance, const VkSurfaceKHR &surface)
    -> VkPhysicalDevice;
auto createLogicalDevice(const VkPhysicalDevice &physicalDevice,
                         const VkSurfaceKHR &surface) -> VkDevice;

auto getGraphicsQueue(const VkDevice &device,
                      const VkPhysicalDevice &physicalDevice,
                      const VkSurfaceKHR &surface) -> VkQueue;
auto getPresentQueue(const VkDevice &device,
                     const VkPhysicalDevice &physicalDevice,
                     const VkSurfaceKHR &surface) -> VkQueue;

auto createSwapChain(const VkDevice &device,
                     const VkPhysicalDevice &physicalDevice,
                     const VkSurfaceKHR &surface,
                     GLFWwindow *glfwWindowPtr) -> SwapChain;

auto retriveSwapChainImages(const VkDevice &device,
                            const VkSwapchainKHR &swapChain,
                            uint32_t &imageCount) -> std::vector<VkImage>;
auto createImageViews(const VkDevice &device,
                      const std::vector<VkImage> &swapChainImages,
                      const VkFormat &swapChainImageFormat)
    -> std::vector<VkImageView>;

auto createRenderPass(const VkDevice &device, const VkFormat &swapChainFormat)
    -> VkRenderPass;
auto createGraphicsPipeline(const VkDevice &device,
                            const VkRenderPass &renderPass,
                            const VkExtent2D &swapChainExtent)
    -> vulkanctx::GraphicsPipeline;

auto createFramebuffers(const VkDevice &device,
                        const VkRenderPass &renderPass,
                        const std::vector<VkImageView> &swapChainImageViews,
                        const VkExtent2D &swapChainExtent)
    -> std::vector<VkFramebuffer>;

auto createCommandPool(const VkDevice &device,
                       const VkPhysicalDevice &physicalDevice,
                       const VkSurfaceKHR &surface) -> VkCommandPool;
auto createCommandBuffers(
    const VkDevice &device,
    const VkExtent2D &swapChainExtent,
    const VkRenderPass &renderPass,
    const VkPipeline &graphicsPipeline,
    const VkCommandPool &commandPool,
    const std::vector<VkFramebuffer> &swapChainFramebuffers)
    -> std::vector<VkCommandBuffer>;

auto createSynchronizationObject(const VkDevice &device,
                                 const uint32_t &amount,
                                 const uint32_t &swapChainImagesSize)
    -> SynchronizationObject;

auto drawFrame(const VkDevice &device,
               const vulkanctx::SwapChain &swapChain,
               const std::vector<VkCommandBuffer> &commandBuffers,
               const VkQueue &graphicsQueue,
               const VkQueue &presentQueue,
               SynchronizationObject &synchronizationObject,
               const uint32_t &currentFrame) -> void;

auto recreateSwapChain(const VkDevice &device,
                       const VkPhysicalDevice &physicalDevice,
                       const VkSurfaceKHR &surface,
                       GLFWwindow *glfwWindowPtr) -> void;

auto cleanup(const VkInstance &instance,
             const VkDevice &device,
             const VkSurfaceKHR &surface,
             const VkSwapchainKHR &swapChain,
             const std::vector<VkImageView> &swapChainImageViews,
             const VkRenderPass &renderPass,
             const VkPipelineLayout &pipelineLayout,
             const VkPipeline &pipeline,
             const std::vector<VkFramebuffer> &frambuffers,
             const VkCommandPool &commandPool,
             const SynchronizationObject &synchronizationObject,
             const VkDebugUtilsMessengerEXT &debugMessenger) -> void;

} // namespace vulkanctx
