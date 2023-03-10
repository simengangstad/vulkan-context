#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <optional>
#include <set>
#include <stdexcept>

#include "vulkan_context.h"

#define UNUSED(x) (void)(x)

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete() {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

static auto findQueueFamilies(const VkPhysicalDevice &device,
                              const VkSurfaceKHR &surface)
    -> QueueFamilyIndices;

static auto querySwapChainSupport(const VkPhysicalDevice &device,
                                  const VkSurfaceKHR &surface)
    -> SwapChainSupportDetails;

static const std::vector<const char *> validationLayers = {
    "VK_LAYER_KHRONOS_validation"};

static const std::vector<const char *> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME};

#ifdef NODEBUG
static const bool enableValidationLayers = false;
#else
static const bool enableValidationLayers = true;
#endif

// ---------------------------------------------------------------------------//
//                          Validation layer                                  //
// ---------------------------------------------------------------------------//

static VKAPI_ATTR VkBool32 VKAPI_CALL
debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
              VkDebugUtilsMessageTypeFlagsEXT messageType,
              const VkDebugUtilsMessengerCallbackDataEXT *callbackDataPtr,
              void *userDataPtr) {
    UNUSED(messageSeverity);
    UNUSED(messageType);
    UNUSED(userDataPtr);

    std::cerr << "Validation layer: " << callbackDataPtr->pMessage << std::endl;
    return VK_FALSE;
}

static auto createDebugUtilsMessengerEXT(
    const VkInstance &instance,
    const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
    const VkAllocationCallbacks *pAllocator,
    VkDebugUtilsMessengerEXT *pDebugMessenger) -> VkResult {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
        instance, "vkCreateDebugUtilsMessengerEXT");

    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

static auto
destroyDebugUtilsMessengerEXT(const VkInstance &instance,
                              const VkDebugUtilsMessengerEXT &debugMessenger,
                              const VkAllocationCallbacks *pAllocator) -> void {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
        instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}

static auto
populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT &createInfo)
    -> void {
    createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
}

auto vulkanctx::setupDebugMessenger(const VkInstance &instance)
    -> VkDebugUtilsMessengerEXT {
    if (!enableValidationLayers)
        return nullptr;

    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    populateDebugMessengerCreateInfo(createInfo);

    VkDebugUtilsMessengerEXT debugMessenger;

    if (createDebugUtilsMessengerEXT(
            instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
        throw std::runtime_error("Failed to set up debug messenger");
    }

    return debugMessenger;
}

static auto
checkValidationLayerSupport(const std::vector<const char *> &validationLayers)
    -> bool {
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const char *layerName : validationLayers) {
        bool layerFound = false;

        for (const auto &layerProperties : availableLayers) {
            if (strcmp(layerName, layerProperties.layerName) == 0) {
                layerFound = true;
                break;
            }
        }

        if (!layerFound) {
            return false;
        }
    }

    return true;
}

// ---------------------------------------------------------------------------//
//                                  Extensions                                //
// ---------------------------------------------------------------------------//

static auto getRequiredExtensions(const bool &enableValidationLayers)
    -> std::vector<const char *> {
    uint32_t glfwExtensionCount = 0;
    const char **glfwExtensions =
        glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector<const char *> extensions(glfwExtensions,
                                         glfwExtensions + glfwExtensionCount);

    if (enableValidationLayers) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return extensions;
}

