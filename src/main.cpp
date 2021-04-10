#include <GLFW/glfw3.h>

#include <iostream>

#include "vulkan_context.h"

namespace app {

GLFWwindow* initializeWindow(const int width,
                             const int height,
                             const char* title) {
    glfwInit();

    // Don't initialize OpenGL context
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    return glfwCreateWindow(width, height, title, nullptr, nullptr);
}

void cleanup(GLFWwindow* window) {
    glfwDestroyWindow(window);
    glfwTerminate();
}
}  // namespace app

const char* AppName = "Vulkan";
const uint32_t Width = 800;
const uint32_t Height = 600;

int main() {
    try {
        auto windowPtr = app::initializeWindow(Width, Height, AppName);

        auto instance = vulkanctx::createInstance(AppName);
        auto debugMessenger = vulkanctx::setupDebugMessenger(instance);
        auto surface = vulkanctx::createSurface(instance, windowPtr);
        auto physicalDevice = vulkanctx::pickPhysicalDevice(instance, surface);
        auto logicalDevice =
            vulkanctx::createLogicalDevice(physicalDevice, surface);

        auto graphicsQueue =
            vulkanctx::getGraphicsQueue(logicalDevice, physicalDevice, surface);
        auto presentQueue =
            vulkanctx::getPresentQueue(logicalDevice, physicalDevice, surface);

        auto [swapChain, imageCount, swapChainImageFormat, extent] =
            vulkanctx::createSwapChain(
                logicalDevice, physicalDevice, surface, windowPtr);

        auto images = vulkanctx::retriveSwapChainImages(
            logicalDevice, swapChain, imageCount);

        auto imageViews = vulkanctx::createImageViews(
            logicalDevice, images, swapChainImageFormat);

        while (!glfwWindowShouldClose(windowPtr)) {
            glfwPollEvents();
        }

        vulkanctx::cleanup(instance,
                           logicalDevice,
                           surface,
                           swapChain,
                           imageViews,
                           debugMessenger);
        app::cleanup(windowPtr);

    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
