#include "Library.h"
#include "Dependecies/json.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <chrono>
#include <memory>
#include <algorithm>
#include <sstream>
#include "ImGui/imgui_impl_vulkan.h"

#define STB_IMAGE_IMPLEMENTATION
#include "Dependecies/stb_image.h"

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#else
#include <unistd.h>
#endif
#include "Dependecies/FontAwesome.h"

using json = nlohmann::json;

namespace {
    // Constants
    constexpr float CARD_WIDTH = 200.0f;
    constexpr float CARD_HEIGHT = 320.0f;
    constexpr float CARD_SPACING = 15.0f;
    constexpr float COVER_AREA_HEIGHT = 180.0f;
    constexpr float INFO_PANEL_COVER_WIDTH = 200.0f;
    constexpr int CHAPTER_GRID_COLUMNS = 3;

    // Helper function to truncate text
    std::string TruncateText(const std::string& text, size_t maxLength) {
        if (text.length() <= maxLength) return text;
        return text.substr(0, maxLength - 3) + "...";
    }
}

// JSON serialization for Novel structures
void to_json(json& j, const Library::Novel::Progress& p) {
    j = json{
        {"readchapters", p.readchapters},
        {"progresspercentage", p.progresspercentage}
    };
}

void from_json(const json& j, Library::Novel::Progress& p) {
    j.at("readchapters").get_to(p.readchapters);
    j.at("progresspercentage").get_to(p.progresspercentage);
}

void to_json(json& j, const Library::Novel& n) {
    j = json{
        {"name", n.name},
        {"authorname", n.authorname},
        {"coverpath", n.coverpath},
        {"synopsis", n.synopsis},
        {"totalchapters", n.totalchapters},
        {"progress", n.progress}
    };
}

void from_json(const json& j, Library::Novel& n) {
    j.at("name").get_to(n.name);
    j.at("authorname").get_to(n.authorname);
    j.at("coverpath").get_to(n.coverpath);
    j.at("synopsis").get_to(n.synopsis);
    j.at("totalchapters").get_to(n.totalchapters);
    j.at("progress").get_to(n.progress);
}

// Constructor and Destructor
Library::Library(ImGuiApp::Application* application) : app(application) {
    chaptermanager = ChapterManager();
    InitializeUIFonts();
    InitializeDownloadSources();
}

Library::~Library() {
    StopDownloadManager();
    CleanupTextures();
    CleanupTextureSampler();
    CleanupFonts(); // Add this line
}

void Library::OnReadingSettingsChanged() {
    // Don't rebuild fonts immediately - defer until safe
    std::cout << "Reading settings changed - deferring font update" << std::endl;
    // Just mark content for reparsing, don't touch fonts
}

void Library::ProcessPendingFontUpdate() {
    if (!pendingFontUpdate || fontUpdateInProgress.load()) {
        return;
    }

    fontUpdateInProgress = true;
    std::cout << "Processing pending font update..." << std::endl;

    try {
        // Wait for current frame to complete
        ImGuiIO& io = ImGui::GetIO();

        // Clear and rebuild font atlas safely
        io.Fonts->Clear();
        InitializeUIFonts();

        // Force rebuild
        bool buildResult = io.Fonts->Build();
        if (!buildResult) {
            std::cout << "Font atlas rebuild failed!" << std::endl;
        }

        pendingFontUpdate = false;
        std::cout << "Font update completed successfully" << std::endl;

    }
    catch (const std::exception& e) {
        std::cout << "Exception during font update: " << e.what() << std::endl;
        pendingFontUpdate = false;
    }

    fontUpdateInProgress = false;
}

// ============================================================================
// UI Font System
// ============================================================================

void Library::InitializeUIFonts() {
    if (uiFonts.initialized) {
        // If fonts are already initialized but we need to rebuild (e.g., settings changed),
        // clear them first
        CleanupFonts();
    }

    ImGuiIO& io = ImGui::GetIO();

    // Clear existing fonts safely
    io.Fonts->Clear();

    const char* fontPath = FindSystemFont();

    try {
        if (fontPath) {
            LoadFontSizesWithFontAwesome(fontPath);
            std::cout << "UI fonts with FontAwesome loaded from: " << fontPath << std::endl;
        }
        else {
            LoadDefaultFontsWithFontAwesome();
            std::cout << "Using default ImGui fonts with FontAwesome" << std::endl;
        }

        // Build fonts
        bool buildResult = io.Fonts->Build();
        if (!buildResult) {
            std::cout << "ERROR: Failed to build font atlas!" << std::endl;
            io.Fonts->Clear();
            uiFonts.normalFont = io.Fonts->AddFontDefault();
            uiFonts.largeFont = io.Fonts->AddFontDefault();
            uiFonts.smallFont = io.Fonts->AddFontDefault();
            uiFonts.titleFont = io.Fonts->AddFontDefault();
            io.Fonts->Build();
        }
        else {
            std::cout << "Font atlas built successfully" << std::endl;
        }

        // Validate all fonts
        if (!uiFonts.normalFont) uiFonts.normalFont = io.Fonts->AddFontDefault();
        if (!uiFonts.largeFont) uiFonts.largeFont = io.Fonts->AddFontDefault();
        if (!uiFonts.smallFont) uiFonts.smallFont = io.Fonts->AddFontDefault();
        if (!uiFonts.titleFont) uiFonts.titleFont = io.Fonts->AddFontDefault();

    }
    catch (const std::exception& e) {
        std::cout << "Exception in font initialization: " << e.what() << std::endl;
        io.Fonts->Clear();
        uiFonts.normalFont = io.Fonts->AddFontDefault();
        uiFonts.largeFont = io.Fonts->AddFontDefault();
        uiFonts.smallFont = io.Fonts->AddFontDefault();
        uiFonts.titleFont = io.Fonts->AddFontDefault();
        io.Fonts->Build();
    }

    uiFonts.initialized = true;
}

void Library::CleanupFonts() {
    if (uiFonts.initialized) {
        // Don't clear the font atlas here - let ImGui handle it
        // Just reset our pointers
        uiFonts.normalFont = nullptr;
        uiFonts.largeFont = nullptr;
        uiFonts.smallFont = nullptr;
        uiFonts.titleFont = nullptr;
        uiFonts.initialized = false;
        std::cout << "UI fonts cleanup completed" << std::endl;
    }
}

void Library::LoadFontSizesWithFontAwesome(const char* path) {
    LoadUIFontWithFontAwesome(path, 18.0f, &uiFonts.smallFont);   // Increased from 14.0f
    LoadUIFontWithFontAwesome(path, 20.0f, &uiFonts.normalFont); // Increased from 16.0f
    LoadUIFontWithFontAwesome(path, 22.0f, &uiFonts.largeFont);  // Increased from 20.0f
    LoadUIFontWithFontAwesome(path, 24.0f, &uiFonts.titleFont);  // Increased from 24.0f
}

void Library::LoadDefaultFontsWithFontAwesome() {
    LoadUIFontWithFontAwesome(nullptr, 18.0f, &uiFonts.smallFont);
    LoadUIFontWithFontAwesome(nullptr, 20.0f, &uiFonts.normalFont);
    LoadUIFontWithFontAwesome(nullptr, 22.0f, &uiFonts.largeFont);
    LoadUIFontWithFontAwesome(nullptr, 24.0f, &uiFonts.titleFont);
}

void Library::LoadUIFontWithFontAwesome(const char* path, float size, ImFont** target) {
    ImGuiIO& io = ImGui::GetIO();

    // Ensure font size is valid
    if (size <= 0.0f || size > 72.0f) {
        std::cout << "Invalid font size: " << size << ", using default 16.0f" << std::endl;
        size = 16.0f;
    }

    try {
        // Load main font first
        ImFontConfig mainConfig;
        mainConfig.SizePixels = size;
        mainConfig.OversampleH = 2;
        mainConfig.OversampleV = 2;
        mainConfig.PixelSnapH = true;
        mainConfig.MergeMode = false;

        // Copy font data to avoid issues with file lifetime
        if (path && std::filesystem::exists(path)) {
            mainConfig.FontDataOwnedByAtlas = true; // Let ImGui manage the data
            *target = io.Fonts->AddFontFromFileTTF(path, size, &mainConfig);
        }
        else {
            *target = io.Fonts->AddFontDefault(&mainConfig);
        }

        // Ensure we have a valid font before proceeding
        if (!*target) {
            std::cout << "Failed to load main font, using default" << std::endl;
            *target = io.Fonts->AddFontDefault();
        }

        // Now merge FontAwesome safely
        std::string fontAwesomePath = "fonts/fa-solid-900.ttf";
        if (std::filesystem::exists(fontAwesomePath)) {
            ImFontConfig faConfig;
            faConfig.MergeMode = true;
            faConfig.PixelSnapH = true;
            faConfig.GlyphMinAdvanceX = size * 0.8f;
            faConfig.FontDataOwnedByAtlas = true;

            // Use the proper icon ranges
            static const ImWchar icon_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };

            ImFont* faFont = io.Fonts->AddFontFromFileTTF(
                fontAwesomePath.c_str(),
                size * 0.9f, // Slightly smaller for icons
                &faConfig,
                icon_ranges
            );

            if (faFont) {
                std::cout << "FontAwesome merged successfully at size " << size << std::endl;
            }
            else {
                std::cout << "Failed to merge FontAwesome at size " << size << std::endl;
            }
        }

    }
    catch (const std::exception& e) {
        std::cout << "Exception loading font: " << e.what() << std::endl;
        if (!*target) {
            *target = io.Fonts->AddFontDefault();
        }
    }
}

void Library::ReinitializeFonts() {
    std::cout << "Reinitializing fonts due to settings change..." << std::endl;

    // Clear the entire font atlas safely
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();

    // Reinitialize fonts
    InitializeUIFonts();

    // Rebuild the font atlas
    io.Fonts->Build();

    std::cout << "Font reinitialization completed" << std::endl;
}

// Function to merge FontAwesome into the current font
void Library::MergeFontAwesome(float size) {
    ImGuiIO& io = ImGui::GetIO();

    // Check if FontAwesome font exists
    std::string fontAwesomePath = "fonts/fa-solid-900.ttf";
    if (!std::filesystem::exists(fontAwesomePath)) {
        std::cout << "FontAwesome font not found at: " << fontAwesomePath << std::endl;
        return;
    }

    try {
        // Configuration for FontAwesome
        ImFontConfig faConfig;
        faConfig.MergeMode = true;
        faConfig.PixelSnapH = true;
        faConfig.GlyphMinAdvanceX = size * 0.8f;
        faConfig.GlyphOffset = ImVec2(0, 1);

        // FontAwesome 6 glyph ranges
        static const ImWchar fa_ranges[] = {
            0xf000, 0xf3ff, // FontAwesome 6 solid icons
            0,
        };

        // Add FontAwesome font merged with the previous font
        ImFont* faFont = io.Fonts->AddFontFromFileTTF(
            fontAwesomePath.c_str(),
            size * 0.9f,
            &faConfig,
            fa_ranges
        );

        if (!faFont) {
            std::cout << "Failed to load FontAwesome font: " << fontAwesomePath << std::endl;
        }
        else {
            std::cout << "FontAwesome merged successfully at size " << size << std::endl;
        }
    }
    catch (const std::exception& e) {
        std::cout << "Exception merging FontAwesome: " << e.what() << std::endl;
    }
}

const char* Library::FindSystemFont() {
    // Try custom font first
    if (std::filesystem::exists("fonts/UI-Regular.ttf")) {
        return "fonts/UI-Regular.ttf";
    }

#ifdef _WIN32
    if (std::filesystem::exists("C:/Windows/Fonts/segoeui.ttf")) {
        return "C:/Windows/Fonts/segoeui.ttf";
    }
    if (std::filesystem::exists("C:/Windows/Fonts/arial.ttf")) {
        return "C:/Windows/Fonts/arial.ttf";
    }
#elif __linux__
    if (std::filesystem::exists("/usr/share/fonts/truetype/ubuntu/Ubuntu-R.ttf")) {
        return "/usr/share/fonts/truetype/ubuntu/Ubuntu-R.ttf";
    }
    if (std::filesystem::exists("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf")) {
        return "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";
    }
#endif
    return nullptr;
}

