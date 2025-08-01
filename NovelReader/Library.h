#pragma once
#include "ImGui/imgui.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <tuple>
#include "ErrorHandler.h"
#include <vulkan/vulkan.h>
#include "WindowManagment.h"
#include "ChapterManager.h"
#include <functional>
#include <thread>
#include <atomic>
#include <memory>
#include "Dependecies/stb_image.h"
#include <array>
#include <mutex>
#include <Windows.h>
#include <chrono>

class Library {
public:
    class Novel {
    public:
        struct Progress {
            int readchapters;
            float progresspercentage;
        };
        std::string name;
        std::string authorname;
        std::string coverpath;
        std::string synopsis;
        int totalchapters;        // Total chapters available online
        int downloadedchapters;   // Actually downloaded chapters
        Progress progress;
    };

    enum class UIState {
        LIBRARY,     // Show library with tabs (Library/Downloads)
        READING      // Full-screen reading mode
    };

    // Download Manager structures
    struct DownloadSource {
        std::string name;
        std::string baseUrl;
        std::string searchEndpoint;
        std::string pythonScript;
        bool enabled;

        // Default constructor
        DownloadSource() : enabled(false) {}

        // Constructor with parameters
        DownloadSource(const std::string& n, const std::string& url, const std::string& endpoint,
            const std::string& script, bool en)
            : name(n), baseUrl(url), searchEndpoint(endpoint), pythonScript(script), enabled(en) {
        }
    };


    enum class ContentType {
        ALL,
        NOVEL,
        MANGA,
        MANHWA,
        MANHUA
    };
    struct DownloadTask {
        std::string downloadId;      // Add this field
        std::string novelName;
        std::string author;
        std::string sourceUrl;
        std::string sourceName;
        int startChapter;
        int endChapter; // -1 for all available
        int currentChapter;
        int totalChapters;
        bool isActive;
        bool isPaused;
        bool isComplete;
        std::string status;
        float progress;
        std::string lastError;       // Add this field
        ContentType contentType;     // Add this field

        // Default constructor
        DownloadTask() : startChapter(1), endChapter(-1), currentChapter(0), totalChapters(0),
            isActive(false), isPaused(false), isComplete(false), progress(0.0f),
            contentType(ContentType::NOVEL) {
            downloadId = ""; // Will be generated when needed
        }
    };



    struct SearchResult {
        std::string title;
        std::string author;
        std::string url;
        std::string sourceName;
        int totalChapters;
        std::string description;
        std::string coverUrl;

        // Default constructor
        SearchResult() : totalChapters(0) {}
    };

    UIState currentState = UIState::LIBRARY;

private:
    struct CoverTexture {
        VkDescriptorSet descriptorSet;
        VkImage image;
        VkImageView imageView;
        VkDeviceMemory imageMemory;
        int width;
        int height;
        bool loaded;
        CoverTexture() : descriptorSet(VK_NULL_HANDLE), image(VK_NULL_HANDLE),
            imageView(VK_NULL_HANDLE), imageMemory(VK_NULL_HANDLE),
            width(0), height(0), loaded(false) {
        }
    };

    // UI Font Management
    struct UIFonts {
        ImFont* normalFont = nullptr;
        ImFont* largeFont = nullptr;
        ImFont* smallFont = nullptr;
        ImFont* titleFont = nullptr;
        bool initialized = false;

        // Corrected FontAwesome glyph ranges for FontAwesome 6
        static const ImWchar* GetFontAwesomeRanges() {
            static const ImWchar ranges[] = {
                0xf000, 0xf8ff, // Extended FontAwesome 6 range
                0,
            };
            return ranges;
        }
    };

    ImGuiApp::Application* app = nullptr;
    std::string currentNovelName = "";
    int targetChapter = 1;
    UIFonts uiFonts;

    // Download Manager
    std::vector<DownloadSource> downloadSources;
    std::vector<DownloadTask> downloadQueue;
    std::vector<SearchResult> searchResults;
    std::string searchQuery = "";
    bool isSearching = false;
    std::unique_ptr<std::thread> downloadThread;
    bool downloadManagerRunning = false;

