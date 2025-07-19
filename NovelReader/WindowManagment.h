#pragma once

#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_sdl3.h"
#include "ImGui/imgui_impl_vulkan.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <functional>
#include <string>
#include <ctime>

// Forward declarations
struct ImGui_ImplVulkanH_Window;

namespace ImGuiApp {

    // Configuration structure
    struct Config {
        std::string title = "Novelreader";
        int width = 1920;
        int height = 1080;
        bool enableValidation = true;
        bool enableDocking = true;
        ImVec4 clearColor = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    };

    // Main application class
    class Application {

    public:
        VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
        VkDevice m_Device = VK_NULL_HANDLE;
        uint32_t m_QueueFamily = UINT32_MAX;
        VkQueue m_Queue = VK_NULL_HANDLE;
        VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;
        ImGui_ImplVulkanH_Window* m_MainWindowData = nullptr;

    private:
        // SDL/Vulkan data
        SDL_Window* m_Window = nullptr;
        VkInstance m_Instance = VK_NULL_HANDLE;
        uint32_t m_MinImageCount = 2;
        bool m_SwapChainRebuild = false;
        bool m_Running = true;

        // Configuration
        Config m_Config;

        // User callbacks
        std::function<void()> m_UpdateCallback;
        std::function<void(SDL_Event&)> m_EventCallback;

#ifdef _DEBUG
        VkDebugReportCallbackEXT m_DebugReport = VK_NULL_HANDLE;
#endif

    public:
        Application(const Config& config = Config());
        ~Application();

        // Main functions
        bool Initialize();
        void Run();
        void Shutdown();
        void Close() { m_Running = false; }

        // Callback setters
        void SetUpdateCallback(std::function<void()> callback) { m_UpdateCallback = callback; }
        void SetEventCallback(std::function<void(SDL_Event&)> callback) { m_EventCallback = callback; }

        // Getters
        SDL_Window* GetWindow() const { return m_Window; }
        const Config& GetConfig() const { return m_Config; }

    private:
        // Vulkan setup functions (simplified from ImGui example)
        bool SetupVulkan();
        bool SetupVulkanWindow(int width, int height);
        void CleanupVulkan();
        void CleanupVulkanWindow();

        // Rendering functions
        void FrameRender(ImDrawData* draw_data);
        void FramePresent();

        // UI functions
        void RenderDockSpace();

        // Helper functions
        static void CheckVkResult(VkResult err);
        static bool IsExtensionAvailable(const ImVector<VkExtensionProperties>& properties, const char* extension);

#ifdef _DEBUG
        static VKAPI_ATTR VkBool32 VKAPI_CALL DebugReport(VkDebugReportFlagsEXT flags,
            VkDebugReportObjectTypeEXT objectType, uint64_t object, size_t location,
            int32_t messageCode, const char* pLayerPrefix, const char* pMessage, void* pUserData);
#endif
    };

    // Utility functions
    namespace Utils {
        void SetDarkTheme();
        void SetCustomTabBarStyle();
        void SetLightTheme();
        void CenterNextWindow();
        void HelpMarker(const char* desc);
    }

} // namespace ImGuiApp

// Implementation (include this section only once by defining IMGUI_APP_IMPLEMENTATION)
#ifdef IMGUI_APP_IMPLEMENTATION

#include <stdio.h>
#include <stdlib.h>

namespace ImGuiApp {

    // Static data
    static VkAllocationCallbacks* g_Allocator = nullptr;

    Application::Application(const Config& config) : m_Config(config) {
        m_MainWindowData = new ImGui_ImplVulkanH_Window();
        memset(m_MainWindowData, 0, sizeof(ImGui_ImplVulkanH_Window));
    }

    Application::~Application() {
        Shutdown();
        delete m_MainWindowData;
    }

    bool Application::Initialize() {
        // Setup SDL
        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
            printf("Error: SDL_Init(): %s\n", SDL_GetError());
            return false;
        }

