#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include <tuple>
#include <vector>

namespace vulkanctx {

VkDebugUtilsMessengerEXT setupDebugMessenger(VkInstance instance);

VkInstance createInstance(const char* application_name);

VkSurfaceKHR createSurface(VkInstance instance, GLFWwindow* window);

VkPhysicalDevice pickPhysicalDevice(VkInstance instance, VkSurfaceKHR surface);

VkDevice createLogicalDevice(VkPhysicalDevice physicalDevice,
                             VkSurfaceKHR surface);

VkQueue getGraphicsQueue(VkDevice device,
                         VkPhysicalDevice physicalDevice,
                         VkSurfaceKHR surface);

VkQueue getPresentQueue(VkDevice device,
                        VkPhysicalDevice physicalDevice,
                        VkSurfaceKHR surface);
std::tuple<VkSwapchainKHR, uint32_t, VkFormat, VkExtent2D> createSwapChain(
    VkDevice device,
    VkPhysicalDevice physicalDevice,
    VkSurfaceKHR surface,
    GLFWwindow* glfwWindowPtr);

std::vector<VkImage> retriveSwapChainImages(VkDevice device,
                                            VkSwapchainKHR swapChain,
                                            uint32_t& imageCount);

std::vector<VkImageView> createImageViews(VkDevice device,
                                          std::vector<VkImage>& swapChainImages,
                                          VkFormat swapChainImageFormat);
void cleanup(VkInstance instance,
             VkDevice device,
             VkSurfaceKHR surface,
             VkSwapchainKHR swapChain,
             std::vector<VkImageView> swapChainImageViews,
             VkDebugUtilsMessengerEXT debugMessenger);

}  // namespace vulkanctx