void Library::LoadFontSizes(const char* path) {
    LoadUIFont(path, 14.0f, &uiFonts.smallFont);
    LoadUIFont(path, 16.0f, &uiFonts.normalFont);
    LoadUIFont(path, 20.0f, &uiFonts.largeFont);
    LoadUIFont(path, 24.0f, &uiFonts.titleFont);
}

void Library::LoadDefaultFonts() {
    ImGuiIO& io = ImGui::GetIO();
    uiFonts.smallFont = io.Fonts->AddFontDefault();
    uiFonts.normalFont = io.Fonts->AddFontDefault();
    uiFonts.largeFont = io.Fonts->AddFontDefault();
    uiFonts.titleFont = io.Fonts->AddFontDefault();
}

void Library::LoadUIFont(const char* path, float size, ImFont** target) {
    ImGuiIO& io = ImGui::GetIO();

    ImFontConfig fontConfig;
    fontConfig.SizePixels = size;
    fontConfig.OversampleH = 2;
    fontConfig.OversampleV = 2;
    fontConfig.PixelSnapH = true;

    *target = io.Fonts->AddFontFromFileTTF(path, size, &fontConfig);
    if (!*target) {
        *target = io.Fonts->AddFontDefault();
        std::cout << "Failed to load UI font, using default" << std::endl;
    }
}

// ============================================================================
// Vulkan Helper Functions
// ============================================================================

uint32_t Library::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(app->m_PhysicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("Failed to find suitable memory type!");
}

void Library::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties, VkBuffer& buffer,
    VkDeviceMemory& bufferMemory) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(app->m_Device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer!");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(app->m_Device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(app->m_Device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate buffer memory!");
    }

    vkBindBufferMemory(app->m_Device, buffer, bufferMemory, 0);
}

VkCommandBuffer Library::CreateOneTimeCommandBuffer() {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = app->m_MainWindowData->Frames[0].CommandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    if (vkAllocateCommandBuffers(app->m_Device, &allocInfo, &commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffer!");
    }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("Failed to begin command buffer!");
    }

    return commandBuffer;
}

void Library::SubmitOneTimeCommandBuffer(VkCommandBuffer commandBuffer) {
    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to end command buffer!");
    }

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    if (vkQueueSubmit(app->m_Queue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit command buffer!");
    }

    vkQueueWaitIdle(app->m_Queue);
    vkFreeCommandBuffers(app->m_Device, app->m_MainWindowData->Frames[0].CommandPool, 1, &commandBuffer);
}

// ============================================================================
// Texture Management
// ============================================================================

VkSampler Library::GetOrCreateTextureSampler() {
    if (textureSampler != VK_NULL_HANDLE) {
        return textureSampler;
    }

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;

    VkResult result = vkCreateSampler(app->m_Device, &samplerInfo, nullptr, &textureSampler);
    if (result != VK_SUCCESS) {
        std::cout << "Failed to create texture sampler! VkResult: " << result << std::endl;
        return VK_NULL_HANDLE;
    }

    return textureSampler;
}

void Library::CleanupTextureSampler() {
    if (textureSampler != VK_NULL_HANDLE) {
        vkDestroySampler(app->m_Device, textureSampler, nullptr);
        textureSampler = VK_NULL_HANDLE;
    }
}

void Library::CleanupTextures() {
    for (auto& [path, texture] : coverTextures) {
        CleanupCoverTexture(texture);
    }
    coverTextures.clear();
}

void Library::CleanupCoverTexture(CoverTexture& texture) {
    if (texture.imageView != VK_NULL_HANDLE) {
        vkDestroyImageView(app->m_Device, texture.imageView, nullptr);
        texture.imageView = VK_NULL_HANDLE;
    }
    if (texture.image != VK_NULL_HANDLE) {
        vkDestroyImage(app->m_Device, texture.image, nullptr);
        texture.image = VK_NULL_HANDLE;
    }
    if (texture.imageMemory != VK_NULL_HANDLE) {
        vkFreeMemory(app->m_Device, texture.imageMemory, nullptr);
        texture.imageMemory = VK_NULL_HANDLE;
    }
}

VkDescriptorSet Library::LoadCoverTexture(const std::string& imagePath) {
    // Check if texture is already loaded
    auto it = coverTextures.find(imagePath);
    if (it != coverTextures.end() && it->second.loaded) {
        return it->second.descriptorSet;
    }

    // Load image using stb_image
    int width, height, channels;
    stbi_uc* pixels = stbi_load(imagePath.c_str(), &width, &height, &channels, STBI_rgb_alpha);

    if (!pixels) {
        std::cout << "Failed to load image: " << imagePath << std::endl;
        coverTextures[imagePath] = CoverTexture();
        return VK_NULL_HANDLE;
    }

    VkDescriptorSet result = CreateTextureFromPixels(pixels, width, height, imagePath);
    stbi_image_free(pixels);

    return result;
}

VkDescriptorSet Library::CreateTextureFromPixels(stbi_uc* pixels, int width, int height,
    const std::string& imagePath) {
    VkDeviceSize imageSize = width * height * 4;

    try {
        // Create staging buffer
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        CreateStagingBuffer(imageSize, pixels, stagingBuffer, stagingBufferMemory);

        // Create Vulkan image
        CoverTexture texture = CreateVulkanImage(width, height);

        // Copy data and transition layouts
        CopyImageData(stagingBuffer, texture, width, height);

        // Create image view and descriptor set
        CreateImageView(texture);
        CreateDescriptorSet(texture);

        // Cleanup staging buffer
        vkDestroyBuffer(app->m_Device, stagingBuffer, nullptr);
        vkFreeMemory(app->m_Device, stagingBufferMemory, nullptr);

        texture.loaded = true;
        coverTextures[imagePath] = texture;

        return texture.descriptorSet;

    }
    catch (const std::exception& e) {
        std::cout << "Failed to create texture: " << e.what() << std::endl;
        return VK_NULL_HANDLE;
    }
}

// ============================================================================
// File Management
// ============================================================================

void Library::CheckNovelsDirectory() {
    std::filesystem::path novelsDir = "Novels";
    if (!std::filesystem::exists(novelsDir)) {
        std::filesystem::create_directory(novelsDir);
    }
}

void Library::CheckNovelFolderStructure(const std::string& novelName) {
    CheckNovelsDirectory();

    std::filesystem::path novelDir = "Novels/" + novelName;
    if (!std::filesystem::exists(novelDir)) {
        std::filesystem::create_directories(novelDir);
    }

    std::filesystem::path chaptersDir = novelDir / "chapters";
    if (!std::filesystem::exists(chaptersDir)) {
        std::filesystem::create_directories(chaptersDir);
    }
}

int Library::CountChaptersInDirectory(const std::string& novelName) {
    std::string chaptersDir = "Novels/" + novelName + "/chapters";
    if (!std::filesystem::exists(chaptersDir)) {
        return 0;
    }

    int count = 0;
    for (const auto& entry : std::filesystem::directory_iterator(chaptersDir)) {
        if (entry.path().extension() == ".json") {
            count++;
        }
    }
    return count;
}

// ============================================================================
// Core Library Functions
// ============================================================================

void Library::SwitchToLibrary() {
    currentState = UIState::LIBRARY;
    std::cout << "Switched to Library view" << std::endl;
}

void Library::SwitchToReading(const std::string& novelName, int chapter) {
    currentState = UIState::READING;
    currentNovelName = novelName;
    targetChapter = chapter;

    chaptermanager.LoadChaptersFromDirectory(novelName);
    chaptermanager.OpenChapter(chapter);
    chaptermanager.SetNovelTitle(novelName);

    // Add this line to connect ChapterManager to Library
    chaptermanager.SetLibraryPointer(this);

    std::cout << "Switched to Reading view: " << novelName << " Chapter " << chapter << std::endl;
}

void Library::RefreshNovelChapterCounts() {
    for (auto& novel : novellist) {
        int chapterCount = CountChaptersInDirectory(novel.name);
        if (chapterCount > novel.totalchapters) {
            novel.totalchapters = chapterCount;
            std::cout << "Updated " << novel.name << " chapter count to " << chapterCount << std::endl;
        }
    }
    SaveNovels(novellist);
}