static auto checkDeviceExtensionSupport(const VkPhysicalDevice &device)
    -> bool {
    uint32_t extensionCount;

    vkEnumerateDeviceExtensionProperties(
        device, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(
        device, nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> requiredExtensions(deviceExtensions.begin(),
                                             deviceExtensions.end());

    // Tick of the needed extensions
    for (const auto &extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }

    // If all extensions got ticked off, we're good to go
    return requiredExtensions.empty();
}

// ---------------------------------------------------------------------------//
//                           Instance & surface                               //
// ---------------------------------------------------------------------------//

auto vulkanctx::createInstance(const char *application_name) -> VkInstance {
    if (enableValidationLayers &&
        !checkValidationLayerSupport(validationLayers)) {
        throw std::runtime_error(
            "Validation layers requested, but not available");
    }

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = application_name;
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    auto extensions = getRequiredExtensions(enableValidationLayers);
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;

    if (enableValidationLayers) {
        createInfo.enabledLayerCount =
            static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();

        populateDebugMessengerCreateInfo(debugCreateInfo);
        createInfo.pNext =
            (VkDebugUtilsMessengerCreateInfoEXT *)&debugCreateInfo;
    } else {
        createInfo.enabledLayerCount = 0;
        createInfo.pNext = nullptr;
    }

    VkInstance instance{};
    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan instance");
    }

    return instance;
}

auto vulkanctx::createSurface(const VkInstance &instance, GLFWwindow *window)
    -> VkSurfaceKHR {
    VkSurfaceKHR surface;

    if (glfwCreateWindowSurface(instance, window, nullptr, &surface) !=
        VK_SUCCESS) {
        throw std::runtime_error("Failed to create window surface");
    }

    return surface;
}

// ---------------------------------------------------------------------------//
//                                   Device                                   //
// ---------------------------------------------------------------------------//

static auto isDeviceSuitable(const VkPhysicalDevice &device,
                             const VkSurfaceKHR &surface) -> bool {
    QueueFamilyIndices indices = findQueueFamilies(device, surface);

    bool extensionSupported = checkDeviceExtensionSupport(device);

    bool swapChainAdequate = false;

    if (extensionSupported) {
        SwapChainSupportDetails swapChainSupport =
            querySwapChainSupport(device, surface);

        swapChainAdequate = !swapChainSupport.formats.empty() &&
                            !swapChainSupport.presentModes.empty();
    }

    return indices.isComplete() && extensionSupported && swapChainAdequate;
}

auto vulkanctx::pickPhysicalDevice(const VkInstance &instance,
                                   const VkSurfaceKHR &surface)
    -> VkPhysicalDevice {
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;

    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

    if (deviceCount == 0) {
        throw std::runtime_error("Failed to find GPUs with Vulkan support");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    for (const auto &device : devices) {
        if (isDeviceSuitable(device, surface)) {
            physicalDevice = device;
            break;
        }
    }

    if (physicalDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("Failed to find suitable GPU");
    }

    return physicalDevice;
}

auto vulkanctx::createLogicalDevice(const VkPhysicalDevice &physicalDevice,
                                    const VkSurfaceKHR &surface) -> VkDevice {
    QueueFamilyIndices indices = findQueueFamilies(physicalDevice, surface);

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(),
                                              indices.presentFamily.value()};

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures{};

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount =
        static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount =
        static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    if (enableValidationLayers) {
        createInfo.enabledLayerCount =
            static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    } else {
        createInfo.enabledLayerCount = 0;
    }

    VkDevice device;
    if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) !=
        VK_SUCCESS) {
        throw std::runtime_error("Failed to create logical device");
    }

    return device;
}

// ---------------------------------------------------------------------------//
//                               Queues                                       //
// ---------------------------------------------------------------------------//

static auto findQueueFamilies(const VkPhysicalDevice &device,
                              const VkSurfaceKHR &surface)
    -> QueueFamilyIndices {
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(
        device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(
        device, &queueFamilyCount, queueFamilies.data());

    int i = 0;

    for (const auto &queueFamily : queueFamilies) {
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
        }

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(
            device, i, surface, &presentSupport);

        if (presentSupport) {
            indices.presentFamily = i;
        }

        if (indices.isComplete()) {
            break;
        }

        i++;
    }

    return indices;
}

auto vulkanctx::getGraphicsQueue(const VkDevice &device,
                                 const VkPhysicalDevice &physicalDevice,
                                 const VkSurfaceKHR &surface) -> VkQueue {
    VkQueue graphicsQueue;
    QueueFamilyIndices indices = findQueueFamilies(physicalDevice, surface);

    vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);

    return graphicsQueue;
}

auto vulkanctx::getPresentQueue(const VkDevice &device,
                                const VkPhysicalDevice &physicalDevice,
                                const VkSurfaceKHR &surface) -> VkQueue {
    VkQueue presentQueue;
    QueueFamilyIndices indices = findQueueFamilies(physicalDevice, surface);

    vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);

    return presentQueue;
}

// ---------------------------------------------------------------------------//
//                                 Swap chain                                 //
// ---------------------------------------------------------------------------//