        // Create window with Vulkan graphics context
        float main_scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
        SDL_WindowFlags window_flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY;
        m_Window = SDL_CreateWindow(m_Config.title.c_str(),
            (int)(m_Config.width * main_scale),
            (int)(m_Config.height * main_scale),
            window_flags);
        if (!m_Window) {
            printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
            return false;
        }

        // Setup Vulkan
        if (!SetupVulkan()) return false;

        // Create Window Surface FIRST
        VkSurfaceKHR surface;
        if (SDL_Vulkan_CreateSurface(m_Window, m_Instance, g_Allocator, &surface) == 0) {
            printf("Failed to create Vulkan surface.\n");
            return false;
        }

        // Set the surface before calling SetupVulkanWindow
        m_MainWindowData->Surface = surface;

        int w, h;
        SDL_GetWindowSize(m_Window, &w, &h);
        if (!SetupVulkanWindow(w, h)) return false;

        SDL_SetWindowPosition(m_Window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
        SDL_ShowWindow(m_Window);

        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

        if (m_Config.enableDocking) {
            io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
            io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
        }

        // Setup Dear ImGui style
        ImGui::StyleColorsDark();
        ImGuiStyle& style = ImGui::GetStyle();
        style.ScaleAllSizes(main_scale);
        style.FontScaleDpi = main_scale;

        // Setup Platform/Renderer backends
        ImGui_ImplSDL3_InitForVulkan(m_Window);

        ImGui_ImplVulkan_InitInfo init_info = {};
        init_info.Instance = m_Instance;
        init_info.PhysicalDevice = m_PhysicalDevice;
        init_info.Device = m_Device;
        init_info.QueueFamily = m_QueueFamily;
        init_info.Queue = m_Queue;
        init_info.DescriptorPool = m_DescriptorPool;
        init_info.RenderPass = m_MainWindowData->RenderPass;
        init_info.Subpass = 0;
        init_info.MinImageCount = m_MinImageCount;
        init_info.ImageCount = m_MainWindowData->ImageCount;
        init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
        init_info.Allocator = g_Allocator;
        init_info.CheckVkResultFn = CheckVkResult;
        ImGui_ImplVulkan_Init(&init_info);

        return true;
    }

    void Application::Run() {
        while (m_Running) {
            // Poll events
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                ImGui_ImplSDL3_ProcessEvent(&event);

                if (event.type == SDL_EVENT_QUIT)
                    m_Running = false;
                if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
                    event.window.windowID == SDL_GetWindowID(m_Window))
                    m_Running = false;

                // User event callback
                if (m_EventCallback) {
                    m_EventCallback(event);
                }
            }

            if (SDL_GetWindowFlags(m_Window) & SDL_WINDOW_MINIMIZED) {
                SDL_Delay(10);
                continue;
            }

            // Resize swap chain if needed
            int fb_width, fb_height;
            SDL_GetWindowSize(m_Window, &fb_width, &fb_height);
            if (fb_width > 0 && fb_height > 0 &&
                (m_SwapChainRebuild || m_MainWindowData->Width != fb_width || m_MainWindowData->Height != fb_height)) {
                ImGui_ImplVulkan_SetMinImageCount(m_MinImageCount);
                ImGui_ImplVulkanH_CreateOrResizeWindow(m_Instance, m_PhysicalDevice, m_Device,
                    m_MainWindowData, m_QueueFamily, g_Allocator,
                    fb_width, fb_height, m_MinImageCount);
                m_MainWindowData->FrameIndex = 0;
                m_SwapChainRebuild = false;
            }

            // Start the Dear ImGui frame
            ImGui_ImplVulkan_NewFrame();
            ImGui_ImplSDL3_NewFrame();
            ImGui::NewFrame();

            // Render dockspace if enabled
            if (m_Config.enableDocking) {
                RenderDockSpace();
            }

            // User update callback
            if (m_UpdateCallback) {
                m_UpdateCallback();
            }

            // Rendering
            ImGui::Render();
            ImDrawData* draw_data = ImGui::GetDrawData();
            const bool is_minimized = (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);