void Library::LoadAllNovelsFromFile() {
    try {
        std::ifstream file("Novels/Novels.json");
        if (!file.is_open()) {
            std::cout << "No existing novels file found" << std::endl;
            return;
        }

        json j;
        file >> j;
        file.close();

        if (j.contains("novels")) {
            novellist = j["novels"].get<std::vector<Novel>>();

            // Update downloaded chapter counts for all novels
            for (auto& novel : novellist) {
                novel.downloadedchapters = CountChaptersInDirectory(novel.name);

                // If downloadedchapters wasn't in the JSON, initialize it
                if (novel.downloadedchapters == 0 && novel.totalchapters > 0) {
                    novel.downloadedchapters = novel.totalchapters; // Assume all are downloaded for existing novels
                }
            }

            std::cout << "Successfully loaded " << novellist.size() << " novels" << std::endl;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Error loading novels: " << e.what() << std::endl;
        novellist.clear();
    }
}

void Library::UpdateReadingProgress(const std::string& novelName, int chapterNumber) {
    for (auto& novel : novellist) {
        if (novel.name == novelName) {
            if (chapterNumber > novel.progress.readchapters) {
                novel.progress.readchapters = chapterNumber;
                // Calculate percentage based on downloaded chapters, not total
                novel.progress.progresspercentage =
                    (static_cast<float>(novel.progress.readchapters) / static_cast<float>(novel.downloadedchapters)) * 100.0f;
                SaveNovels(novellist);
                std::cout << "Updated reading progress for " << novelName << " to chapter " << chapterNumber << std::endl;
            }
            break;
        }
    }
}

bool Library::SaveNovels(const std::vector<Novel>& novels) {
    try {
        CheckNovelsDirectory();

        for (const auto& novel : novels) {
            CheckNovelFolderStructure(novel.name);
        }

        json j;
        j["novels"] = novels;

        std::ofstream file("Novels/Novels.json");
        if (!file.is_open()) {
            std::cerr << "Error: Could not open file for writing" << std::endl;
            return false;
        }

        file << j.dump(4);
        file.close();

        std::cout << "Successfully saved " << novels.size() << " novels" << std::endl;
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "Error saving novels: " << e.what() << std::endl;
        return false;
    }
}

bool Library::AddNovel(const Novel& novel) {
    try {
        CheckNovelFolderStructure(novel.name);

        // Check for duplicates
        for (const auto& existingNovel : novellist) {
            if (existingNovel.name == novel.name && existingNovel.authorname == novel.authorname) {
                std::cout << "Novel '" << novel.name << "' by " << novel.authorname
                    << " already exists. Skipping save." << std::endl;
                return false;
            }
        }

        Novel novelToSave = novel;
        std::string expectedCoverPath = "Novels/" + novel.name + "/cover.jpg";
        if (novelToSave.coverpath != expectedCoverPath) {
            novelToSave.coverpath = expectedCoverPath;
        }

        novellist.push_back(novelToSave);
        return SaveNovels(novellist);

    }
    catch (const std::exception& e) {
        std::cerr << "Error saving single novel: " << e.what() << std::endl;
        return false;
    }
}

bool Library::RemoveNovel(const std::string& novelName, const std::string& authorName) {
    try {
        auto it = std::remove_if(novellist.begin(), novellist.end(),
            [&](const Novel& novel) {
                return novel.name == novelName && novel.authorname == authorName;
            });

        if (it != novellist.end()) {
            novellist.erase(it, novellist.end());

            if (SaveNovels(novellist)) {
                std::cout << "Successfully removed novel '" << novelName
                    << "' by " << authorName << std::endl;

                // Remove novel folder
                std::filesystem::path novelDir = "Novels/" + novelName;
                if (std::filesystem::exists(novelDir)) {
                    std::filesystem::remove_all(novelDir);
                    std::cout << "Removed novel folder: " << novelDir << std::endl;
                }
                return true;
            }
        }
        else {
            std::cout << "Novel '" << novelName << "' by " << authorName
                << " not found." << std::endl;
        }
        return false;
    }
    catch (const std::exception& e) {
        std::cerr << "Error removing novel: " << e.what() << std::endl;
        return false;
    }
}

// ============================================================================
// UI Rendering Functions
// ============================================================================

void Library::Render() {
    // Check if fonts need reinitialization
    if (!uiFonts.initialized) {
        InitializeUIFonts();
    }

    // Rest of render code - NO font rebuilding here
    switch (currentState) {
    case UIState::LIBRARY:
        RenderLibraryInterface();
        break;
    case UIState::READING:
        RenderFullScreenReading();
        break;
    }
}

void Library::RenderLibraryInterface() {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;
    if (ImGui::Begin("MainWindow", nullptr, flags)) {
        RenderMainTabs();
    }
    ImGui::End();
}

void Library::RenderMainTabs() {
    ImGui::PushStyleVar(ImGuiStyleVar_TabRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(20, 12));

    if (uiFonts.largeFont) ImGui::PushFont(uiFonts.largeFont);

    if (ImGui::BeginTabBar("MainTabs", ImGuiTabBarFlags_FittingPolicyScroll)) {
        if (ImGui::BeginTabItem(ICON_FA_BOOK " Library")) {
            currentLibraryTab = 0;
            RestoreUIState();
            RenderLibraryView();
            PrepareUIState();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem(ICON_FA_DOWNLOAD " Downloads")) {
            currentLibraryTab = 1;
            RestoreUIState();
            RenderDownloadManager();
            PrepareUIState();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    if (uiFonts.largeFont) ImGui::PopFont();
    ImGui::PopStyleVar(2);
}

void Library::PrepareUIState() {
    if (uiFonts.largeFont) ImGui::PushFont(uiFonts.largeFont);
    ImGui::PushStyleVar(ImGuiStyleVar_TabRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(20, 12));
}

void Library::RestoreUIState() {
    if (uiFonts.largeFont) ImGui::PopFont();
    ImGui::PopStyleVar(2);
}

void Library::RenderLibraryView() {
    ImVec2 windowSize = ImGui::GetContentRegionAvail();

    if (showInfoPanel) {
        RenderSplitView(windowSize);
    }
    else {
        RenderNovelGrid();
    }
}

void Library::RenderSplitView(const ImVec2& windowSize) {
    float libraryWidth = windowSize.x * 0.6f - 5.0f;
    float infoPanelWidth = windowSize.x * 0.4f - 5.0f;

    ImGui::BeginChild("LibrarySection", ImVec2(libraryWidth, 0), false);
    RenderNovelGrid();
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::Dummy(ImVec2(10, 0));
    ImGui::SameLine();

    ImGui::BeginChild("InfoSection", ImVec2(infoPanelWidth, 0), false);
    RenderInfoPanel();
    ImGui::EndChild();
}

void Library::RenderNovelGrid() {
    RenderGridHeader();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (showGrid) {
        RenderNovelGridView();
    }
    else {
        RenderNovelListView();
    }
}

void Library::RenderGridHeader() {
    ImVec2 availableSize = ImGui::GetContentRegionAvail();

    ImGui::BeginGroup();
    {
        if (uiFonts.titleFont && uiFonts.initialized) {
            ImGui::PushFont(uiFonts.titleFont);
        }
        // Use the proper FontAwesome constants from the header
        ImGui::Spacing();
        ImGui::Text("%s Novel Library (%zu novels)", ICON_FA_BOOK, novellist.size());
        if (uiFonts.titleFont && uiFonts.initialized) {
            ImGui::PopFont();
        }

        ImGui::SameLine();

        float toggleStart = availableSize.x - 200;
        ImGui::SetCursorPosX(toggleStart);

        const char* viewIcon = showGrid ? ICON_FA_LIST : ICON_FA_THERMOMETER;
        const char* viewText = showGrid ? " List View" : " Grid View";
        std::string buttonText = std::string(viewIcon) + viewText;

        if (ImGui::Button(buttonText.c_str(), ImVec2(90, 0))) {
            showGrid = !showGrid;
        }

        ImGui::SameLine();
        std::string refreshText = std::string(ICON_FA_RECYCLE) + " Refresh";
        if (ImGui::Button(refreshText.c_str(), ImVec2(80, 0))) {
            RefreshNovelChapterCounts();
            LoadAllNovelsFromFile();
        }
    }
    ImGui::EndGroup();
}

void Library::RenderNovelGridView() {
    ImVec2 availableSize = ImGui::GetContentRegionAvail();
    int columns = CalculateGridColumns(availableSize.x);

    ImGui::BeginChild("NovelGrid", ImVec2(0, 0), false);

    // Use traditional ImGui layout instead of SetCursorPos
    for (size_t i = 0; i < novellist.size(); i++) {
        // Start new row if needed
        if (i > 0 && i % columns == 0) {
            ImGui::Spacing();
        }

        // Use SameLine for columns (except first in row)
        if (i % columns != 0) {
            ImGui::SameLine(0, CARD_SPACING);
        }

        // Create a child window for consistent card sizing
        ImGui::PushID(static_cast<int>(i));

        // Begin child with exact card dimensions
        if (ImGui::BeginChild("NovelCard", ImVec2(CARD_WIDTH, CARD_HEIGHT), false,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground)) {

            // Get the screen position for this child window
            ImVec2 cardStart = ImGui::GetCursorScreenPos();
            ImVec2 cardEnd = ImVec2(cardStart.x + CARD_WIDTH, cardStart.y + CARD_HEIGHT);

            bool isSelected = (selectedNovelIndex == static_cast<int>(i));

            // Render card background
            RenderCardBackground(cardStart, cardEnd, isSelected);

            // Create invisible button for interaction
            ImGui::InvisibleButton("CardInteraction", ImVec2(CARD_WIDTH, CARD_HEIGHT));
            if (ImGui::IsItemClicked()) {
                selectedNovelIndex = static_cast<int>(i);
                showInfoPanel = true;
            }

            // Render card content
            RenderCardContent(novellist[i], cardStart, isSelected);

            // Add hover effect
            if (ImGui::IsItemHovered()) {
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                drawList->AddRect(cardStart, cardEnd, IM_COL32(150, 170, 200, 200), 8.0f, 0, 2.0f);
            }
        }
        ImGui::EndChild();
        ImGui::PopID();
    }

    ImGui::EndChild();
}

int Library::CalculateGridColumns(float availableWidth) {
    int columns = static_cast<int>((availableWidth + CARD_SPACING) / (CARD_WIDTH + CARD_SPACING));
    return std::max(1, columns);
}

void Library::RenderNovelCard(const Novel& novel, int index) {
    ImGui::PushID(index);

    // Get current position (set by grid layout)
    ImVec2 cardStart = ImGui::GetCursorScreenPos();
    ImVec2 cardEnd = ImVec2(cardStart.x + CARD_WIDTH, cardStart.y + CARD_HEIGHT);

    bool isSelected = (selectedNovelIndex == index);
    RenderCardBackground(cardStart, cardEnd, isSelected);

    // Create invisible button for the entire card area
    ImGui::InvisibleButton("CardButton", ImVec2(CARD_WIDTH, CARD_HEIGHT));
    if (ImGui::IsItemClicked()) {
        selectedNovelIndex = index;
        showInfoPanel = true;
    }

    // Save cursor position before rendering content
    ImVec2 originalPos = ImGui::GetCursorPos();

    // Render content without affecting cursor
    RenderCardContent(novel, cardStart, isSelected);

    // Restore position for grid layout
    ImGui::SetCursorPos(originalPos);

    // Add hover effect
    if (ImGui::IsItemHovered()) {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRect(cardStart, cardEnd, IM_COL32(150, 170, 200, 200), 8.0f, 0, 2.0f);
    }

    ImGui::PopID();
}

void Library::RenderCardBackground(const ImVec2& start, const ImVec2& end, bool isSelected) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    ImU32 cardColor = isSelected ? IM_COL32(60, 80, 120, 255) : IM_COL32(35, 35, 40, 255);
    ImU32 borderColor = isSelected ? IM_COL32(100, 140, 200, 255) : IM_COL32(60, 60, 70, 255);

    drawList->AddRectFilled(start, end, cardColor, 8.0f);
    drawList->AddRect(start, end, borderColor, 8.0f, 0, isSelected ? 3.0f : 1.0f);
}

void Library::RenderCardContent(const Novel& novel, const ImVec2& cardStart, bool isSelected) {
    // Use ImDrawList for all rendering to avoid cursor position issues
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Render cover area
    ImVec2 coverStart = ImVec2(cardStart.x + 10, cardStart.y + 10);
    ImVec2 coverEnd = ImVec2(coverStart.x + CARD_WIDTH - 20, coverStart.y + COVER_AREA_HEIGHT);

    VkDescriptorSet coverTexture = GetCoverTexture(novel.coverpath);
    if (coverTexture != VK_NULL_HANDLE && coverTextures.find(novel.coverpath) != coverTextures.end()) {
        CoverTexture& texture = coverTextures[novel.coverpath];

        // Calculate image dimensions
        float aspectRatio = static_cast<float>(texture.width) / static_cast<float>(texture.height);
        float displayHeight = COVER_AREA_HEIGHT;
        float displayWidth = displayHeight * aspectRatio;
        float coverAreaWidth = CARD_WIDTH - 20;

        if (displayWidth > coverAreaWidth) {
            displayWidth = coverAreaWidth;
            displayHeight = displayWidth / aspectRatio;
        }

        float centerX = (coverAreaWidth - displayWidth) * 0.5f;
        float centerY = (COVER_AREA_HEIGHT - displayHeight) * 0.5f;

        ImVec2 imageStart = ImVec2(coverStart.x + centerX, coverStart.y + centerY);
        ImVec2 imageEnd = ImVec2(imageStart.x + displayWidth, imageStart.y + displayHeight);

        drawList->AddImage(reinterpret_cast<ImTextureID>(coverTexture), imageStart, imageEnd);
    }
    else {
        // Render placeholder
        drawList->AddRectFilled(coverStart, coverEnd, IM_COL32(40, 40, 45, 255), 4.0f);
        drawList->AddRect(coverStart, coverEnd, IM_COL32(80, 80, 90, 255), 4.0f);

        // Add placeholder text
        ImVec2 textPos = ImVec2(coverStart.x + (CARD_WIDTH - 20) * 0.5f - 30,
            coverStart.y + COVER_AREA_HEIGHT * 0.5f - 10);
        drawList->AddText(textPos, IM_COL32(128, 128, 128, 255), "No Cover");
        drawList->AddText(ImVec2(textPos.x, textPos.y + 20), IM_COL32(128, 128, 128, 255), "Available");
    }

    // Render text info
    ImVec2 infoStart = ImVec2(cardStart.x + 10, cardStart.y + COVER_AREA_HEIGHT + 30);

    // Title
    std::string truncatedTitle = TruncateText(novel.name, 25);
    drawList->AddText(infoStart, IM_COL32(230, 240, 255, 255), truncatedTitle.c_str());

    // Author
    std::string truncatedAuthor = TruncateText(novel.authorname, 20);
    std::string authorText = "Author: " + truncatedAuthor;
    drawList->AddText(ImVec2(infoStart.x, infoStart.y + 18), IM_COL32(204, 204, 153, 255), authorText.c_str());

    // Chapters
    std::string chapterText = std::to_string(novel.downloadedchapters) + "/" + std::to_string(novel.totalchapters) + " chapters";
    drawList->AddText(ImVec2(infoStart.x, infoStart.y + 36), IM_COL32(180, 180, 180, 255), chapterText.c_str());

    // Progress bar
    ImVec2 progressStart = ImVec2(infoStart.x, infoStart.y + 54);
    ImVec2 progressEnd = ImVec2(progressStart.x + CARD_WIDTH - 20, progressStart.y + 15);

    // Background
    drawList->AddRectFilled(progressStart, progressEnd, IM_COL32(60, 60, 60, 255), 2.0f);

    // Progress fill
    float progress = novel.progress.progresspercentage / 100.0f;
    ImVec2 fillEnd = ImVec2(progressStart.x + ((CARD_WIDTH - 20) * progress), progressEnd.y);
    drawList->AddRectFilled(progressStart, fillEnd, IM_COL32(51, 179, 76, 255), 2.0f);

    // Progress text
    std::string progressText = std::to_string(static_cast<int>(novel.progress.progresspercentage)) + "% complete";
    drawList->AddText(ImVec2(infoStart.x, infoStart.y + 72), IM_COL32(180, 180, 180, 255), progressText.c_str());
}

void Library::RenderCardCover(const Novel& novel, const ImVec2& cardStart) {
    VkDescriptorSet coverTexture = GetCoverTexture(novel.coverpath);
    float coverAreaWidth = CARD_WIDTH - 20;

    if (coverTexture != VK_NULL_HANDLE) {
        RenderValidCoverImage(coverTexture, novel.coverpath, cardStart, coverAreaWidth);
    }
    else {
        RenderPlaceholderCover(cardStart);
    }
}

VkDescriptorSet Library::GetCoverTexture(const std::string& coverPath) {
    auto textureIt = coverTextures.find(coverPath);
    if (textureIt != coverTextures.end() && textureIt->second.loaded) {
        return textureIt->second.descriptorSet;
    }

    if (textureIt == coverTextures.end()) {
        return LoadCoverTexture(coverPath);
    }

    return VK_NULL_HANDLE;
}

void Library::RenderValidCoverImage(VkDescriptorSet texture, const std::string& coverPath,
    const ImVec2& cardStart, float coverAreaWidth) {
    if (coverTextures.find(coverPath) == coverTextures.end()) return;

    CoverTexture& coverTexture = coverTextures[coverPath];

    float aspectRatio = static_cast<float>(coverTexture.width) / static_cast<float>(coverTexture.height);
    float displayHeight = COVER_AREA_HEIGHT;
    float displayWidth = displayHeight * aspectRatio;

    if (displayWidth > coverAreaWidth) {
        displayWidth = coverAreaWidth;
        displayHeight = displayWidth / aspectRatio;
    }

    float centerX = (coverAreaWidth - displayWidth) * 0.5f;
    float centerY = (COVER_AREA_HEIGHT - displayHeight) * 0.5f;

    // Use screen coordinates with ImDrawList for stable positioning
    ImVec2 imagePos = ImVec2(cardStart.x + 10 + centerX, cardStart.y + 10 + centerY);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddImage(
        reinterpret_cast<ImTextureID>(texture),
        imagePos,
        ImVec2(imagePos.x + displayWidth, imagePos.y + displayHeight)
    );
}

void Library::RenderPlaceholderCover(const ImVec2& cardStart) {
    float placeholderWidth = CARD_WIDTH - 20;
    float placeholderHeight = COVER_AREA_HEIGHT;

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 placeholderStart = ImVec2(cardStart.x + 10, cardStart.y + 10);
    ImVec2 placeholderEnd = ImVec2(placeholderStart.x + placeholderWidth, placeholderStart.y + placeholderHeight);

    // Draw placeholder background
    drawList->AddRectFilled(placeholderStart, placeholderEnd, IM_COL32(40, 40, 45, 255), 4.0f);
    drawList->AddRect(placeholderStart, placeholderEnd, IM_COL32(80, 80, 90, 255), 4.0f);

    // Add placeholder text using ImDrawList to avoid cursor issues
    ImU32 textColor = IM_COL32(128, 128, 128, 255);
    ImVec2 centerPos = ImVec2(
        placeholderStart.x + placeholderWidth * 0.5f - 30,
        placeholderStart.y + placeholderHeight * 0.5f - 10
    );

    drawList->AddText(centerPos, textColor, "No Cover");
    drawList->AddText(ImVec2(centerPos.x, centerPos.y + 20), textColor, "Available");
}

void Library::RenderCardInfo(const Novel& novel, const ImVec2& cardStart) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Calculate info area position
    ImVec2 infoStart = ImVec2(cardStart.x + 10, cardStart.y + COVER_AREA_HEIGHT + 30);
    float infoWidth = CARD_WIDTH - 20;

    // Use ImDrawList for text rendering to avoid cursor position issues
    ImU32 titleColor = IM_COL32(230, 240, 255, 255);
    ImU32 authorColor = IM_COL32(204, 204, 153, 255);
    ImU32 infoColor = IM_COL32(180, 180, 180, 255);

    // Title (truncated)
    std::string truncatedTitle = TruncateText(novel.name, 25);
    drawList->AddText(infoStart, titleColor, truncatedTitle.c_str());

    // Author
    std::string truncatedAuthor = TruncateText(novel.authorname, 20);
    std::string authorText = "Author: " + truncatedAuthor;
    drawList->AddText(ImVec2(infoStart.x, infoStart.y + 18), authorColor, authorText.c_str());

    // Chapter info
    std::string chapterText = std::to_string(novel.downloadedchapters) + "/" + std::to_string(novel.totalchapters) + " chapters";
    drawList->AddText(ImVec2(infoStart.x, infoStart.y + 36), infoColor, chapterText.c_str());

    // Progress bar
    ImVec2 progressStart = ImVec2(infoStart.x, infoStart.y + 54);
    ImVec2 progressEnd = ImVec2(progressStart.x + infoWidth, progressStart.y + 15);

    // Background
    drawList->AddRectFilled(progressStart, progressEnd, IM_COL32(60, 60, 60, 255), 2.0f);

    // Progress fill
    float progress = novel.progress.progresspercentage / 100.0f;
    ImVec2 fillEnd = ImVec2(progressStart.x + (infoWidth * progress), progressEnd.y);
    drawList->AddRectFilled(progressStart, fillEnd, IM_COL32(51, 179, 76, 255), 2.0f);

    // Progress text
    std::string progressText = std::to_string(static_cast<int>(novel.progress.progresspercentage)) + "% complete";
    drawList->AddText(ImVec2(infoStart.x, infoStart.y + 72), infoColor, progressText.c_str());
}

void Library::RenderNovelListView() {
    ImGui::BeginChild("NovelList", ImVec2(0, 0), true);

    if (ImGui::BeginTable("NovelsTable", 5,
        ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_ScrollY | ImGuiTableFlags_Sortable)) {

        SetupTableColumns();
        ImGui::TableHeadersRow();

        for (int i = 0; i < static_cast<int>(novellist.size()); i++) {
            RenderTableRow(novellist[i], i);
        }

        ImGui::EndTable();
    }

    ImGui::EndChild();
}

void Library::SetupTableColumns() {
    ImGui::TableSetupColumn("Cover", ImGuiTableColumnFlags_WidthFixed, 60.0f);
    ImGui::TableSetupColumn("Title", ImGuiTableColumnFlags_WidthFixed, 250.0f);
    ImGui::TableSetupColumn("Author", ImGuiTableColumnFlags_WidthFixed, 180.0f);
    ImGui::TableSetupColumn("Progress", ImGuiTableColumnFlags_WidthFixed, 120.0f);
    ImGui::TableSetupColumn("Chapters", ImGuiTableColumnFlags_WidthFixed, 100.0f);
}

void Library::RenderTableRow(const Novel& novel, int index) {
    ImGui::TableNextRow();
    ImGui::PushID(index);

    // Cover column
    ImGui::TableSetColumnIndex(0);
    VkDescriptorSet coverTexture = GetCoverTexture(novel.coverpath);
    if (coverTexture != VK_NULL_HANDLE) {
        ImGui::Image(reinterpret_cast<ImTextureID>(coverTexture), ImVec2(40, 50));
    }
    else {
        ImGui::Text("No Cover");
    }

    // Title column
    ImGui::TableSetColumnIndex(1);
    bool isSelected = (selectedNovelIndex == index);
    if (uiFonts.normalFont) ImGui::PushFont(uiFonts.normalFont);

    if (ImGui::Selectable(novel.name.c_str(), isSelected,
        ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap)) {
        selectedNovelIndex = index;
        showInfoPanel = true;
    }
    if (uiFonts.normalFont) ImGui::PopFont();

    // Author column
    ImGui::TableSetColumnIndex(2);
    if (uiFonts.normalFont) ImGui::PushFont(uiFonts.normalFont);
    ImGui::Text("%s", novel.authorname.c_str());
    if (uiFonts.normalFont) ImGui::PopFont();

    // Progress column
    ImGui::TableSetColumnIndex(3);
    RenderProgressBar(novel.progress.progresspercentage);

    // Chapters column
    ImGui::TableSetColumnIndex(4);
    if (uiFonts.normalFont) ImGui::PushFont(uiFonts.normalFont);
    ImGui::Text("%d/%d", novel.progress.readchapters, novel.downloadedchapters);
    if (uiFonts.normalFont) ImGui::PopFont();

    ImGui::PopID();
}

void Library::RenderProgressBar(float percentage) {
    float progress = percentage / 100.0f;

    // Get available width for the progress bar in the current table cell
    float availableWidth = ImGui::GetContentRegionAvail().x;
    float progressBarWidth = std::min(100.0f, availableWidth * 0.7f);

    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.2f, 0.7f, 0.3f, 1.0f));
    ImGui::ProgressBar(progress, ImVec2(progressBarWidth, 0), "");
    ImGui::PopStyleColor();

    // Only use SameLine if there's enough space
    float textWidth = ImGui::CalcTextSize("100.0%").x;
    if (availableWidth > progressBarWidth + textWidth + 10) {
        ImGui::SameLine();
    }

    if (uiFonts.smallFont) ImGui::PushFont(uiFonts.smallFont);
    ImGui::Text("%.1f%%", percentage);
    if (uiFonts.smallFont) ImGui::PopFont();
}