    // Member variables
    std::vector<Novel> novellist;
    std::unordered_map<std::string, CoverTexture> coverTextures;
    int selectedNovelIndex = -1;
    bool showInfoPanel = false;
    VkSampler textureSampler = VK_NULL_HANDLE;
    ErrorHandler LibraryErrors;
    ChapterManager chaptermanager;

    bool pendingFontUpdate = false;

    // UI State
    int currentLibraryTab = 0; // 0=Library, 1=Downloads
    bool showGrid = true; // true=grid view, false=list view


    struct ActiveDownload {
        std::string novelName;
        std::string novelDir;
        bool isActive;
        std::shared_ptr<std::thread> thread;

        ActiveDownload() : isActive(false) {}
    };

    bool shouldTerminateDownload;
    bool shouldTerminateDownloads;
    int activeDownloadCount;
    std::vector<ActiveDownload> activeDownloads;

public:
    Library(ImGuiApp::Application* application);
    ~Library();

    // ============================================================================
    // Core Library Functions
    // ============================================================================
    void Render();
    bool SaveNovels(const std::vector<Novel>& novels);
    bool AddNovel(const Novel& novel);
    bool RemoveNovel(const std::string& novelName, const std::string& authorName);
    void LoadAllNovelsFromFile();
    void SwitchToReading(const std::string& novelName, int chapter);
    void SwitchToLibrary();
    void RefreshNovelChapterCounts();

    void LoadFontSizesWithFontAwesome(const char* path);
    void LoadDefaultFontsWithFontAwesome();
    void LoadUIFontWithFontAwesome(const char* path, float size, ImFont** target);
    void MergeFontAwesome(float size);

    // UI State accessors
    bool isInLibrary() const;
    bool isInReading() const;
    std::string GetCurrentNovelName() const;
    int GetTargetChapter() const { return targetChapter; }

    void ProcessPendingFontUpdate();
    std::atomic<bool> fontUpdateInProgress{ false };
    bool fontTextureNeedsRebuild = false;

    // ============================================================================
    // Main UI Rendering
    // ============================================================================
    void RenderLibraryInterface();
    void RenderMainTabs();
    void PrepareUIState();
    void RestoreUIState();
    void RenderLibraryView();
    void RenderSplitView(const ImVec2& windowSize);
    void RenderFullScreenReading();

    void MarkNovelAsRead(const std::string& novelName);
    void UpdateReadingProgress(const std::string& novelName, int chapterNumber);
    void CheckAndDownloadLatestChapters(const Novel& novel);

    std::mutex activeDownloadsMutex;

    // ============================================================================
    // Novel Grid/List Rendering
    // ============================================================================
    void RenderNovelGrid();
    void RenderGridHeader();
    void RenderNovelGridView();
    void RenderNovelListView();
    int CalculateGridColumns(float availableWidth);

    // Novel Card Rendering
    void RenderNovelCard(const Novel& novel, int index);
    void RenderCardBackground(const ImVec2& start, const ImVec2& end, bool isSelected);
    void RenderCardContent(const Novel& novel, const ImVec2& cardStart, bool isSelected);
    void RenderCardCover(const Novel& novel, const ImVec2& cardStart);
    void RenderCardInfo(const Novel& novel, const ImVec2& cardStart);
    void RenderValidCoverImage(VkDescriptorSet texture, const std::string& coverPath,
        const ImVec2& cardStart, float coverAreaWidth);
    void RenderPlaceholderCover(const ImVec2& cardStart);

    // Table Rendering
    void SetupTableColumns();
    void RenderTableRow(const Novel& novel, int index);
    void RenderProgressBar(float percentage);

