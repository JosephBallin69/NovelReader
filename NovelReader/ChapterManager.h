#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include "ImGui/imgui.h"

class Library;

class ChapterManager {
public:
    struct Chapter {
        int chapterNumber;
        std::string title;
        std::string content;
        Chapter() : chapterNumber(0) {}
    };

    // Font management
    struct FontInfo {
        ImFont* font;
        std::string name;
        std::string path;
        float size;
        FontInfo() : font(nullptr), size(16.0f) {}
        FontInfo(ImFont* f, const std::string& n, const std::string& p, float s)
            : font(f), name(n), path(p), size(s) {
        }
    };

    struct ReadingSettings {
        // Typography
        float fontSize = 32.0f;
        float lineSpacing = 1.4f;
        int fontFamily = 0; // Index into available fonts
        int textAlignment = 0; // 0=Left, 1=Center, 2=Justify
        int readingWidth = 0; // 0=Narrow(55%), 1=Medium(75%), 2=Wide(90%)

        // Visual theme - IMPROVED DEFAULTS
        bool darkTheme = true;
        bool customBackground = true; // Enable by default
        ImVec4 backgroundColor = ImVec4(0.1059f, 0.1059f, 0.1059f, 1.0f); // Changed to match better with main background
        ImVec4 textColor = ImVec4(0.92f, 0.92f, 0.94f, 1.0f); // Slightly warm white
        ImVec4 headerColor = ImVec4(0.92f, 0.92f, 0.94f, 1.0f); // Light blue headers

        // Reading experience
        bool showScrollbar = true;
        float marginSize = 25.0f; // Increased default margin
        bool smoothScrolling = true;

        // Reading progress
        float scrollPosition = 0.0f;
        int currentChapter = 1;

        // Font sizes for different elements
        float headerFontScale = 1.8f; // Increased for better hierarchy
        float header2FontScale = 1.5f;
        float header3FontScale = 1.25f;
    };

public:
    ChapterManager();
    ~ChapterManager();

    void SetLibraryPointer(Library* lib) { libraryPtr = lib; }

    // Core functionality
    void RenderContentOnly();
    bool LoadChapter(const std::string& filePath);
    bool SaveChapter(const Chapter& chapter, const std::string& novelName);
    void ParseMarkdownContent();
    void ParseInlineFormatting(const std::string& line);
    void RenderContent();
    void RenderSettingsPanel();
    void Render();
    void OpenChapter(int chapterNumber);
    void SetNovelTitle(const std::string& title);
    void LoadChaptersFromDirectory(const std::string& novelName);

    void RenderEnhancedSettingsPanel();
    void RenderTypographyTab();
    void RenderAppearanceTab();
    void RenderLayoutTab();
    void RenderNavigationTab();


    // Font management
    bool InitializeFonts();
    void LoadFont(const std::string& name, const std::string& path, float size);
    void LoadDefaultFonts();
    void LoadReadingFonts(); // Load fonts at reading size
    ImFont* GetReadingFont();
    ImFont* GetReadingFontAtSize(float size);
    ImFont* GetHeaderFont();
    ImFont* GetMenuFont();
    void ReloadFonts();
    void UpdateReadingFonts(); // Update fonts when size changes

    // Settings
    void SaveSettings();
    void LoadSettings();
    void ToggleSettings() { showSettings = !showSettings; }

private:

    struct TextElement {
        enum Type { TEXT, BOLD, ITALIC, HEADER1, HEADER2, HEADER3, PARAGRAPH_BREAK, LINE_BREAK };
        Type type;
        std::string text;
        TextElement(Type t, const std::string& txt) : type(t), text(txt) {}
    };

private:
    // Core data
    ReadingSettings settings;
    std::vector<TextElement> parsedContent;
    std::string novelTitle = "Novel Title";
    bool showSettings = false;
    bool contentNeedsReparsing = true;
    std::vector<Chapter> chapters;

    Library* libraryPtr = nullptr; // Add this member variable

    // Font management
    std::unordered_map<std::string, FontInfo> fonts;
    std::unordered_map<std::string, ImFont*> readingFonts; // Fonts at reading sizes
    std::vector<std::string> availableFontNames;
    bool fontsInitialized = false;
    bool fontsNeedReload = false;
    float lastFontSize = 18.0f; // Track font size changes

    // UI helpers
    void RenderTypographySettings();
    void RenderVisualSettings();
    void RenderReadingSettings();
    void RenderFontSettings();
    void ApplyTheme();
    float GetWidthMultiplier();

public:
    ReadingSettings& getSettings() { return settings; }
    const std::vector<Chapter>& getChapters() const { return chapters; }
    const std::vector<std::string>& getAvailableFonts() const { return availableFontNames; }
};