// ============================================================================
// Info Panel
// ============================================================================

void Library::RenderInfoPanel() {
    if (!showInfoPanel || selectedNovelIndex < 0 ||
        selectedNovelIndex >= static_cast<int>(novellist.size())) {
        return;
    }

    const Novel& novel = novellist[selectedNovelIndex];

    ImGui::BeginChild("InfoPanel", ImVec2(0, 0), true);

    RenderInfoPanelHeader();
    RenderInfoPanelContent(novel);

    ImGui::EndChild();
}

void Library::RenderInfoPanelHeader() {
    // Close button
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
    if (ImGui::Button(ICON_FA_XMARK, ImVec2(30, 30))) {
        showInfoPanel = false;
        selectedNovelIndex = -1;
    }
    ImGui::PopStyleColor(2);

    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.9f, 1.0f, 1.0f));
    if (uiFonts.titleFont) ImGui::PushFont(uiFonts.titleFont);
    ImGui::Text("%s Novel Information", ICON_FA_CIRCLE_INFO);
    if (uiFonts.titleFont) ImGui::PopFont();
    ImGui::PopStyleColor();

    ImGui::Separator();
    ImGui::Spacing();
}

void Library::RenderInfoPanelContent(const Novel& novel) {
    ImVec2 availableSize = ImGui::GetContentRegionAvail();
    float detailsWidth = availableSize.x - INFO_PANEL_COVER_WIDTH - 20.0f;

    // Two-column layout
    RenderInfoPanelCover(novel);

    ImGui::SameLine();
    ImGui::Dummy(ImVec2(20, 0));
    ImGui::SameLine();

    RenderInfoPanelDetails(novel, detailsWidth);

    ImGui::Spacing();
    ImGui::Separator();

    RenderSynopsisSection(novel);
    RenderChapterOverview(novel);
}