    // ============================================================================
    // Info Panel
    // ============================================================================
    void RenderInfoPanel();
    void RenderInfoPanelHeader();
    void RenderInfoPanelContent(const Novel& novel);
    void RenderInfoPanelCover(const Novel& novel);
    void RenderInfoPanelCoverImage(VkDescriptorSet texture, const std::string& coverPath,
        const ImVec2& coverStart);
    void RenderInfoPanelPlaceholder(const ImVec2& coverStart);
    void RenderInfoPanelDetails(const Novel& novel, float detailsWidth);
    void RenderStatsArea(const Novel& novel, float detailsWidth);
    void RenderActionButtons(const Novel& novel, float detailsWidth);
    void RenderSmallActionButtons(const Novel& novel, float buttonWidth);
    void RenderSynopsisSection(const Novel& novel);
    void RenderChapterOverview(const Novel& novel);
    void RenderChapterButton(const Novel& novel, int chapterNum, float buttonWidth, float buttonHeight);
    std::tuple<ImVec4, ImVec4, ImVec4, ImVec4> GetChapterButtonColors(bool isRead, bool isCurrentChapter);
    void RenderChapterTooltip(int chapterNum, bool isRead, bool isCurrentChapter);

    void CleanupFonts();
    void ReinitializeFonts();
    void OnReadingSettingsChanged();

    // ============================================================================
    // Reading View
    // ============================================================================
    float RenderReadingMenuBar();
    void RenderChapterInfo();
    void RenderNavigationControls();
    void RenderReadingContent(float menuBarHeight);

    // ============================================================================
    // Download Manager
    // ============================================================================
    void RenderDownloadManager();
    void InitializeDownloadSources();
    bool IsFullUrl(const std::string& input);
    void LoadDownloadSources();
    void SaveDownloadSources();
    void CreateDefaultDownloadSources();

    // Search functionality
    bool SearchNovels(const std::string& query);
    bool ParseSearchResults(const std::string& output);
    void RenderSearchTab();
    void RenderSearchInput();
    void RenderSearchResults();
    void RenderSearchResultCard(const SearchResult& result, size_t index);
    void RenderResultCardContent(const SearchResult& result);
    void RenderDownloadOptions(const SearchResult& result, size_t index);

    // Download management
    void StartDownload(const SearchResult& result, int startChapter = 1, int endChapter = -1);
    DownloadTask CreateDownloadTask(const SearchResult& result, int startChapter, int endChapter);
    void StartDownloadManager();
    void StopDownloadManager();
    void ProcessDownloadQueue();
    bool ProcessNextDownload();
    bool ExecuteDownloadTask(DownloadTask& task);
    std::vector<std::string> BuildDownloadArgs(const DownloadTask& task);
    bool HasQueuedDownloads();
    void PauseDownload(const std::string& downloadId);
    void ResumeDownload(const std::string& downloadId);
    void CancelDownload(const std::string& downloadId);
    bool IsValidTaskIndex(int taskIndex);

    // Download Queue UI
    void RenderDownloadQueue();
    void RenderDownloadTable();
    void SetupDownloadTableColumns();
    void RenderDownloadTableRow(const DownloadTask& task, size_t index);
    void RenderTaskProgress(const DownloadTask& task);
    void RenderTaskStatus(const DownloadTask& task);
    void RenderTaskChapterRange(const DownloadTask& task);
    void RenderTaskActions(const DownloadTask& task, size_t index);

    // Sources management
    void RenderSourcesTab();
    void RenderSourcesTable();
    void SetupSourcesTableColumns();
    void RenderSourcesTableRow(DownloadSource& source, size_t index);
    void RenderSourcesManagement();
    void AddNewDownloadSource();

    void ParseProgressLine(const std::string& line, DownloadTask& task);

    void CleanupStopSignals();

    bool CallPythonScriptAsync(const std::string& scriptName, const std::vector<std::string>& args,
        std::function<void(const std::string&)> progressCallback,
        std::function<void(bool, const std::string&)> completionCallback);

    struct DownloadProgress {
        int current = 0;
        int total = 0;
        float percentage = 0.0f;
        std::string chapterTitle;
        std::string novelTitle;  // Add novel title to identify which download
        bool isActive = false;
        bool isComplete = false;
        bool hasError = false;
        std::string errorMessage;

        void reset() {
            current = 0;
            total = 0;
            percentage = 0.0f;
            chapterTitle.clear();
            novelTitle.clear();
            isActive = false;
            isComplete = false;
            hasError = false;
            errorMessage.clear();
        }
    };