static auto querySwapChainSupport(const VkPhysicalDevice &device,
                                  const VkSurfaceKHR &surface)
    -> SwapChainSupportDetails {
    SwapChainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        device, surface, &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(
        device, surface, &formatCount, nullptr);

    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(
            device, surface, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(
        device, surface, &presentModeCount, nullptr);

    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);

        vkGetPhysicalDeviceSurfacePresentModesKHR(
            device, surface, &presentModeCount, details.presentModes.data());
    }

    return details;
}

static auto
chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats)
    -> VkSurfaceFormatKHR {
    for (const auto &availableFormats : availableFormats) {
        if (availableFormats.format == VK_FORMAT_B8G8R8A8_SRGB &&
            availableFormats.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormats;
        }
    }

    // Settle on the first format if the above specification isn't available
    return availableFormats[0];
}

static auto chooseSwapPresentMode(
    const std::vector<VkPresentModeKHR> &availablePresentModes)
    -> VkPresentModeKHR {
    for (const auto &availablePresentModes : availablePresentModes) {
        // Prefer to have a mailbox where if the queue is full we just replace
        // with newer ones and don't block
        if (availablePresentModes == VK_PRESENT_MODE_MAILBOX_KHR) {
            return availablePresentModes;
        }
    }

    // Fall back to blocking FIFO queue
    return VK_PRESENT_MODE_FIFO_KHR;
}

static auto chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities,
                             GLFWwindow *glfwWindowPtr) -> VkExtent2D {
    // If the context don't allow us to differ in resolution of the swap
    // chain and the actual window
    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    } else {
        // Elsewise we can pick the best resolution suited
        int width, height;
        glfwGetFramebufferSize(glfwWindowPtr, &width, &height);

        VkExtent2D actualExtent = {static_cast<uint32_t>(width),
                                   static_cast<uint32_t>(height)};

        // Clamp between the minimum and maximum extents supported
        actualExtent.width = std::max(
            capabilities.minImageExtent.width,
            std::min(capabilities.maxImageExtent.width, actualExtent.width));

        actualExtent.height = std::max(
            capabilities.minImageExtent.height,
            std::min(capabilities.maxImageExtent.height, actualExtent.height));

        return actualExtent;
    }
}

auto vulkanctx::createSwapChain(const VkDevice &device,
                                const VkPhysicalDevice &physicalDevice,
                                const VkSurfaceKHR &surface,
                                GLFWwindow *glfwWindowPtr)
    -> vulkanctx::SwapChain {
    SwapChainSupportDetails swapChainSupport =
        querySwapChainSupport(physicalDevice, surface);

    VkSurfaceFormatKHR surfaceFormat =
        chooseSwapSurfaceFormat(swapChainSupport.formats);

    VkPresentModeKHR presentMode =
        chooseSwapPresentMode(swapChainSupport.presentModes);

    VkExtent2D extent =
        chooseSwapExtent(swapChainSupport.capabilities, glfwWindowPtr);

    // Have one image extra to prevent waiting for the driver
    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;

    // Use the max availabe image count if greater than min count
    // A max image count of 0, means that there is no upper bound,
    // so we ought to stick with the count stated in the previous
    // declaration
    if (swapChainSupport.capabilities.maxImageCount > 0 &&
        imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;

    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    QueueFamilyIndices indices = findQueueFamilies(physicalDevice, surface);

    uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(),
                                     indices.presentFamily.value()};

    if (indices.graphicsFamily != indices.presentFamily) {
        // Not explicit ownership
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        // Strict ownership of image
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices = nullptr;
    }

    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;

    // Not blend with other windows
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    VkSwapchainKHR swapChain;

    if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain)) {
        throw std::runtime_error("Failed to create swap chain");
    }

    return SwapChain{swapChain, imageCount, surfaceFormat.format, extent};
}

auto vulkanctx::retriveSwapChainImages(const VkDevice &device,
                                       const VkSwapchainKHR &swapChain,
                                       uint32_t &imageCount)
    -> std::vector<VkImage> {
    std::vector<VkImage> swapChainImages;
    vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
    swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(
        device, swapChain, &imageCount, swapChainImages.data());
    return swapChainImages;
}

// ---------------------------------------------------------------------------//
//                                Image views                                 //
// ---------------------------------------------------------------------------//