void Library::RenderInfoPanelCover(const Novel& novel) {
    ImGui::BeginGroup();

    // Cover frame background
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 coverStart = ImGui::GetCursorScreenPos();
    ImVec2 coverEnd = ImVec2(coverStart.x + INFO_PANEL_COVER_WIDTH, coverStart.y + 280);
    drawList->AddRectFilled(coverStart, coverEnd, IM_COL32(30, 30, 35, 255), 5.0f);
    drawList->AddRect(coverStart, coverEnd, IM_COL32(60, 60, 70, 255), 5.0f, 0, 2.0f);

    VkDescriptorSet coverTexture = GetCoverTexture(novel.coverpath);
    if (coverTexture != VK_NULL_HANDLE) {
        RenderInfoPanelCoverImage(coverTexture, novel.coverpath, coverStart);
    }
    else {
        RenderInfoPanelPlaceholder(coverStart);
    }

    ImGui::EndGroup();
}

void Library::RenderInfoPanelCoverImage(VkDescriptorSet texture, const std::string& coverPath,
    const ImVec2& coverStart) {
    CoverTexture& coverTexture = coverTextures[coverPath];

    float maxHeight = 260.0f;
    float maxWidth = INFO_PANEL_COVER_WIDTH - 20.0f;
    float aspectRatio = static_cast<float>(coverTexture.width) / static_cast<float>(coverTexture.height);

    float displayHeight = maxHeight;
    float displayWidth = displayHeight * aspectRatio;

    if (displayWidth > maxWidth) {
        displayWidth = maxWidth;
        displayHeight = displayWidth / aspectRatio;
    }

    float imageStartX = (INFO_PANEL_COVER_WIDTH - displayWidth) * 0.5f;
    float imageStartY = (280 - displayHeight) * 0.5f;

    ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() + imageStartX,
        ImGui::GetCursorPosY() + imageStartY + 10));
    ImGui::Image(reinterpret_cast<ImTextureID>(texture), ImVec2(displayWidth, displayHeight));
}

void Library::RenderInfoPanelPlaceholder(const ImVec2& coverStart) {
    ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() + 10, ImGui::GetCursorPosY() + 100));
    ImGui::BeginGroup();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
    ImGui::Text("📚");
    ImGui::Text("No Cover");
    ImGui::Text("Available");
    ImGui::PopStyleColor();
    ImGui::EndGroup();
}

void Library::RenderInfoPanelDetails(const Novel& novel, float detailsWidth) {
    ImGui::BeginGroup();

    // Novel Title
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.95f, 1.0f, 1.0f));
    if (uiFonts.largeFont) ImGui::PushFont(uiFonts.largeFont);
    ImGui::TextWrapped("%s", novel.name.c_str());
    if (uiFonts.largeFont) ImGui::PopFont();
    ImGui::PopStyleColor();

    // Author
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.9f, 0.7f, 1.0f));
    if (uiFonts.normalFont) ImGui::PushFont(uiFonts.normalFont);
    ImGui::Text("%s %s", ICON_FA_PEN_TO_SQUARE, novel.authorname.c_str());
    if (uiFonts.normalFont) ImGui::PopFont();
    ImGui::PopStyleColor();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    RenderStatsArea(novel, detailsWidth);
    RenderActionButtons(novel, detailsWidth);

    ImGui::EndGroup();
}

void Library::RenderStatsArea(const Novel& novel, float detailsWidth) {
    ImGui::BeginChild("StatsArea", ImVec2(detailsWidth, 120), true, ImGuiWindowFlags_NoScrollbar);

    // Downloaded Chapters
    ImGui::BeginGroup();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.9f, 1.0f, 1.0f));
    ImGui::Text("%s Downloaded Chapters", ICON_FA_BOOK_OPEN);
    ImGui::PopStyleColor();
    if (uiFonts.largeFont) ImGui::PushFont(uiFonts.largeFont);
    ImGui::Text("%d", novel.downloadedchapters);
    if (uiFonts.largeFont) ImGui::PopFont();
    ImGui::EndGroup();

    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 20);

    // Read Chapters
    ImGui::BeginGroup();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 1.0f, 0.7f, 1.0f));
    ImGui::Text("%s Chapters Read", ICON_FA_CHECK);
    ImGui::PopStyleColor();
    if (uiFonts.largeFont) ImGui::PushFont(uiFonts.largeFont);
    ImGui::Text("%d", novel.progress.readchapters);
    if (uiFonts.largeFont) ImGui::PopFont();
    ImGui::EndGroup();

    ImGui::Spacing();

    // Progress Bar
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.6f, 1.0f));
    ImGui::Text("%s Reading Progress", ICON_FA_CHART_BAR);
    ImGui::PopStyleColor();

    float progress = novel.progress.progresspercentage / 100.0f;
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.2f, 0.8f, 0.3f, 1.0f));
    ImGui::ProgressBar(progress, ImVec2(detailsWidth - 20, 25), "");
    ImGui::PopStyleColor();

    ImGui::SameLine();
    ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() - detailsWidth + 30, ImGui::GetCursorPosY() + 2));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    ImGui::Text("%.1f%% Complete", novel.progress.progresspercentage);
    ImGui::PopStyleColor();

    ImGui::EndChild();
}

void Library::RenderActionButtons(const Novel& novel, float detailsWidth) {
    ImGui::Spacing();
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10, 8));

    // Check if user has reached the latest downloaded chapter
    bool atLatestChapter = (novel.progress.readchapters >= novel.downloadedchapters);
    bool hasMoreOnline = (novel.downloadedchapters < novel.totalchapters);

    if (atLatestChapter && hasMoreOnline) {
        // Show download latest chapters button
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.4f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.5f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.3f, 0.1f, 1.0f));

        std::string downloadButtonText = std::string(ICON_FA_DOWNLOAD) + " Download Latest Chapters";
        if (ImGui::Button(downloadButtonText.c_str(), ImVec2(detailsWidth, 45))) {
            CheckAndDownloadLatestChapters(novel);
        }
        ImGui::PopStyleColor(3);
    }
    else {
        // Continue Reading button
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.6f, 0.1f, 1.0f));

        std::string continueButtonText = std::string(ICON_FA_BOOK) + " Continue Reading";
        if (ImGui::Button(continueButtonText.c_str(), ImVec2(detailsWidth, 45))) {
            int nextChapter = novel.progress.readchapters + 1;
            if (nextChapter > novel.downloadedchapters) nextChapter = novel.downloadedchapters;
            SwitchToReading(novel.name, nextChapter);
        }
        ImGui::PopStyleColor(3);
    }

    // Smaller action buttons in a row
    float smallButtonWidth = (detailsWidth - 20) / 3.0f;
    RenderSmallActionButtons(novel, smallButtonWidth);

    ImGui::PopStyleVar(); // ItemSpacing
}

void Library::RenderSmallActionButtons(const Novel& novel, float buttonWidth) {
    // Read from Start
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.4f, 0.8f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.5f, 0.5f, 0.9f, 1.0f));

    std::string startOverText = std::string(ICON_FA_ARROW_RIGHT) + " Start Over";
    if (ImGui::Button(startOverText.c_str(), ImVec2(buttonWidth, 35))) {
        SwitchToReading(novel.name, 1);
    }
    ImGui::PopStyleColor(2);

    ImGui::SameLine();

    // Read Latest
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.4f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.5f, 0.3f, 1.0f));

    std::string latestText = std::string(ICON_FA_BOLT) + " Latest";
    if (ImGui::Button(latestText.c_str(), ImVec2(buttonWidth, 35))) {
        SwitchToReading(novel.name, novel.downloadedchapters);
    }
    ImGui::PopStyleColor(2);

    ImGui::SameLine();

    // Mark as Read - now functional
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));

    std::string markReadText = std::string(ICON_FA_CHECK) + " Mark Read";
    if (ImGui::Button(markReadText.c_str(), ImVec2(buttonWidth, 35))) {
        MarkNovelAsRead(novel.name);
    }
    ImGui::PopStyleColor(2);
}

void Library::MarkNovelAsRead(const std::string& novelName) {
    for (auto& novel : novellist) {
        if (novel.name == novelName) {
            novel.progress.readchapters = novel.downloadedchapters;
            novel.progress.progresspercentage = 100.0f;
            SaveNovels(novellist);
            std::cout << "Marked " << novelName << " as read" << std::endl;
            break;
        }
    }
}

void Library::CheckAndDownloadLatestChapters(const Novel& novel) {
    // Create a search result from the current novel to trigger download
    SearchResult result;
    result.title = novel.name;
    result.author = novel.authorname;
    result.totalChapters = novel.totalchapters;

    // Download from the last downloaded chapter + 1 to the total
    int startChapter = novel.downloadedchapters + 1;
    int endChapter = novel.totalchapters;

    if (startChapter <= endChapter) {
        StartDownload(result, startChapter, endChapter);
        std::cout << "Started downloading chapters " << startChapter << " to " << endChapter
            << " for " << novel.name << std::endl;
    }
}

void Library::RenderSynopsisSection(const Novel& novel) {
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.9f, 1.0f, 1.0f));
    if (uiFonts.normalFont) ImGui::PushFont(uiFonts.normalFont);
    ImGui::Text("%s Synopsis", ICON_FA_PEN_TO_SQUARE);
    if (uiFonts.normalFont) ImGui::PopFont();
    ImGui::PopStyleColor();

    ImGui::BeginChild("SynopsisArea", ImVec2(0, 120), true);
    if (uiFonts.normalFont) ImGui::PushFont(uiFonts.normalFont);
    if (!novel.synopsis.empty()) {
        ImGui::TextWrapped("%s", novel.synopsis.c_str());
    }
    else {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
        ImGui::TextWrapped("No synopsis available for this novel.");
        ImGui::PopStyleColor();
    }
    if (uiFonts.normalFont) ImGui::PopFont();
    ImGui::EndChild();

    ImGui::Spacing();
}

void Library::RenderChapterOverview(const Novel& novel) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.9f, 1.0f, 1.0f));
    if (uiFonts.normalFont) ImGui::PushFont(uiFonts.normalFont);
    ImGui::Text("%s Chapter Overview", ICON_FA_CIRCLE_INFO);
    if (uiFonts.normalFont) ImGui::PopFont();
    ImGui::PopStyleColor();

    ImGui::BeginChild("ChapterGrid", ImVec2(0, 0), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);

    float chapterButtonWidth = (ImGui::GetContentRegionAvail().x - 25.0f) / CHAPTER_GRID_COLUMNS;
    float chapterButtonHeight = 40.0f;

    // Only show downloaded chapters
    for (int i = 1; i <= novel.downloadedchapters; i++) {
        RenderChapterButton(novel, i, chapterButtonWidth, chapterButtonHeight);

        // Create 3-column layout
        if (i % CHAPTER_GRID_COLUMNS != 0 && i < novel.downloadedchapters) {
            ImGui::SameLine();
        }
    }

    ImGui::EndChild();
}

void Library::RenderChapterButton(const Novel& novel, int chapterNum,
    float buttonWidth, float buttonHeight) {
    bool isRead = (chapterNum <= novel.progress.readchapters);
    bool isCurrentChapter = (chapterNum == novel.progress.readchapters + 1);

    auto [buttonColor, buttonHovered, buttonActive, textColor] = GetChapterButtonColors(isRead, isCurrentChapter);

    ImGui::PushStyleColor(ImGuiCol_Button, buttonColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, buttonHovered);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, buttonActive);
    ImGui::PushStyleColor(ImGuiCol_Text, textColor);

    // Fix: Use proper chapter text without FontAwesome icons that might not render
    std::string chapterText = std::to_string(chapterNum);
    if (isRead) {
        chapterText += " " + std::string(ICON_FA_CHECK);
    }
    else if (isCurrentChapter) {
        chapterText += " " + std::string(ICON_FA_PLAY);
    }

    if (ImGui::Button(chapterText.c_str(), ImVec2(buttonWidth, buttonHeight))) {
        SwitchToReading(novel.name, chapterNum);
    }

    ImGui::PopStyleColor(4);

    RenderChapterTooltip(chapterNum, isRead, isCurrentChapter);
}