    static const int MAX_CONCURRENT_DOWNLOADS = 3;
    std::array<DownloadProgress, MAX_CONCURRENT_DOWNLOADS> downloadProgresses;


    void SaveDownloadStates();
    void LoadDownloadStates();

    struct ContentItem {
        std::string name;
        std::string authorname;
        std::string coverpath;
        std::string synopsis;
        ContentType type;
        int totalchapters;
        int downloadedchapters;
        DownloadProgress progress;
        std::string sourceName;
        std::string sourceUrl;

        // Manga-specific data
        struct ChapterProvider {
            std::string name;
            std::string language;
            std::string url;
            std::string uploadDate;
        };
        std::vector<ChapterProvider> providers;
    };

    struct SearchFilter {
        ContentType contentType = ContentType::ALL;
        std::string language = "";
        bool showAdult = false;
        int maxResults = 2;
    };

    struct DownloadState {
        std::string id;
        std::string contentName;
        ContentType type;
        int currentChapter;
        int totalChapters;
        bool isPaused;
        bool isComplete;
        float progress;
        std::string lastError;
        std::chrono::system_clock::time_point lastUpdate;
    };

    void SaveAllReadingPositions();

    int FindAvailableDownloadSlot();
    int FindDownloadSlotByTitle(const std::string& novelTitle);

    void RenderSearchFilters();
    bool SearchContentWithFilters(const std::string& query, const SearchFilter& filter);
    void RenderContentTypeFilter(SearchFilter& filter);
    void RenderLanguageFilter(SearchFilter& filter);
    void RenderDownloadStateIndicator(const DownloadTask& task);

    void SaveReadingPosition(const std::string& contentName, ContentType type,
        int chapter, float scrollPos = 0.0f, int page = 0);

    void LoadAllReadingPositions();

    void SwitchToMangaReading(const std::string& mangaName, int chapter = 1, int page = 0);
    void RenderMangaReader();
    void LoadMangaChapter(const std::string& mangaName, int chapter);
    void NavigateMangaPage(int direction); // -1 for prev, 1 for next
    VkDescriptorSet LoadMangaPage(const std::string& imagePath);

    std::string GenerateDownloadId(const std::string& contentName, ContentType type);
    void UpdateDownloadState(const std::string& downloadId, const DownloadState& state);

    std::string ContentTypeToString(ContentType type);
    ContentType StringToContentType(const std::string& typeStr);


    struct ProcessInfo {
        std::shared_ptr<std::thread> thread;
        std::atomic<bool> shouldStop;
        std::atomic<bool> shouldTerminate;
        std::string contentName;
        ContentType contentType;

        // Default constructor
        ProcessInfo() : shouldStop(false), shouldTerminate(false), contentType(ContentType::NOVEL) {}

        // Move constructor
        ProcessInfo(ProcessInfo&& other) noexcept
            : thread(std::move(other.thread))
            , shouldStop(other.shouldStop.load())
            , shouldTerminate(other.shouldTerminate.load())
            , contentName(std::move(other.contentName))
            , contentType(other.contentType) {
        }

        // Move assignment operator
        ProcessInfo& operator=(ProcessInfo&& other) noexcept {
            if (this != &other) {
                // Clean up existing thread properly
                if (thread && thread->joinable()) {
                    shouldTerminate.store(true);
                    thread->join();
                }

                thread = std::move(other.thread);
                shouldStop.store(other.shouldStop.load());
                shouldTerminate.store(other.shouldTerminate.load());
                contentName = std::move(other.contentName);
                contentType = other.contentType;
            }
            return *this;
        }

        // Destructor
        ~ProcessInfo() {
            if (thread && thread->joinable()) {
                shouldTerminate.store(true);
                thread->join();  // Wait for thread to finish
            }
        }

        // Delete copy constructor and copy assignment
        ProcessInfo(const ProcessInfo&) = delete;
        ProcessInfo& operator=(const ProcessInfo&) = delete;
    };

    std::vector<DownloadState> persistentDownloadStates;
    std::mutex downloadStateMutex;
    std::unordered_map<std::string, ProcessInfo> activeProcesses;