            if (!is_minimized) {
                m_MainWindowData->ClearValue.color.float32[0] = m_Config.clearColor.x * m_Config.clearColor.w;
                m_MainWindowData->ClearValue.color.float32[1] = m_Config.clearColor.y * m_Config.clearColor.w;
                m_MainWindowData->ClearValue.color.float32[2] = m_Config.clearColor.z * m_Config.clearColor.w;
                m_MainWindowData->ClearValue.color.float32[3] = m_Config.clearColor.w;

                FrameRender(draw_data);
                FramePresent();
            }

            // Update and render platform windows if enabled
            if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
                ImGui::UpdatePlatformWindows();
                ImGui::RenderPlatformWindowsDefault();
            }
        }
    }

    void Application::Shutdown() {
        if (m_Device) {
            VkResult err = vkDeviceWaitIdle(m_Device);
            CheckVkResult(err);

            ImGui_ImplVulkan_Shutdown();
            ImGui_ImplSDL3_Shutdown();
            ImGui::DestroyContext();

            CleanupVulkanWindow();
            CleanupVulkan();
        }

        if (m_Window) {
            SDL_DestroyWindow(m_Window);
            m_Window = nullptr;
        }

        SDL_Quit();
    }

    bool Application::SetupVulkan() {
        VkResult err;

        // Create Vulkan Instance
        {
            VkInstanceCreateInfo create_info = {};
            create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;

            // Get SDL extensions
            ImVector<const char*> extensions;
            uint32_t sdl_extensions_count = 0;
            const char* const* sdl_extensions = SDL_Vulkan_GetInstanceExtensions(&sdl_extensions_count);
            for (uint32_t i = 0; i < sdl_extensions_count; i++)
                extensions.push_back(sdl_extensions[i]);

            // Enumerate available extensions
            uint32_t properties_count;
            ImVector<VkExtensionProperties> properties;
            vkEnumerateInstanceExtensionProperties(nullptr, &properties_count, nullptr);
            properties.resize(properties_count);
            err = vkEnumerateInstanceExtensionProperties(nullptr, &properties_count, properties.Data);
            CheckVkResult(err);

            // Enable required extensions
            if (IsExtensionAvailable(properties, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME))
                extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

#ifdef VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME
            if (IsExtensionAvailable(properties, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME)) {
                extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
                create_info.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
            }
#endif

#ifdef _DEBUG
            if (m_Config.enableValidation) {
                const char* layers[] = { "VK_LAYER_KHRONOS_validation" };
                create_info.enabledLayerCount = 1;
                create_info.ppEnabledLayerNames = layers;
                extensions.push_back("VK_EXT_debug_report");
            }
#endif

            create_info.enabledExtensionCount = (uint32_t)extensions.Size;
            create_info.ppEnabledExtensionNames = extensions.Data;
            err = vkCreateInstance(&create_info, g_Allocator, &m_Instance);
            CheckVkResult(err);

#ifdef _DEBUG
            if (m_Config.enableValidation) {
                auto f_vkCreateDebugReportCallbackEXT = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(m_Instance, "vkCreateDebugReportCallbackEXT");
                IM_ASSERT(f_vkCreateDebugReportCallbackEXT != nullptr);
                VkDebugReportCallbackCreateInfoEXT debug_report_ci = {};
                debug_report_ci.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
                debug_report_ci.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
                debug_report_ci.pfnCallback = DebugReport;
                debug_report_ci.pUserData = nullptr;
                err = f_vkCreateDebugReportCallbackEXT(m_Instance, &debug_report_ci, g_Allocator, &m_DebugReport);
                CheckVkResult(err);
            }
#endif
        }

        // Select Physical Device (GPU)
        m_PhysicalDevice = ImGui_ImplVulkanH_SelectPhysicalDevice(m_Instance);
        IM_ASSERT(m_PhysicalDevice != VK_NULL_HANDLE);

        // Select graphics queue family
        m_QueueFamily = ImGui_ImplVulkanH_SelectQueueFamilyIndex(m_PhysicalDevice);
        IM_ASSERT(m_QueueFamily != (uint32_t)-1);

        // Create Logical Device
        {
            ImVector<const char*> device_extensions;
            device_extensions.push_back("VK_KHR_swapchain");

            // Enumerate physical device extension
            uint32_t properties_count;
            ImVector<VkExtensionProperties> properties;
            vkEnumerateDeviceExtensionProperties(m_PhysicalDevice, nullptr, &properties_count, nullptr);
            properties.resize(properties_count);
            vkEnumerateDeviceExtensionProperties(m_PhysicalDevice, nullptr, &properties_count, properties.Data);

#ifdef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
            if (IsExtensionAvailable(properties, VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME))
                device_extensions.push_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
#endif

            const float queue_priority[] = { 1.0f };
            VkDeviceQueueCreateInfo queue_info[1] = {};
            queue_info[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queue_info[0].queueFamilyIndex = m_QueueFamily;
            queue_info[0].queueCount = 1;
            queue_info[0].pQueuePriorities = queue_priority;

            VkDeviceCreateInfo create_info = {};
            create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
            create_info.queueCreateInfoCount = sizeof(queue_info) / sizeof(queue_info[0]);
            create_info.pQueueCreateInfos = queue_info;
            create_info.enabledExtensionCount = (uint32_t)device_extensions.Size;
            create_info.ppEnabledExtensionNames = device_extensions.Data;

            err = vkCreateDevice(m_PhysicalDevice, &create_info, g_Allocator, &m_Device);
            CheckVkResult(err);
            vkGetDeviceQueue(m_Device, m_QueueFamily, 0, &m_Queue);
        }

        // Create Descriptor Pool
        {
            VkDescriptorPoolSize pool_sizes[] = {
                { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 }, // Increased from minimum
            };
            VkDescriptorPoolCreateInfo pool_info = {};
            pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
            pool_info.maxSets = 1000; // Set explicit limit
            pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
            pool_info.pPoolSizes = pool_sizes;
            err = vkCreateDescriptorPool(m_Device, &pool_info, g_Allocator, &m_DescriptorPool);
            CheckVkResult(err);
        }

        return true;
    }

    bool Application::SetupVulkanWindow(int width, int height) {
        // Check for WSI support
        VkBool32 res;
        vkGetPhysicalDeviceSurfaceSupportKHR(m_PhysicalDevice, m_QueueFamily, m_MainWindowData->Surface, &res);
        if (res != VK_TRUE) {
            fprintf(stderr, "Error no WSI support on physical device 0\n");
            return false;
        }

        // Select Surface Format
        const VkFormat requestSurfaceImageFormat[] = { VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM };
        const VkColorSpaceKHR requestSurfaceColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
        m_MainWindowData->SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(m_PhysicalDevice, m_MainWindowData->Surface, requestSurfaceImageFormat, (size_t)IM_ARRAYSIZE(requestSurfaceImageFormat), requestSurfaceColorSpace);

        // Select Present Mode
        VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_FIFO_KHR };
        m_MainWindowData->PresentMode = ImGui_ImplVulkanH_SelectPresentMode(m_PhysicalDevice, m_MainWindowData->Surface, &present_modes[0], IM_ARRAYSIZE(present_modes));

        // Create SwapChain, RenderPass, Framebuffer, etc.
        IM_ASSERT(m_MinImageCount >= 2);
        ImGui_ImplVulkanH_CreateOrResizeWindow(m_Instance, m_PhysicalDevice, m_Device, m_MainWindowData, m_QueueFamily, g_Allocator, width, height, m_MinImageCount);

        return true;
    }

    void Application::CleanupVulkan() {
        vkDestroyDescriptorPool(m_Device, m_DescriptorPool, g_Allocator);

#ifdef _DEBUG
        if (m_Config.enableValidation) {
            auto f_vkDestroyDebugReportCallbackEXT = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(m_Instance, "vkDestroyDebugReportCallbackEXT");
            f_vkDestroyDebugReportCallbackEXT(m_Instance, m_DebugReport, g_Allocator);
        }
#endif

        vkDestroyDevice(m_Device, g_Allocator);
        vkDestroyInstance(m_Instance, g_Allocator);
    }

    void Application::CleanupVulkanWindow() {
        ImGui_ImplVulkanH_DestroyWindow(m_Instance, m_Device, m_MainWindowData, g_Allocator);
    }

    void Application::FrameRender(ImDrawData* draw_data) {
        VkSemaphore image_acquired_semaphore = m_MainWindowData->FrameSemaphores[m_MainWindowData->SemaphoreIndex].ImageAcquiredSemaphore;
        VkSemaphore render_complete_semaphore = m_MainWindowData->FrameSemaphores[m_MainWindowData->SemaphoreIndex].RenderCompleteSemaphore;
        VkResult err = vkAcquireNextImageKHR(m_Device, m_MainWindowData->Swapchain, UINT64_MAX, image_acquired_semaphore, VK_NULL_HANDLE, &m_MainWindowData->FrameIndex);
        if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR) {
            m_SwapChainRebuild = true;
            if (err == VK_ERROR_OUT_OF_DATE_KHR)
                return;
        }
        CheckVkResult(err);

        ImGui_ImplVulkanH_Frame* fd = &m_MainWindowData->Frames[m_MainWindowData->FrameIndex];
        {
            err = vkWaitForFences(m_Device, 1, &fd->Fence, VK_TRUE, UINT64_MAX);
            CheckVkResult(err);
            err = vkResetFences(m_Device, 1, &fd->Fence);
            CheckVkResult(err);
        }
        {
            err = vkResetCommandPool(m_Device, fd->CommandPool, 0);
            CheckVkResult(err);
            VkCommandBufferBeginInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            err = vkBeginCommandBuffer(fd->CommandBuffer, &info);
            CheckVkResult(err);
        }
        {
            VkRenderPassBeginInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            info.renderPass = m_MainWindowData->RenderPass;
            info.framebuffer = fd->Framebuffer;
            info.renderArea.extent.width = m_MainWindowData->Width;
            info.renderArea.extent.height = m_MainWindowData->Height;
            info.clearValueCount = 1;
            info.pClearValues = &m_MainWindowData->ClearValue;
            vkCmdBeginRenderPass(fd->CommandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);
        }

        // Record dear imgui primitives into command buffer
        ImGui_ImplVulkan_RenderDrawData(draw_data, fd->CommandBuffer);

        // Submit command buffer
        vkCmdEndRenderPass(fd->CommandBuffer);
        {
            VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            VkSubmitInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            info.waitSemaphoreCount = 1;
            info.pWaitSemaphores = &image_acquired_semaphore;
            info.pWaitDstStageMask = &wait_stage;
            info.commandBufferCount = 1;
            info.pCommandBuffers = &fd->CommandBuffer;
            info.signalSemaphoreCount = 1;
            info.pSignalSemaphores = &render_complete_semaphore;

            err = vkEndCommandBuffer(fd->CommandBuffer);
            CheckVkResult(err);
            err = vkQueueSubmit(m_Queue, 1, &info, fd->Fence);
            CheckVkResult(err);
        }
    }

    void Application::FramePresent() {
        if (m_SwapChainRebuild)
            return;
        VkSemaphore render_complete_semaphore = m_MainWindowData->FrameSemaphores[m_MainWindowData->SemaphoreIndex].RenderCompleteSemaphore;
        VkPresentInfoKHR info = {};
        info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        info.waitSemaphoreCount = 1;
        info.pWaitSemaphores = &render_complete_semaphore;
        info.swapchainCount = 1;
        info.pSwapchains = &m_MainWindowData->Swapchain;
        info.pImageIndices = &m_MainWindowData->FrameIndex;
        VkResult err = vkQueuePresentKHR(m_Queue, &info);
        if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR) {
            m_SwapChainRebuild = true;
            if (err == VK_ERROR_OUT_OF_DATE_KHR)
                return;
        }
        CheckVkResult(err);
        m_MainWindowData->SemaphoreIndex = (m_MainWindowData->SemaphoreIndex + 1) % m_MainWindowData->SemaphoreCount;
    }

    void Application::RenderDockSpace() {
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);

        ImGuiWindowFlags window_flags =  ImGuiWindowFlags_NoDocking;
        window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse;
        window_flags |= ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

        ImGui::Begin("DockSpace", nullptr, window_flags);
        ImGui::PopStyleVar(3);

        ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

        ImGui::End();
    }


    void Application::CheckVkResult(VkResult err) {
        if (err == VK_SUCCESS)
            return;
        fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
        if (err < 0)
            abort();
    }

    bool Application::IsExtensionAvailable(const ImVector<VkExtensionProperties>& properties, const char* extension) {
        for (const VkExtensionProperties& p : properties)
            if (strcmp(p.extensionName, extension) == 0)
                return true;
        return false;
    }