std::tuple<ImVec4, ImVec4, ImVec4, ImVec4> Library::GetChapterButtonColors(bool isRead, bool isCurrentChapter) {
    if (isRead) {
        // Read chapters - green
        return {
            ImVec4(0.15f, 0.4f, 0.15f, 1.0f),  // button
            ImVec4(0.2f, 0.5f, 0.2f, 1.0f),    // hovered
            ImVec4(0.1f, 0.35f, 0.1f, 1.0f),   // active
            ImVec4(0.9f, 1.0f, 0.9f, 1.0f)     // text
        };
    }
    else if (isCurrentChapter) {
        // Next chapter to read - bright blue
        return {
            ImVec4(0.2f, 0.4f, 0.8f, 1.0f),
            ImVec4(0.3f, 0.5f, 0.9f, 1.0f),
            ImVec4(0.15f, 0.35f, 0.7f, 1.0f),
            ImVec4(1.0f, 1.0f, 1.0f, 1.0f)
        };
    }
    else {
        // Unread chapters - gray
        return {
            ImVec4(0.25f, 0.25f, 0.25f, 1.0f),
            ImVec4(0.35f, 0.35f, 0.35f, 1.0f),
            ImVec4(0.2f, 0.2f, 0.2f, 1.0f),
            ImVec4(0.7f, 0.7f, 0.7f, 1.0f)
        };
    }
}

void Library::RenderChapterTooltip(int chapterNum, bool isRead, bool isCurrentChapter) {
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        if (isRead) {
            ImGui::Text("%s Chapter %d - Read", ICON_FA_CHECK, chapterNum);
        }
        else if (isCurrentChapter) {
            ImGui::Text("%s Chapter %d - Continue from here", ICON_FA_PLAY, chapterNum);
        }
        else {
            ImGui::Text("%s Chapter %d - Unread", ICON_FA_PAUSE, chapterNum);
        }
        ImGui::EndTooltip();
    }
}

// ============================================================================
// Reading View
// ============================================================================

void Library::RenderFullScreenReading() {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_MenuBar;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    if (ImGui::Begin("Reading View", nullptr, windowFlags)) {
        float menuBarHeight = RenderReadingMenuBar();
        RenderReadingContent(menuBarHeight);
    }

    ImGui::End();
    ImGui::PopStyleVar(3);

    // Render settings panel if open (outside the main window)
    chaptermanager.RenderSettingsPanel();

    // Handle ESC key
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        SwitchToLibrary();
    }
}

float Library::RenderReadingMenuBar() {
    float menuBarHeight = 0.0f;

    if (ImGui::BeginMenuBar()) {
        menuBarHeight = ImGui::GetFrameHeight();

        // Use normal font for better readability
        if (uiFonts.normalFont) ImGui::PushFont(uiFonts.normalFont);

        // Back button with FontAwesome
        std::string backButtonText = std::string(ICON_FA_ARROW_LEFT) + " Back";
        if (ImGui::Button(backButtonText.c_str())) {
            SwitchToLibrary();
        }

        RenderChapterInfo();
        RenderNavigationControls();

        if (uiFonts.normalFont) ImGui::PopFont();
        ImGui::EndMenuBar();
    }

    return menuBarHeight;
}

void Library::RenderChapterInfo() {
    if (chaptermanager.getChapters().empty()) return;

    const auto& chapters = chaptermanager.getChapters();
    const auto& settings = chaptermanager.getSettings();

    if (settings.currentChapter >= 1 && settings.currentChapter <= static_cast<int>(chapters.size())) {
        const ChapterManager::Chapter& current = chapters[settings.currentChapter - 1];

        // Calculate center position for chapter title
        std::string chapterText = GetCurrentNovelName() + " - Chapter " +
            std::to_string(current.chapterNumber) + ": " + current.title;
        float textWidth = ImGui::CalcTextSize(chapterText.c_str()).x;
        float availableWidth = ImGui::GetContentRegionAvail().x - 200; // Reserve space for navigation
        float centerPos = (availableWidth - textWidth) * 0.5f;

        if (centerPos > 20) {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + centerPos);
        }
        ImGui::Text("%s", chapterText.c_str());
    }
}

void Library::RenderNavigationControls() {
    auto& settings = chaptermanager.getSettings();
    const auto& chapters = chaptermanager.getChapters();

    // Push to right side
    float navWidth = 300.0f;
    float remainingWidth = ImGui::GetContentRegionAvail().x - navWidth;
    if (remainingWidth > 0) {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + remainingWidth);
    }

    std::string prevButtonText = std::string(ICON_FA_ARROW_LEFT) + " Prev";
    if (ImGui::Button(prevButtonText.c_str()) && settings.currentChapter > 1) {
        chaptermanager.OpenChapter(settings.currentChapter - 1);
    }

    ImGui::SameLine();
    ImGui::Text("%d/%zu", settings.currentChapter, chapters.size());

    ImGui::SameLine();
    std::string nextButtonText = std::string("Next ") + ICON_FA_ARROW_RIGHT;
    if (ImGui::Button(nextButtonText.c_str()) && settings.currentChapter < static_cast<int>(chapters.size())) {
        chaptermanager.OpenChapter(settings.currentChapter + 1);
    }

    ImGui::SameLine();
    std::string settingsButtonText = std::string(ICON_FA_GEAR) + " Settings";
    if (ImGui::Button(settingsButtonText.c_str())) {
        chaptermanager.ToggleSettings();
    }
}

void Library::RenderReadingContent(float menuBarHeight) {
    // Calculate available space for content (subtract menu bar height)
    ImVec2 availableSize = ImGui::GetContentRegionAvail();
    availableSize.y -= menuBarHeight;

    // Create content area that uses the full remaining space
    ImGui::BeginChild("FullScreenContent", availableSize, false, ImGuiWindowFlags_None);
    chaptermanager.RenderContentOnly();
    ImGui::EndChild();
}

// ============================================================================
// Download Manager
// ============================================================================

void Library::InitializeDownloadSources() {
    if (!std::filesystem::exists("sources.json")) {
        SaveDownloadSources();
    }
    LoadDownloadSources();
}

void Library::LoadDownloadSources() {
    try {
        std::ifstream file("sources.json");
        if (!file.is_open()) {
            std::cout << "No sources config found, creating default" << std::endl;
            return;
        }

        json j;
        file >> j;
        file.close();

        downloadSources.clear();
        if (j.contains("sources")) {
            for (const auto& sourceJson : j["sources"]) {
                DownloadSource source;
                source.name = sourceJson.value("name", "");
                source.baseUrl = sourceJson.value("base_url", "");
                source.searchEndpoint = sourceJson.value("search_endpoint", "");
                source.pythonScript = "download_manager.py";
                source.enabled = sourceJson.value("enabled", true);
                downloadSources.push_back(source);
            }
        }

        std::cout << "Loaded " << downloadSources.size() << " download sources" << std::endl;
    }
    catch (const std::exception& e) {
        std::cout << "Error loading download sources: " << e.what() << std::endl;
    }
}

void Library::SaveDownloadSources() {
    try {
        json j;
        json sourcesArray = json::array();

        if (downloadSources.empty()) {
            CreateDefaultDownloadSources();
        }

        for (const auto& source : downloadSources) {
            json sourceJson;
            sourceJson["name"] = source.name;
            sourceJson["base_url"] = source.baseUrl;
            sourceJson["search_endpoint"] = source.searchEndpoint;
            sourceJson["enabled"] = source.enabled;
            sourcesArray.push_back(sourceJson);
        }

        j["sources"] = sourcesArray;

        std::ofstream file("sources.json");
        if (file.is_open()) {
            file << j.dump(4);
            file.close();
            std::cout << "Download sources saved" << std::endl;
        }
    }
    catch (const std::exception& e) {
        std::cout << "Error saving download sources: " << e.what() << std::endl;
    }
}

void Library::CreateDefaultDownloadSources() {
    downloadSources = {
        DownloadSource("RoyalRoad", "https://www.royalroad.com",
                      "/fictions/search?title={query}", "download_manager.py", true),
        DownloadSource("NovelUpdates", "https://www.novelupdates.com",
                      "/series-finder/?sf=1&sh={query}", "download_manager.py", true),
        DownloadSource("WebNovel", "https://www.webnovel.com",
                      "/search?keywords={query}", "download_manager.py", false)
    };
}

bool Library::CallPythonScript(const std::string& scriptName, const std::vector<std::string>& args,
    std::string& output) {
    std::string command = "python " + scriptName;
    for (const auto& arg : args) {
        command += " \"" + arg + "\"";
    }
    std::cout << "Executing: " << command << std::endl;

#ifdef _WIN32
    FILE* pipe = _popen(command.c_str(), "r");
#else
    FILE* pipe = popen(command.c_str(), "r");
#endif

    if (!pipe) {
        std::cout << "Failed to execute Python script" << std::endl;
        return false;
    }

    char buffer[128];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output += buffer;
    }

#ifdef _WIN32
    int result = _pclose(pipe);
#else
    int result = pclose(pipe);
#endif

    return result == 0;
}

bool Library::CallPythonScriptAsync(const std::string& scriptName, const std::vector<std::string>& args,
    std::function<void(const std::string&)> progressCallback,
    std::function<void(bool, const std::string&)> completionCallback) {

    std::string command = "python " + scriptName;
    for (const auto& arg : args) {
        command += " \"" + arg + "\"";
    }

    // IMPORTANT: Add "2>&1" to redirect stderr to stdout
    command += " 2>&1";

    std::cout << "Executing: " << command << std::endl;

    // Run in separate thread to avoid blocking UI
    std::thread([command, progressCallback, completionCallback]() {
        std::string output;

#ifdef _WIN32
        FILE* pipe = _popen(command.c_str(), "r");
#else
        FILE* pipe = popen(command.c_str(), "r");
#endif

        if (!pipe) {
            completionCallback(false, "Failed to execute Python script");
            return;
        }

        char buffer[256];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            std::string line(buffer);
            output += line;

            // Call progress callback for each line (real-time updates)
            if (progressCallback) {
                progressCallback(line);
            }
        }

#ifdef _WIN32
        int result = _pclose(pipe);
#else
        int result = pclose(pipe);
#endif

        // Call completion callback when done
        completionCallback(result == 0, output);
        }).detach();

    return true;
}

bool Library::SearchNovels(const std::string& query) {
    if (query.empty()) return false;

    isSearching = true;
    searchResults.clear();
    searchQuery = query;

    std::vector<std::string> args = {
        "search",
        "--query", query,
        "--config", "sources.json"
    };

    std::string output;
    bool success = CallPythonScript("download_manager.py", args, output);

    if (success && !output.empty()) {
        success = ParseSearchResults(output);
    }

    isSearching = false;
    return success;
}

bool Library::ParseSearchResults(const std::string& output) {
    try {
        json resultsJson = json::parse(output);

        for (const auto& resultJson : resultsJson) {
            SearchResult result;
            result.title = resultJson.value("title", "");
            result.author = resultJson.value("author", "");
            result.url = resultJson.value("url", "");
            result.sourceName = resultJson.value("source_name", "");
            result.totalChapters = resultJson.value("total_chapters", 0);
            result.description = resultJson.value("description", "");
            result.coverUrl = resultJson.value("cover_url", "");

            searchResults.push_back(result);
        }

        std::cout << "Found " << searchResults.size() << " search results" << std::endl;
        return true;
    }
    catch (const std::exception& e) {
        std::cout << "Error parsing search results: " << e.what() << std::endl;
        return false;
    }
}

void Library::StartDownload(const SearchResult& result, int startChapter, int endChapter) {
    DownloadTask task = CreateDownloadTask(result, startChapter, endChapter);
    downloadQueue.push_back(task);

    if (!downloadManagerRunning) {
        StartDownloadManager();
    }

    std::cout << "Added download task: " << result.title << std::endl;
}