auto vulkanctx::createImageViews(const VkDevice &device,
                                 const std::vector<VkImage> &swapChainImages,
                                 const VkFormat &swapChainImageFormat)
    -> std::vector<VkImageView> {
    std::vector<VkImageView> swapChainImageViews;
    swapChainImageViews.resize(swapChainImages.size());

    for (size_t i = 0; i < swapChainImages.size(); i++) {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = swapChainImages[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = swapChainImageFormat;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        VkResult result = vkCreateImageView(
            device, &createInfo, nullptr, &swapChainImageViews[i]);

        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to create image view");
        }
    }

    return swapChainImageViews;
}

// ---------------------------------------------------------------------------//
//                                Shaders                                     //
// ---------------------------------------------------------------------------//

static auto readFile(const std::string &fileName) -> std::vector<char> {
    // Start reading at end of the file and specify that the file is binary
    std::ifstream file(fileName, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file!");
    }

    // Since we start at end of file, we can get the file size
    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);

    file.close();

    return buffer;
}

static auto createShaderModule(const VkDevice &device,
                               const std::vector<char> &code)
    -> VkShaderModule {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t *>(code.data());

    VkShaderModule shaderModule;

    VkResult result =
        vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule);

    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module");
    }

    return shaderModule;
}

// ---------------------------------------------------------------------------//
//                               Pipeline                                     //
// ---------------------------------------------------------------------------//

auto vulkanctx::createRenderPass(const VkDevice &device,
                                 const VkFormat &swapChainFormat)
    -> VkRenderPass {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapChainFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.pDependencies = &dependency;

    VkRenderPass renderPass;

    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) !=
        VK_SUCCESS) {
        throw std::runtime_error("Failed to create render pass.");
    }

    return renderPass;
}

auto vulkanctx::createGraphicsPipeline(const VkDevice &device,
                                       const VkRenderPass &renderPass,
                                       const VkExtent2D &swapChainExtent)
    -> vulkanctx::GraphicsPipeline {
    auto vertexShaderCode = readFile("shaders/shader.vert.spv");
    auto fragmentShaderCode = readFile("shaders/shader.frag.spv");

    VkShaderModule vertexShaderModule =
        createShaderModule(device, vertexShaderCode);
    VkShaderModule fragmentShaderModule =
        createShaderModule(device, fragmentShaderCode);

    VkPipelineShaderStageCreateInfo vertexShaderStageInfo{};
    vertexShaderStageInfo.sType =
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertexShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertexShaderStageInfo.module = vertexShaderModule;
    vertexShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragmentShaderStageInfo{};
    fragmentShaderStageInfo.sType =
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragmentShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragmentShaderStageInfo.module = fragmentShaderModule;
    fragmentShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertexShaderStageInfo,
                                                      fragmentShaderStageInfo};

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 0;
    vertexInputInfo.vertexAttributeDescriptionCount = 0;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType =
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)swapChainExtent.width;
    viewport.height = (float)swapChainExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = swapChainExtent;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType =
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;
    rasterizer.depthBiasConstantFactor = 0.0f;
    rasterizer.depthBiasClamp = 0.0f;
    rasterizer.depthBiasSlopeFactor = 0.0f;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType =
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading = 1.0f;
    multisampling.pSampleMask = nullptr;
    multisampling.alphaToCoverageEnable = VK_FALSE;
    multisampling.alphaToOneEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType =
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f;
    colorBlending.blendConstants[1] = 0.0f;
    colorBlending.blendConstants[2] = 0.0f;
    colorBlending.blendConstants[3] = 0.0f;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 0;
    pipelineLayoutInfo.pushConstantRangeCount = 0;

    VkPipelineLayout pipelineLayout;

    if (vkCreatePipelineLayout(
            device, &pipelineLayoutInfo, nullptr, &pipelineLayout) !=
        VK_SUCCESS) {
        throw std::runtime_error("Failed to create pipeline layout");
    }

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    VkPipeline pipeline;

    if (vkCreateGraphicsPipelines(
            device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) !=
        VK_SUCCESS) {
        throw std::runtime_error("Failed to create graphics pipeline");
    }

    // Can destroy the shader modules as they are loaded into the pipeline and
    // no longer needed
    vkDestroyShaderModule(device, vertexShaderModule, nullptr);
    vkDestroyShaderModule(device, fragmentShaderModule, nullptr);

    return GraphicsPipeline{pipelineLayout, pipeline};
}

// ---------------------------------------------------------------------------//
//                                Framebuffers                                //
// ---------------------------------------------------------------------------//