#ifdef _DEBUG
    VKAPI_ATTR VkBool32 VKAPI_CALL Application::DebugReport(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType, uint64_t object, size_t location, int32_t messageCode, const char* pLayerPrefix, const char* pMessage, void* pUserData) {
        (void)flags; (void)object; (void)location; (void)messageCode; (void)pUserData; (void)pLayerPrefix;
        fprintf(stderr, "[vulkan] Debug report from ObjectType: %i\nMessage: %s\n\n", objectType, pMessage);
        return VK_FALSE;
    }
#endif

    // Utility functions
    namespace Utils {
        void SetDarkTheme() {
            ImGuiStyle& style = ImGui::GetStyle();
            ImVec4* colors = style.Colors;

            // Base colors - Deep dark with subtle variations
            ImVec4 bg_very_dark = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
            ImVec4 bg_dark = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
            ImVec4 bg_medium = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
            ImVec4 bg_light = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
            ImVec4 accent = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);  // Nice blue
            ImVec4 accent_hover = ImVec4(0.36f, 0.69f, 1.00f, 1.00f);
            ImVec4 accent_active = ImVec4(0.16f, 0.49f, 0.88f, 1.00f);
            ImVec4 text_primary = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
            ImVec4 text_secondary = ImVec4(0.70f, 0.70f, 0.70f, 1.00f);
            ImVec4 text_disabled = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);

            // Window and background colors
            colors[ImGuiCol_WindowBg] = bg_dark;
            colors[ImGuiCol_ChildBg] = bg_very_dark;
            colors[ImGuiCol_PopupBg] = bg_medium;
            colors[ImGuiCol_MenuBarBg] = bg_dark;

            // Text colors
            colors[ImGuiCol_Text] = text_primary;
            colors[ImGuiCol_TextDisabled] = text_disabled;
            colors[ImGuiCol_TextSelectedBg] = ImVec4(accent.x, accent.y, accent.z, 0.35f);

            // Borders
            colors[ImGuiCol_Border] = ImVec4(0.30f, 0.30f, 0.30f, 0.50f);
            colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

            // Frame backgrounds (inputs, etc.)
            colors[ImGuiCol_FrameBg] = bg_medium;
            colors[ImGuiCol_FrameBgHovered] = bg_light;
            colors[ImGuiCol_FrameBgActive] = ImVec4(accent.x, accent.y, accent.z, 0.20f);

            // Title bar
            colors[ImGuiCol_TitleBg] = bg_very_dark;
            colors[ImGuiCol_TitleBgActive] = bg_dark;
            colors[ImGuiCol_TitleBgCollapsed] = bg_very_dark;

            // Scrollbars
            colors[ImGuiCol_ScrollbarBg] = bg_very_dark;
            colors[ImGuiCol_ScrollbarGrab] = bg_light;
            colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
            colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);

            // Checkmarks and sliders
            colors[ImGuiCol_CheckMark] = accent;
            colors[ImGuiCol_SliderGrab] = accent;
            colors[ImGuiCol_SliderGrabActive] = accent_active;

            // Buttons
            colors[ImGuiCol_Button] = bg_medium;
            colors[ImGuiCol_ButtonHovered] = bg_light;
            colors[ImGuiCol_ButtonActive] = ImVec4(accent.x, accent.y, accent.z, 0.30f);

            // Headers (collapsible sections, selectables)
            colors[ImGuiCol_Header] = bg_medium;
            colors[ImGuiCol_HeaderHovered] = bg_light;
            colors[ImGuiCol_HeaderActive] = ImVec4(accent.x, accent.y, accent.z, 0.25f);

            // Separators
            colors[ImGuiCol_Separator] = ImVec4(0.35f, 0.35f, 0.35f, 0.50f);
            colors[ImGuiCol_SeparatorHovered] = accent_hover;
            colors[ImGuiCol_SeparatorActive] = accent_active;

            // Resize grips
            colors[ImGuiCol_ResizeGrip] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
            colors[ImGuiCol_ResizeGripHovered] = accent_hover;
            colors[ImGuiCol_ResizeGripActive] = accent_active;

            // TABS - This is what you want for clean tab styling!
            colors[ImGuiCol_Tab] = bg_dark;
            colors[ImGuiCol_TabHovered] = bg_light;
            colors[ImGuiCol_TabActive] = bg_medium;
            colors[ImGuiCol_TabUnfocused] = bg_very_dark;
            colors[ImGuiCol_TabUnfocusedActive] = bg_dark;

            // Docking
            colors[ImGuiCol_DockingPreview] = ImVec4(accent.x, accent.y, accent.z, 0.70f);
            colors[ImGuiCol_DockingEmptyBg] = bg_very_dark;

            // Plots
            colors[ImGuiCol_PlotLines] = accent;
            colors[ImGuiCol_PlotLinesHovered] = accent_hover;
            colors[ImGuiCol_PlotHistogram] = accent;
            colors[ImGuiCol_PlotHistogramHovered] = accent_hover;

            // Drag and drop
            colors[ImGuiCol_DragDropTarget] = accent;

            // Navigation
            colors[ImGuiCol_NavHighlight] = accent;
            colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
            colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
            colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.35f);

            // Style settings for modern look
            style.WindowRounding = 6.0f;
            style.ChildRounding = 6.0f;
            style.FrameRounding = 4.0f;
            style.PopupRounding = 4.0f;
            style.ScrollbarRounding = 8.0f;
            style.GrabRounding = 4.0f;
            style.TabRounding = 4.0f;

            style.WindowBorderSize = 1.0f;
            style.ChildBorderSize = 1.0f;
            style.PopupBorderSize = 1.0f;
            style.FrameBorderSize = 0.0f;
            style.TabBorderSize = 0.0f;

            style.WindowPadding = ImVec2(12.0f, 12.0f);
            style.FramePadding = ImVec2(8.0f, 4.0f);
            style.ItemSpacing = ImVec2(8.0f, 6.0f);
            style.ItemInnerSpacing = ImVec2(6.0f, 6.0f);
            style.IndentSpacing = 20.0f;
            style.ScrollbarSize = 16.0f;
            style.GrabMinSize = 12.0f;

            style.WindowTitleAlign = ImVec2(0.0f, 0.5f);
            style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
            style.SelectableTextAlign = ImVec2(0.0f, 0.0f);
        }

        void SetCustomTabBarStyle() {
            ImGuiStyle& style = ImGui::GetStyle();
            ImVec4* colors = style.Colors;

            // Custom tab bar colors for a clean look
            colors[ImGuiCol_Tab] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);                    // Inactive tab
            colors[ImGuiCol_TabHovered] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);             // Hovered tab
            colors[ImGuiCol_TabActive] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);              // Active tab
            colors[ImGuiCol_TabUnfocused] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);           // Unfocused tab
            colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);     // Unfocused active tab

            // Tab styling
            style.TabRounding = 2.0f;                    // Subtle rounding
            style.TabBorderSize = 0.0f;                  // No borders for clean look
        }

        void SetLightTheme() {
            ImGui::StyleColorsLight();
        }

        void CenterNextWindow() {
            ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImVec2 center = ImVec2(viewport->Pos.x + viewport->Size.x * 0.5f, viewport->Pos.y + viewport->Size.y * 0.5f);
            ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        }

    }

} // namespace ImGuiApp

#endif // IMGUI_APP_IMPLEMENTATION