Library::DownloadTask Library::CreateDownloadTask(const SearchResult& result, int startChapter, int endChapter) {
    DownloadTask task;
    task.novelName = result.title;
    task.author = result.author;
    task.sourceUrl = result.url;
    task.sourceName = result.sourceName;
    task.startChapter = startChapter;
    task.endChapter = endChapter;
    task.currentChapter = 0;
    task.totalChapters = result.totalChapters;
    task.isActive = false;
    task.isPaused = false;
    task.isComplete = false;
    task.status = "Queued";
    task.progress = 0.0f;
    return task;
}

void Library::StartDownloadManager() {
    downloadManagerRunning = true;
    downloadThread = std::make_unique<std::thread>(&Library::ProcessDownloadQueue, this);
}

void Library::StopDownloadManager() {
    downloadManagerRunning = false;
    if (downloadThread && downloadThread->joinable()) {
        downloadThread->join();
    }
}

void Library::ProcessDownloadQueue() {
    while (downloadManagerRunning) {
        bool hasActiveDownload = ProcessNextDownload();

        if (!hasActiveDownload && !HasQueuedDownloads()) {
            downloadManagerRunning = false;
            break;
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

bool Library::ProcessNextDownload() {
    for (auto& task : downloadQueue) {
        if (task.isComplete || task.isPaused) {
            continue;
        }

        if (!task.isActive) {
            return ExecuteDownloadTask(task);
        }
    }
    return false;
}

void Library::ParseProgressLine(const std::string& line) {
    // Remove newline characters
    std::string cleanLine = line;
    cleanLine.erase(std::remove(cleanLine.begin(), cleanLine.end(), '\n'), cleanLine.end());
    cleanLine.erase(std::remove(cleanLine.begin(), cleanLine.end(), '\r'), cleanLine.end());

    // Parse "Progress: 1/50 (2.0%) - Chapter 1: Title"
    if (cleanLine.find("Progress: ") == 0) {
        try {
            // Find the pattern: Progress: X/Y (Z.Z%) - Chapter Title
            size_t colonPos = cleanLine.find(": ");
            if (colonPos == std::string::npos) return;

            size_t slashPos = cleanLine.find("/", colonPos);
            if (slashPos == std::string::npos) return;

            size_t openParenPos = cleanLine.find(" (", slashPos);
            if (openParenPos == std::string::npos) return;

            size_t closeParenPos = cleanLine.find("%) - ", openParenPos);
            if (closeParenPos == std::string::npos) return;

            // Extract current chapter number
            std::string currentStr = cleanLine.substr(colonPos + 2, slashPos - colonPos - 2);
            downloadprogress.current = std::stoi(currentStr);

            // Extract total chapters
            std::string totalStr = cleanLine.substr(slashPos + 1, openParenPos - slashPos - 1);
            downloadprogress.total = std::stoi(totalStr);

            // Extract percentage
            std::string percentStr = cleanLine.substr(openParenPos + 2, closeParenPos - openParenPos - 2);
            downloadprogress.percentage = std::stof(percentStr);

            // Extract chapter title
            downloadprogress.chapterTitle = cleanLine.substr(closeParenPos + 4);

            std::cout << "Progress parsed: " << downloadprogress.current << "/"
                << downloadprogress.total << " (" << downloadprogress.percentage
                << "%) - " << downloadprogress.chapterTitle << std::endl;

        }
        catch (const std::exception& e) {
            std::cout << "Error parsing progress line: " << e.what() << std::endl;
        }
    }

    else if (cleanLine.find("Download completed") != std::string::npos) {
        std::cout << "Download completed successfully" << std::endl;
        downloadprogress.isComplete = true;
    }
}

bool Library::ExecuteDownloadTask(DownloadTask& task) {
    task.isActive = true;
    task.status = "Downloading";

    std::vector<std::string> args = BuildDownloadArgs(task);
    std::string output;

    downloadprogress.reset();
    downloadprogress.isActive = true;

    CallPythonScriptAsync("download_manager.py", args,
        // Progress callback (called for each line)
        [this](const std::string& line) {
            ParseProgressLine(line);
        },
        // Completion callback
        [this](bool success, const std::string& output) {
            downloadprogress.isActive = false;
            downloadprogress.isComplete = success;
            LoadAllNovelsFromFile();
            if (!success) {
                std::cout << "Download failed: " << output << std::endl;
                return false;
            }
        }
    );
    return true;
}

std::vector<std::string> Library::BuildDownloadArgs(const DownloadTask& task) {
    std::vector<std::string> args = {
        "download",
        "--source", task.sourceName,
        "--output", "Novels",
        "--start", std::to_string(task.startChapter)
    };

    // Check if we have a full URL or just a novel name
    if (IsFullUrl(task.sourceUrl)) {
        // Use the full URL
        args.push_back("--url");
        args.push_back(task.sourceUrl);
    }
    else {
        // Use novel name (will be converted to URL by Python script)
        args.push_back("--name");
        args.push_back(task.sourceUrl); // This actually contains the novel name
    }

    // Add end chapter if specified
    if (task.endChapter > 0) {
        args.push_back("--end");
        args.push_back(std::to_string(task.endChapter));
    }

    return args;
}

bool Library::IsFullUrl(const std::string& input) {
    return input.find("http://") == 0 || input.find("https://") == 0;
}

bool Library::HasQueuedDownloads() {
    for (const auto& task : downloadQueue) {
        if (!task.isComplete && !task.isPaused) {
            return true;
        }
    }
    return false;
}

void Library::PauseDownload(int taskIndex) {
    if (IsValidTaskIndex(taskIndex)) {
        downloadQueue[taskIndex].isPaused = true;
        downloadQueue[taskIndex].status = "Paused";
    }
}

void Library::ResumeDownload(int taskIndex) {
    if (IsValidTaskIndex(taskIndex)) {
        downloadQueue[taskIndex].isPaused = false;
        downloadQueue[taskIndex].status = "Queued";
    }
}

void Library::CancelDownload(int taskIndex) {
    if (IsValidTaskIndex(taskIndex)) {
        downloadQueue[taskIndex].isComplete = true;
        downloadQueue[taskIndex].status = "Cancelled";
    }
}

bool Library::IsValidTaskIndex(int taskIndex) {
    return taskIndex >= 0 && taskIndex < static_cast<int>(downloadQueue.size());
}

// ============================================================================
// Download Manager UI
// ============================================================================

void Library::RenderDownloadManager() {
    ImGui::BeginChild("DownloadManager", ImVec2(0, 0), true);

    ImGui::PushStyleVar(ImGuiStyleVar_TabRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(20, 12));

    if (ImGui::BeginTabBar("DownloadTabs")) {
        if (ImGui::BeginTabItem(ICON_FA_MAGNIFYING_GLASS " Search")) {
            RenderSearchTab();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem(ICON_FA_DOWNLOAD " Downloads")) {
            RenderDownloadQueue();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::PopStyleVar(2);

    ImGui::EndChild();

    
}

void Library::RenderSearchTab() {

    RenderSearchInput();
    RenderSearchResults();
}

void Library::RenderSearchInput() {
    static char searchBuffer[256] = "";
    ImGui::Text("Search Query:");
    ImGui::SetNextItemWidth(300);
    ImGui::InputText("##SearchQuery", searchBuffer, sizeof(searchBuffer));

    ImGui::SameLine();
    if (ImGui::Button("Search", ImVec2(80, 0)) && strlen(searchBuffer) > 0) {
        searchQuery = std::string(searchBuffer);
        std::thread([this]() {
            SearchNovels(searchQuery);
            }).detach();
    }

    ImGui::SameLine();
    if (isSearching) {
        ImGui::Text("Searching...");
    }

    ImGui::Spacing();
    ImGui::Separator();
}

void Library::RenderSearchResults() {
    if (!searchResults.empty()) {
        ImGui::Text("Search Results (%zu found):", searchResults.size());
        ImGui::Spacing();

        for (size_t i = 0; i < searchResults.size(); i++) {
            RenderSearchResultCard(searchResults[i], i);
        }
    }
    else if (!searchQuery.empty() && !isSearching) {
        ImGui::Text("No results found for: %s", searchQuery.c_str());
    }
}

void Library::RenderSearchResultCard(const SearchResult& result, size_t index) {
    ImGui::PushID(static_cast<int>(index));

    ImGui::BeginGroup();
    {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.15f, 0.15f, 0.17f, 1.0f));
        ImGui::BeginChild("ResultCard", ImVec2(0, 180), true);

        RenderResultCardContent(result);

        ImGui::EndChild();
        ImGui::PopStyleColor();

        RenderDownloadOptions(result, index);
    }
    ImGui::EndGroup();

    ImGui::PopID();
    ImGui::Spacing();
}

void Library::RenderResultCardContent(const SearchResult& result) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.95f, 1.0f, 1.0f));
    ImGui::Text("%s", result.title.c_str());
    ImGui::PopStyleColor();

    ImGui::Text(ICON_FA_PEN_TO_SQUARE " %s", result.author.c_str());
    ImGui::Text(ICON_FA_GLOBE " %s", result.sourceName.c_str());

    if (result.totalChapters > 0) {
        ImGui::Text(ICON_FA_BOOK_OPEN " %d chapters", result.totalChapters);
    }

    if (!result.description.empty()) {
        ImGui::TextWrapped("%s", result.description.c_str());
    }
}

void Library::RenderDownloadOptions(const SearchResult& result, size_t index) {
    ImGui::BeginGroup();
    {
        static std::vector<int> startChapters;
        static std::vector<int> endChapters;
        static std::vector<bool> showAdvanced;

        // Ensure vectors are large enough
        if (startChapters.size() <= index) {
            startChapters.resize(index + 1, 1);
            endChapters.resize(index + 1, -1);
            showAdvanced.resize(index + 1, false);
        }

        ImGui::Text("Download Options:");

        // Quick Download Buttons
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.8f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.9f, 1.0f));

        if (ImGui::Button(ICON_FA_DOWNLOAD " First 5", ImVec2(100, 50))) {
            StartDownload(result, 1, 5);
        }
        ImGui::SameLine();

        if (ImGui::Button(ICON_FA_DOWNLOAD " First 10", ImVec2(100, 50))) {
            StartDownload(result, 1, 10);
        }
        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
        if (ImGui::Button(ICON_FA_DOWNLOAD " All Chapters", ImVec2(130, 50))) {
            StartDownload(result, 1, -1);
        }
        ImGui::PopStyleColor(4);

        if (downloadprogress.isActive) {
            ImGui::Text("Downloading: %d/%d chapters (%.1f%%)",
                downloadprogress.current,
                downloadprogress.total,
                downloadprogress.percentage);

            if (downloadprogress.total > 0) {
                ImGui::ProgressBar(downloadprogress.percentage / 100.0f);
            }
            else {
                ImGui::ProgressBar(0.0f, ImVec2(-1.0f, 0.0f), "Initializing...");
            }

            if (!downloadprogress.chapterTitle.empty()) {
                ImGui::Text("Current: %s", downloadprogress.chapterTitle.c_str());
            }
        }
        else if (downloadprogress.isComplete) {
            ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "Download Complete!");

            if (ImGui::Button("Clear")) {
                downloadprogress.reset();
            }
        }

        // Advanced Options Toggle
        ImGui::Spacing();
        if (ImGui::Button(showAdvanced[index] ? ICON_FA_CHEVRON_DOWN " Advanced" : ICON_FA_CHEVRON_RIGHT " Advanced")) {
            showAdvanced[index] = !showAdvanced[index];
        }

        if (showAdvanced[index]) {
            ImGui::Indent(20.0f);
            ImGui::Separator();
            ImGui::Text("Custom Range:");

            // Chapter Range Inputs
            ImGui::SetNextItemWidth(80);
            ImGui::InputInt("Start##Start", &startChapters[index], 1, 10);
            if (startChapters[index] < 1) startChapters[index] = 1;

            ImGui::SameLine();
            ImGui::SetNextItemWidth(80);
            ImGui::InputInt("End##End", &endChapters[index], 1, 10);

            ImGui::SameLine();
            ImGui::TextDisabled("(-1 = all)");

            // Show total chapters if available
            if (result.totalChapters > 0) {
                ImGui::SameLine();
                ImGui::TextDisabled("(Total: %d)", result.totalChapters);
            }

            // Quick range presets
            ImGui::Text("Quick Presets:");
            if (ImGui::Button("1-50")) {
                startChapters[index] = 1;
                endChapters[index] = 50;
            }
            ImGui::SameLine();
            if (ImGui::Button("1-100")) {
                startChapters[index] = 1;
                endChapters[index] = 100;
            }
            ImGui::SameLine();
            if (ImGui::Button("Latest 10")) {
                if (result.totalChapters > 0) {
                    startChapters[index] = std::max(1, result.totalChapters - 9);
                    endChapters[index] = result.totalChapters;
                }
            }

            // Validation and Download
            bool validRange = true;
            if (endChapters[index] != -1 && endChapters[index] < startChapters[index]) {
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Invalid range: End must be >= Start");
                validRange = false;
            }

            // Custom Download Button
            ImGui::PushStyleColor(ImGuiCol_Button, validRange ? ImVec4(0.2f, 0.7f, 0.2f, 1.0f) : ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, validRange ? ImVec4(0.3f, 0.8f, 0.3f, 1.0f) : ImVec4(0.6f, 0.6f, 0.6f, 1.0f));

            if (ImGui::Button(ICON_FA_DOWNLOAD " Download Range", ImVec2(150, 0)) && validRange) {
                StartDownload(result, startChapters[index], endChapters[index]);
            }

            ImGui::PopStyleColor(2);
            ImGui::Unindent(20.0f);
        }
    }
    ImGui::EndGroup();
}

