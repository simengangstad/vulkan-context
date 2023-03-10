#include <GLFW/glfw3.h>

#include <iostream>

#include "vulkan_context.h"

#define MAX_FRAMES_IN_FLIGHT 2
#define APP_NAME             "Vulkan"
#define WIDTH                800
#define HEIGHT               600

namespace app {

auto initializeWindow(const int width, const int height, const char *title)
    -> GLFWwindow * {
    glfwInit();

    // Don't initialize OpenGL context
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    return glfwCreateWindow(width, height, title, nullptr, nullptr);
}

auto cleanup(GLFWwindow *window) -> void {
    glfwDestroyWindow(window);
    glfwTerminate();
}

} // namespace app

int main() {
    try {
        auto windowPtr = app::initializeWindow(WIDTH, HEIGHT, APP_NAME);

        auto instance = vulkanctx::createInstance(APP_NAME);
        auto debugMessenger = vulkanctx::setupDebugMessenger(instance);
        auto surface = vulkanctx::createSurface(instance, windowPtr);
        auto physicalDevice = vulkanctx::pickPhysicalDevice(instance, surface);
        auto device = vulkanctx::createLogicalDevice(physicalDevice, surface);

        auto graphicsQueue =
            vulkanctx::getGraphicsQueue(device, physicalDevice, surface);
        auto presentQueue =
            vulkanctx::getPresentQueue(device, physicalDevice, surface);

        auto swapChain = vulkanctx::createSwapChain(
            device, physicalDevice, surface, windowPtr);
        auto swapChainImages = vulkanctx::retriveSwapChainImages(
            device, swapChain.handle, swapChain.count);
        auto swapChainImageViews = vulkanctx::createImageViews(
            device, swapChainImages, swapChain.format);

        auto renderPass = vulkanctx::createRenderPass(device, swapChain.format);
        auto graphicsPipeline = vulkanctx::createGraphicsPipeline(
            device, renderPass, swapChain.extent);
        auto framebuffers = vulkanctx::createFramebuffers(
            device, renderPass, swapChainImageViews, swapChain.extent);

        auto commandPool =
            vulkanctx::createCommandPool(device, physicalDevice, surface);
        auto commandBuffers =
            vulkanctx::createCommandBuffers(device,
                                            swapChain.extent,
                                            renderPass,
                                            graphicsPipeline.handle,
                                            commandPool,
                                            framebuffers);

        auto synchronizationObject = vulkanctx::createSynchronizationObject(
            device, MAX_FRAMES_IN_FLIGHT, swapChainImages.size());

        size_t currentFrame = 0;

        while (!glfwWindowShouldClose(windowPtr)) {
            glfwPollEvents();

            vulkanctx::drawFrame(device,
                                 swapChain,
                                 commandBuffers,
                                 graphicsQueue,
                                 presentQueue,
                                 synchronizationObject,
                                 currentFrame);

            currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
        }

        vkDeviceWaitIdle(device);

        vulkanctx::cleanup(instance,
                           device,
                           surface,
                           swapChain.handle,
                           swapChainImageViews,
                           renderPass,
                           graphicsPipeline.layout,
                           graphicsPipeline.handle,
                           framebuffers,
                           commandPool,
                           synchronizationObject,
                           debugMessenger);
        app::cleanup(windowPtr);

    } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