auto vulkanctx::createFramebuffers(
    const VkDevice &device,
    const VkRenderPass &renderPass,
    const std::vector<VkImageView> &swapChainImageViews,
    const VkExtent2D &swapChainExtent) -> std::vector<VkFramebuffer> {
    std::vector<VkFramebuffer> swapChainFramebuffers(
        swapChainImageViews.size());

    for (size_t i = 0; i < swapChainImageViews.size(); i++) {
        VkImageView attachments[] = {swapChainImageViews[i]};

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = swapChainExtent.width;
        framebufferInfo.height = swapChainExtent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(
                device, &framebufferInfo, nullptr, &swapChainFramebuffers[i]) !=
            VK_SUCCESS) {
            throw std::runtime_error("Failed to create framebuffer");
        }
    }

    return swapChainFramebuffers;
}

// ---------------------------------------------------------------------------//
//                        Command pools & buffers                             //
// ---------------------------------------------------------------------------//

auto vulkanctx::createCommandPool(const VkDevice &device,
                                  const VkPhysicalDevice &physicalDevice,
                                  const VkSurfaceKHR &surface)
    -> VkCommandPool {
    VkCommandPool commandPool;

    QueueFamilyIndices queueFamilyIndices =
        findQueueFamilies(physicalDevice, surface);

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

    if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) !=
        VK_SUCCESS) {
        throw std::runtime_error("Failed to create command pool!");
    }

    return commandPool;
}