    // Replace the old Novel with ContentItem
    std::vector<ContentItem> contentLibrary;  // Replace novellist

    // Reading progress tracking
    struct ReadingProgress {
        std::string contentName;
        ContentType type;
        int currentChapter;
        float scrollPosition;
        int currentPage;  // For manga
        std::time_t lastRead;
    };
    std::unordered_map<std::string, ReadingProgress> readingProgressMap;


    struct ReadingPosition {
        std::string contentName;
        ContentType type;
        int currentChapter;
        float scrollPosition;
        int currentPage; // For manga
        std::time_t lastRead;

        ReadingPosition() : type(ContentType::NOVEL), currentChapter(1),
            scrollPosition(0.0f), currentPage(0), lastRead(0) {
        }
    };

    // Content management
    bool AddContent(const ContentItem& content);
    bool RemoveContent(const std::string& contentName, const std::string& authorName);
    void LoadAllContentFromFile();
    bool SaveContent(const std::vector<ContentItem>& content);
    void QueueDownloadResume(const DownloadState& state);
    void CleanupPartialDownload(const std::string& downloadId, const std::string& contentName, ContentType type);
    void SaveAllReadingProgress();

    // ============================================================================
    // Font Management
    // ============================================================================
    void InitializeUIFonts();
    const char* FindSystemFont();
    void LoadFontSizes(const char* path);
    void LoadDefaultFonts();
    void LoadUIFont(const char* path, float size, ImFont** target);

    // ============================================================================
    // Texture Management
    // ============================================================================
    VkDescriptorSet LoadCoverTexture(const std::string& imagePath);
    VkDescriptorSet CreateTextureFromPixels(stbi_uc* pixels, int width, int height,
        const std::string& imagePath);
    VkDescriptorSet GetCoverTexture(const std::string& coverPath);
    void CleanupTextures();
    void CleanupCoverTexture(CoverTexture& texture);
    VkSampler GetOrCreateTextureSampler();
    void CleanupTextureSampler();

    // ============================================================================
    // Vulkan Helper Functions
    // ============================================================================
    uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
        VkBuffer& buffer, VkDeviceMemory& bufferMemory);
    VkCommandBuffer CreateOneTimeCommandBuffer();
    void SubmitOneTimeCommandBuffer(VkCommandBuffer commandBuffer);

    // Vulkan image operations
    void CreateStagingBuffer(VkDeviceSize imageSize, stbi_uc* pixels,
        VkBuffer& stagingBuffer, VkDeviceMemory& stagingBufferMemory);
    CoverTexture CreateVulkanImage(int width, int height);
    void AllocateImageMemory(CoverTexture& texture);
    void CopyImageData(VkBuffer stagingBuffer, CoverTexture& texture, int width, int height);
    void CreateImageView(CoverTexture& texture);
    void CreateDescriptorSet(CoverTexture& texture);
    void CopyBufferToImage(VkCommandBuffer commandBuffer, VkBuffer buffer, VkImage image,
        uint32_t width, uint32_t height);
    void TransitionImageLayout(VkCommandBuffer commandBuffer, VkImage image, VkFormat format,
        VkImageLayout oldLayout, VkImageLayout newLayout);

    // ============================================================================
    // File Management
    // ============================================================================
    void CheckNovelsDirectory();
    void CheckNovelFolderStructure(const std::string& novelName);
    int CountChaptersInDirectory(const std::string& novelName);

    // ============================================================================
    // Python Integration
    // ============================================================================
    bool CallPythonScript(const std::string& scriptName, const std::vector<std::string>& args,
        std::string& output);

    ReadingPosition LoadReadingPosition(const std::string& contentName);


private:

    std::unordered_map<std::string, ReadingPosition> readingPositions;
    SearchFilter currentSearchFilter;

    struct MangaViewer {
        std::string mangaName;
        int currentChapter;
        int currentPage;
        int totalPages;
        std::vector<std::string> pageFiles;
        VkDescriptorSet currentPageTexture = VK_NULL_HANDLE;
        bool isLoading = false;
    };
    MangaViewer mangaViewer;

};