void Library::RenderDownloadQueue() {

    if (downloadQueue.empty()) {
        ImGui::Text("No downloads in queue");
        return;
    }

    RenderDownloadTable();
}

void Library::RenderDownloadTable() {
    if (ImGui::BeginTable("DownloadsTable", 6,
        ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {

        SetupDownloadTableColumns();
        ImGui::TableHeadersRow();

        for (size_t i = 0; i < downloadQueue.size(); i++) {
            RenderDownloadTableRow(downloadQueue[i], i);
        }

        ImGui::EndTable();
    }
}

void Library::SetupDownloadTableColumns() {
    ImGui::TableSetupColumn("Novel", ImGuiTableColumnFlags_WidthFixed, 200.0f);
    ImGui::TableSetupColumn("Source", ImGuiTableColumnFlags_WidthFixed, 100.0f);
    ImGui::TableSetupColumn("Progress", ImGuiTableColumnFlags_WidthFixed, 120.0f);
    ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 100.0f);
    ImGui::TableSetupColumn("Chapters", ImGuiTableColumnFlags_WidthFixed, 80.0f);
    ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 150.0f);
}

void Library::RenderDownloadTableRow(const DownloadTask& task, size_t index) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::Text("%s", task.novelName.c_str());

    ImGui::TableSetColumnIndex(1);
    ImGui::Text("%s", task.sourceName.c_str());

    ImGui::TableSetColumnIndex(2);
    RenderTaskProgress(task);

    ImGui::TableSetColumnIndex(3);
    RenderTaskStatus(task);

    ImGui::TableSetColumnIndex(4);
    RenderTaskChapterRange(task);

    ImGui::TableSetColumnIndex(5);
    RenderTaskActions(task, index);
}

void Library::RenderTaskProgress(const DownloadTask& task) {
    float progress = task.progress;
    if (task.totalChapters > 0 && task.currentChapter > 0) {
        progress = static_cast<float>(task.currentChapter) / static_cast<float>(task.totalChapters) * 100.0f;
    }
    ImGui::ProgressBar(progress / 100.0f, ImVec2(-1, 0), "");
    ImGui::SameLine();
    ImGui::Text("%.1f%%", progress);
}

void Library::RenderTaskStatus(const DownloadTask& task) {
    ImVec4 statusColor;
    if (task.isActive) {
        statusColor = ImVec4(0.2f, 0.8f, 0.2f, 1.0f);
        ImGui::TextColored(statusColor, "Active");
    }
    else if (task.isComplete) {
        statusColor = ImVec4(0.2f, 0.8f, 0.2f, 1.0f);
        ImGui::TextColored(statusColor, "Complete");
    }
    else if (task.isPaused) {
        statusColor = ImVec4(0.8f, 0.8f, 0.2f, 1.0f);
        ImGui::TextColored(statusColor, "Paused");
    }
    else {
        statusColor = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
        ImGui::TextColored(statusColor, "Queued");
    }
}

void Library::RenderTaskChapterRange(const DownloadTask& task) {
    if (task.endChapter > 0) {
        ImGui::Text("%d-%d", task.startChapter, task.endChapter);
    }
    else {
        ImGui::Text("%d-All", task.startChapter);
    }
}

void Library::RenderTaskActions(const DownloadTask& task, size_t index) {
    ImGui::PushID(static_cast<int>(index));

    if (!task.isComplete) {
        if (task.isPaused) {
            if (ImGui::SmallButton("Resume")) {
                ResumeDownload(static_cast<int>(index));
            }
        }
        else {
            if (ImGui::SmallButton("Pause")) {
                PauseDownload(static_cast<int>(index));
            }
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Cancel")) {
            CancelDownload(static_cast<int>(index));
        }
    }
    else {
        ImGui::Text("Done");
    }

    ImGui::PopID();
}

void Library::RenderSourcesTab() {
    ImGui::Text(ICON_FA_GEAR "Download Sources Configuration");
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextWrapped("Configure novel download sources. Each source needs a corresponding template in sources.json.");
    ImGui::Spacing();

    RenderSourcesTable();
    RenderSourcesManagement();
}

void Library::RenderSourcesTable() {
    if (ImGui::BeginTable("SourcesTable", 5,
        ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {

        SetupSourcesTableColumns();
        ImGui::TableHeadersRow();

        for (size_t i = 0; i < downloadSources.size(); i++) {
            RenderSourcesTableRow(downloadSources[i], i);
        }

        ImGui::EndTable();
    }
}

void Library::SetupSourcesTableColumns() {
    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 120.0f);
    ImGui::TableSetupColumn("Base URL", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Enabled", ImGuiTableColumnFlags_WidthFixed, 70.0f);
    ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 80.0f);
    ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 100.0f);
}

void Library::RenderSourcesTableRow(DownloadSource& source, size_t index) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    if (uiFonts.normalFont) ImGui::PushFont(uiFonts.normalFont);
    ImGui::Text("%s", source.name.c_str());
    if (uiFonts.normalFont) ImGui::PopFont();

    ImGui::TableSetColumnIndex(1);
    if (uiFonts.smallFont) ImGui::PushFont(uiFonts.smallFont);
    ImGui::Text("%s", source.baseUrl.c_str());
    if (uiFonts.smallFont) ImGui::PopFont();

    ImGui::TableSetColumnIndex(2);
    ImGui::PushID(static_cast<int>(index));
    if (ImGui::Checkbox("##Enabled", &source.enabled)) {
        SaveDownloadSources();
    }
    ImGui::PopID();

    ImGui::TableSetColumnIndex(3);
    if (source.enabled) {
        ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "Active");
    }
    else {
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Disabled");
    }

    ImGui::TableSetColumnIndex(4);
    ImGui::PushID(static_cast<int>(index));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.8f, 1.0f));
    if (ImGui::SmallButton("Test")) {
        std::cout << "Testing source: " << source.name << std::endl;
    }
    ImGui::PopStyleColor();
    ImGui::PopID();
}

void Library::RenderSourcesManagement() {
    ImGui::Spacing();
    ImGui::Separator();

    ImGui::Text(ICON_FA_NOTES_MEDICAL "Source Management");
    ImGui::Spacing();

    if (ImGui::Button("💾 Save Configuration", ImVec2(150, 30))) {
        SaveDownloadSources();
    }

    ImGui::SameLine();
    if (ImGui::Button("🔄 Reload Sources", ImVec2(130, 30))) {
        LoadDownloadSources();
    }

    ImGui::SameLine();
    if (ImGui::Button("➕ Add Source", ImVec2(100, 30))) {
        AddNewDownloadSource();
    }
}

void Library::AddNewDownloadSource() {
    DownloadSource newSource;
    newSource.name = "New Source";
    newSource.baseUrl = "https://example.com";
    newSource.searchEndpoint = "/search?q={query}";
    newSource.pythonScript = "download_manager.py";
    newSource.enabled = false;
    downloadSources.push_back(newSource);
}

// ============================================================================
// Helper Functions for Vulkan Operations
// ============================================================================

void Library::CreateStagingBuffer(VkDeviceSize imageSize, stbi_uc* pixels,
    VkBuffer& stagingBuffer, VkDeviceMemory& stagingBufferMemory) {
    CreateBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer, stagingBufferMemory);

    void* data;
    VkResult result = vkMapMemory(app->m_Device, stagingBufferMemory, 0, imageSize, 0, &data);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to map staging buffer memory");
    }

    memcpy(data, pixels, static_cast<size_t>(imageSize));
    vkUnmapMemory(app->m_Device, stagingBufferMemory);
}

Library::CoverTexture Library::CreateVulkanImage(int width, int height) {
    CoverTexture texture;
    texture.width = width;
    texture.height = height;

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkResult result = vkCreateImage(app->m_Device, &imageInfo, nullptr, &texture.image);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create image");
    }

    AllocateImageMemory(texture);
    return texture;
}

void Library::AllocateImageMemory(CoverTexture& texture) {
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(app->m_Device, texture.image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkResult result = vkAllocateMemory(app->m_Device, &allocInfo, nullptr, &texture.imageMemory);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate image memory");
    }

    result = vkBindImageMemory(app->m_Device, texture.image, texture.imageMemory, 0);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to bind image memory");
    }
}

void Library::CopyImageData(VkBuffer stagingBuffer, CoverTexture& texture, int width, int height) {
    VkCommandBuffer commandBuffer = CreateOneTimeCommandBuffer();

    TransitionImageLayout(commandBuffer, texture.image, VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    CopyBufferToImage(commandBuffer, stagingBuffer, texture.image,
        static_cast<uint32_t>(width), static_cast<uint32_t>(height));

    TransitionImageLayout(commandBuffer, texture.image, VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    SubmitOneTimeCommandBuffer(commandBuffer);
}

void Library::CreateImageView(CoverTexture& texture) {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = texture.image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VkResult result = vkCreateImageView(app->m_Device, &viewInfo, nullptr, &texture.imageView);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create image view");
    }
}

void Library::CreateDescriptorSet(CoverTexture& texture) {
    VkSampler sampler = GetOrCreateTextureSampler();
    if (sampler == VK_NULL_HANDLE) {
        throw std::runtime_error("Failed to create texture sampler");
    }

    texture.descriptorSet = ImGui_ImplVulkan_AddTexture(sampler, texture.imageView,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    if (texture.descriptorSet == VK_NULL_HANDLE) {
        throw std::runtime_error("Failed to create descriptor set");
    }
}

void Library::CopyBufferToImage(VkCommandBuffer commandBuffer, VkBuffer buffer, VkImage image,
    uint32_t width, uint32_t height) {
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = { 0, 0, 0 };
    region.imageExtent = { width, height, 1 };

    vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
}

void Library::TransitionImageLayout(VkCommandBuffer commandBuffer, VkImage image, VkFormat format,
    VkImageLayout oldLayout, VkImageLayout newLayout) {
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
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
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
        newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else {
        throw std::invalid_argument("Unsupported layout transition!");
    }

    vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0,
        0, nullptr, 0, nullptr, 1, &barrier);
}

// ============================================================================
// Utility Functions
// ============================================================================

std::string Library::GetCurrentNovelName() const {
    return currentNovelName;
}

bool Library::isInLibrary() const {
    return currentState == UIState::LIBRARY;
}

bool Library::isInReading() const {
    return currentState == UIState::READING;
}