auto vulkanctx::createCommandBuffers(
    const VkDevice &device,
    const VkExtent2D &swapChainExtent,
    const VkRenderPass &renderPass,
    const VkPipeline &graphicsPipeline,
    const VkCommandPool &commandPool,
    const std::vector<VkFramebuffer> &swapChainFramebuffers)
    -> std::vector<VkCommandBuffer> {
    std::vector<VkCommandBuffer> commandBuffers(swapChainFramebuffers.size());

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (uint32_t)commandBuffers.size();

    if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) !=
        VK_SUCCESS) {
        throw std::runtime_error("Failed to create command buffers");
    }

    for (size_t i = 0; i < commandBuffers.size(); i++) {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        if (vkBeginCommandBuffer(commandBuffers[i], &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error(
                "Failed to begin recording command buffers.");
        }

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = renderPass;
        renderPassInfo.framebuffer = swapChainFramebuffers[i];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = swapChainExtent;

        VkClearValue clearColor = {0.0f, 0.0f, 0.0f, 1.0f};
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearColor;

        vkCmdBeginRenderPass(
            commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(commandBuffers[i],
                          VK_PIPELINE_BIND_POINT_GRAPHICS,
                          graphicsPipeline);

        vkCmdDraw(commandBuffers[i], 3, 1, 0, 0);

        vkCmdEndRenderPass(commandBuffers[i]);

        if (vkEndCommandBuffer(commandBuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to record command buffer!");
        }
    }

    return commandBuffers;
}

auto vulkanctx::drawFrame(const VkDevice &device,
                          const vulkanctx::SwapChain &swapChain,
                          const std::vector<VkCommandBuffer> &commandBuffers,
                          const VkQueue &graphicsQueue,
                          const VkQueue &presentQueue,
                          SynchronizationObject &synchronizationObject,
                          const uint32_t &currentFrame) -> void {
    vkWaitForFences(device,
                    1,
                    &synchronizationObject.inFlightFences[currentFrame],
                    VK_TRUE,
                    UINT64_MAX);

    uint32_t imageIndex;
    vkAcquireNextImageKHR(
        device,
        swapChain.handle,
        UINT64_MAX,
        synchronizationObject.imageAvailableSemaphores[currentFrame],
        VK_NULL_HANDLE,
        &imageIndex);

    // Check if a previous frame is using this image
    if (synchronizationObject.imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
        vkWaitForFences(device,
                        1,
                        &synchronizationObject.imagesInFlight[imageIndex],
                        VK_TRUE,
                        UINT64_MAX);
    }

    // Mark the image as now being in use by this frame
    synchronizationObject.imagesInFlight[imageIndex] =
        synchronizationObject.inFlightFences[currentFrame];

    VkSemaphore waitSemaphores[] = {
        synchronizationObject.imageAvailableSemaphores[currentFrame]};
    VkSemaphore signalSemaphores[] = {
        synchronizationObject.renderFinishedSemaphores[currentFrame]};

    VkPipelineStageFlags waitStages[] = {
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffers[imageIndex];
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    vkResetFences(
        device, 1, &synchronizationObject.inFlightFences[currentFrame]);

    if (vkQueueSubmit(graphicsQueue,
                      1,
                      &submitInfo,
                      synchronizationObject.inFlightFences[currentFrame]) !=
        VK_SUCCESS) {
        throw std::runtime_error("Failed to submit draw command buffer");
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = {swapChain.handle};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;

    vkQueuePresentKHR(presentQueue, &presentInfo);
}

// ---------------------------------------------------------------------------//
//                              Cleanup and misc                              //
// ---------------------------------------------------------------------------//

auto vulkanctx::createSynchronizationObject(const VkDevice &device,
                                            const uint32_t &amount,
                                            const uint32_t &swapChainImagesSize)
    -> SynchronizationObject {
    std::vector<VkSemaphore> imageAvailableSemaphores(amount);
    std::vector<VkSemaphore> renderFinishedSemaphores(amount);
    std::vector<VkFence> inFlightFences(amount);
    std::vector<VkFence> imagesInFlight(swapChainImagesSize, VK_NULL_HANDLE);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < amount; i++) {
        if (vkCreateSemaphore(device,
                              &semaphoreInfo,
                              nullptr,
                              &imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(device,
                              &semaphoreInfo,
                              nullptr,
                              &renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) !=
                VK_SUCCESS) {
            throw std::runtime_error("Failed to create semaphore");
        }
    }

    return SynchronizationObject{amount,
                                 imageAvailableSemaphores,
                                 renderFinishedSemaphores,
                                 inFlightFences,
                                 imagesInFlight};
}

// auto vulkanctx::cleanupSwapChain() -> void {}

auto vulkanctx::recreateSwapChain(const VkDevice &device,
                                  const VkPhysicalDevice &physicalDevice,
                                  const VkSurfaceKHR &surface,
                                  GLFWwindow *glfwWindowPtr) -> void {

    vkDeviceWaitIdle(device);

    SwapChain swapChain =
        createSwapChain(device, physicalDevice, surface, glfwWindowPtr);

    auto swapChainImages = vulkanctx::retriveSwapChainImages(
        device, swapChain.handle, swapChain.count);

    auto swapChainImageViews =
        vulkanctx::createImageViews(device, swapChainImages, swapChain.format);

    auto renderPass = vulkanctx::createRenderPass(device, swapChain.format);

    auto graphicsPipeline =
        vulkanctx::createGraphicsPipeline(device, renderPass, swapChain.extent);

    auto framebuffers = vulkanctx::createFramebuffers(
        device, renderPass, swapChainImageViews, swapChain.extent);
}

auto vulkanctx::cleanup(const VkInstance &instance,
                        const VkDevice &device,
                        const VkSurfaceKHR &surface,
                        const VkSwapchainKHR &swapChain,
                        const std::vector<VkImageView> &swapChainImageViews,
                        const VkRenderPass &renderPass,
                        const VkPipelineLayout &pipelineLayout,
                        const VkPipeline &pipeline,
                        const std::vector<VkFramebuffer> &swapChainFramebuffers,
                        const VkCommandPool &commandPool,
                        const SynchronizationObject &synchronizationObject,
                        const VkDebugUtilsMessengerEXT &debugMessenger)
    -> void {

    for (size_t i = 0; i < synchronizationObject.amount; i++) {
        vkDestroySemaphore(
            device, synchronizationObject.renderFinishedSemaphores[i], nullptr);
        vkDestroySemaphore(
            device, synchronizationObject.imageAvailableSemaphores[i], nullptr);
        vkDestroyFence(
            device, synchronizationObject.inFlightFences[i], nullptr);
    }

    vkDestroyCommandPool(device, commandPool, nullptr);

    for (auto framebuffer : swapChainFramebuffers) {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    }

    vkDestroyPipeline(device, pipeline, nullptr);
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    vkDestroyRenderPass(device, renderPass, nullptr);
    for (auto imageView : swapChainImageViews) {
        vkDestroyImageView(device, imageView, nullptr);
    }

    vkDestroySwapchainKHR(device, swapChain, nullptr);
    vkDestroyDevice(device, nullptr);

    if (enableValidationLayers && debugMessenger != nullptr) {
        destroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
    }

    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyInstance(instance, nullptr);
}
