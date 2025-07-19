#include "ChapterManager.h"
#include "Library.h"
#include "Dependecies/json.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include "ImGui/imgui.h"
#include "Dependecies/FontAwesome.h"

using json = nlohmann::json;

ChapterManager::ChapterManager() {
    LoadSettings();
    InitializeFonts();
    lastFontSize = settings.fontSize;
}

ChapterManager::~ChapterManager() {
    SaveSettings();
}

// JSON serialization for settings
void to_json(json& j, const ChapterManager::ReadingSettings& s) {
    j = json{
        {"fontSize", s.fontSize},
        {"lineSpacing", s.lineSpacing},
        {"fontFamily", s.fontFamily},
        {"textAlignment", s.textAlignment},
        {"readingWidth", s.readingWidth},
        {"darkTheme", s.darkTheme},
        {"customBackground", s.customBackground},
        {"backgroundColor", {s.backgroundColor.x, s.backgroundColor.y, s.backgroundColor.z, s.backgroundColor.w}},
        {"textColor", {s.textColor.x, s.textColor.y, s.textColor.z, s.textColor.w}},
        {"headerColor", {s.headerColor.x, s.headerColor.y, s.headerColor.z, s.headerColor.w}},
        {"showScrollbar", s.showScrollbar},
        {"marginSize", s.marginSize},
        {"smoothScrolling", s.smoothScrolling},
        {"headerFontScale", s.headerFontScale},
        {"header2FontScale", s.header2FontScale},
        {"header3FontScale", s.header3FontScale}
    };
}

void from_json(const json& j, ChapterManager::ReadingSettings& s) {
    j.at("fontSize").get_to(s.fontSize);
    j.at("lineSpacing").get_to(s.lineSpacing);
    j.at("fontFamily").get_to(s.fontFamily);
    j.at("textAlignment").get_to(s.textAlignment);
    j.at("readingWidth").get_to(s.readingWidth);
    j.at("darkTheme").get_to(s.darkTheme);
    j.at("customBackground").get_to(s.customBackground);

    auto bg = j.at("backgroundColor");
    s.backgroundColor = ImVec4(bg[0], bg[1], bg[2], bg[3]);

    auto tc = j.at("textColor");
    s.textColor = ImVec4(tc[0], tc[1], tc[2], tc[3]);

    auto hc = j.at("headerColor");
    s.headerColor = ImVec4(hc[0], hc[1], hc[2], hc[3]);

    j.at("showScrollbar").get_to(s.showScrollbar);
    j.at("marginSize").get_to(s.marginSize);
    j.at("smoothScrolling").get_to(s.smoothScrolling);
    j.at("headerFontScale").get_to(s.headerFontScale);
    j.at("header2FontScale").get_to(s.header2FontScale);
    j.at("header3FontScale").get_to(s.header3FontScale);
}

// Chapter serialization (existing)
void to_json(json& j, const ChapterManager::Chapter& c) {
    j = json{
        {"chapterNumber", c.chapterNumber},
        {"title", c.title},
        {"content", c.content}
    };
}

void from_json(const json& j, ChapterManager::Chapter& c) {
    j.at("chapterNumber").get_to(c.chapterNumber);
    j.at("title").get_to(c.title);
    j.at("content").get_to(c.content);
}

// Font Management
bool ChapterManager::InitializeFonts() {
    if (fontsInitialized) return true;

    try {
        LoadDefaultFonts(); // Just populates font names
        fontsInitialized = true;
        return true;
    }
    catch (const std::exception& e) {
        std::cout << "Error initializing ChapterManager fonts: " << e.what() << std::endl;
        return false;
    }
}

void ChapterManager::LoadDefaultFonts() {
    // Don't load fonts here - let Library handle all font management
    // Just populate the available font names for the UI
    availableFontNames.clear();
    availableFontNames.push_back("Default");

    // Add other font names without actually loading them
    std::vector<std::pair<std::string, std::string>> fontPaths = {
        {"Segoe UI", "C:/Windows/Fonts/segoeui.ttf"},
        {"Arial", "C:/Windows/Fonts/arial.ttf"},
        {"Times New Roman", "C:/Windows/Fonts/times.ttf"},
        {"Georgia", "C:/Windows/Fonts/georgia.ttf"},
        {"Verdana", "C:/Windows/Fonts/verdana.ttf"}
    };

    for (const auto& [name, path] : fontPaths) {
        if (std::filesystem::exists(path)) {
            availableFontNames.push_back(name);
        }
    }

    std::cout << "ChapterManager: Available font names populated" << std::endl;
}

void ChapterManager::LoadFont(const std::string& name, const std::string& path, float size) {
    ImGuiIO& io = ImGui::GetIO();

    try {
        ImFont* font = io.Fonts->AddFontFromFileTTF(path.c_str(), size);
        if (font) {
            fonts[name] = FontInfo(font, name, path, size);
            if (std::find(availableFontNames.begin(), availableFontNames.end(), name) == availableFontNames.end()) {
                availableFontNames.push_back(name);
            }
            std::cout << "Loaded font: " << name << " from " << path << std::endl;
        }
    }
    catch (const std::exception& e) {
        std::cout << "Failed to load font " << name << ": " << e.what() << std::endl;
    }
}

void ChapterManager::LoadReadingFonts() {
    fontsNeedReload = true;
    std::cout << "Marked reading fonts for reload" << std::endl;
}

void ChapterManager::UpdateReadingFonts() {

}

ImFont* ChapterManager::GetReadingFont() {
    // Return nullptr to use default ImGui font and avoid conflicts with Library's font system
    return nullptr;
}

ImFont* ChapterManager::GetReadingFontAtSize(float size) {
    // Return nullptr to use default ImGui font
    return nullptr;
}

ImFont* ChapterManager::GetHeaderFont() {
    // Return nullptr to use default ImGui font
    return nullptr;
}

ImFont* ChapterManager::GetMenuFont() {
    // Return nullptr to use default ImGui font
    return nullptr;
}

void ChapterManager::ReloadFonts() {
    // Don't clear the entire font atlas - let Library handle this
    std::cout << "Font reload requested - notifying Library" << std::endl;

    if (libraryPtr) {
        libraryPtr->OnReadingSettingsChanged();
    }

    fontsNeedReload = false;
}

// Settings Management
void ChapterManager::SaveSettings() {
    try {
        std::filesystem::create_directories("settings");

        json j;
        j["readingSettings"] = settings;

        std::ofstream file("settings/reading_settings.json");
        if (file.is_open()) {
            file << j.dump(4);
            file.close();
            std::cout << "Reading settings saved" << std::endl;
        }
    }
    catch (const std::exception& e) {
        std::cout << "Error saving settings: " << e.what() << std::endl;
    }
}

void ChapterManager::LoadSettings() {
    try {
        std::ifstream file("settings/reading_settings.json");
        if (file.is_open()) {
            json j;
            file >> j;
            file.close();

            if (j.contains("readingSettings")) {
                settings = j["readingSettings"].get<ReadingSettings>();
                std::cout << "Reading settings loaded" << std::endl;
            }
        }
    }
    catch (const std::exception& e) {
        std::cout << "Could not load settings, using defaults: " << e.what() << std::endl;
    }
}

float ChapterManager::GetWidthMultiplier() {
    switch (settings.readingWidth) {
    case 0: return 0.45f;  // Narrow
    case 1: return 0.65f;  // Medium
    case 2: return 1.0f;  // Wide
    default: return 0.90f;
    }
}

void ChapterManager::ApplyTheme() {
    if (settings.customBackground) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, settings.backgroundColor);
    }
}

// Enhanced Settings Panel
void ChapterManager::RenderSettingsPanel() {
    if (!showSettings) return;

    ImGui::SetNextWindowSize(ImVec2(450, 600), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(50, 50), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Reading Settings", &showSettings, ImGuiWindowFlags_AlwaysVerticalScrollbar)) {

        if (ImGui::BeginTabBar("SettingsTabs")) {

            // Typography Tab
            if (ImGui::BeginTabItem("Typography")) {
                RenderTypographySettings();
                ImGui::EndTabItem();
            }

            // Visual Tab
            if (ImGui::BeginTabItem("Visual")) {
                RenderVisualSettings();
                ImGui::EndTabItem();
            }

            // Reading Tab
            if (ImGui::BeginTabItem("Reading")) {
                RenderReadingSettings();
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        ImGui::Separator();

        // Action buttons
        if (ImGui::Button("Save Settings", ImVec2(100, 0))) {
            SaveSettings();
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset to Defaults", ImVec2(120, 0))) {
            settings = ReadingSettings(); // Reset to defaults
            contentNeedsReparsing = true;
        }
    }
    ImGui::End();
}

void ChapterManager::RenderTypographySettings() {
    bool needsReparse = false;

    ImGui::Text("%s Font Configuration", ICON_FA_FONT);
    ImGui::Separator();

    // Font family - visual only, no actual font changes
    if (!availableFontNames.empty()) {
        int currentFont = settings.fontFamily;
        if (currentFont >= availableFontNames.size()) currentFont = 0;

        if (ImGui::Combo("Font Family", &currentFont, [](void* data, int idx, const char** out_text) {
            auto* names = static_cast<std::vector<std::string>*>(data);
            if (idx >= 0 && idx < names->size()) {
                *out_text = (*names)[idx].c_str();
                return true;
            }
            return false;
            }, &availableFontNames, (int)availableFontNames.size())) {

            settings.fontFamily = currentFont;
            needsReparse = true;
            // NO font rebuilding - just visual change
        }
    }

    // Font size - affects scaling only
    if (ImGui::SliderFloat("Font Size", &settings.fontSize, 10.0f, 32.0f, "%.1f px")) {
        needsReparse = true;
        // NO font rebuilding - just scaling
    }

    // Line spacing
    if (ImGui::SliderFloat("Line Spacing", &settings.lineSpacing, 0.8f, 3.0f, "%.1f")) {
        needsReparse = true;
    }

    if (needsReparse) {
        contentNeedsReparsing = true;
    }
}

void ChapterManager::RenderVisualSettings() {
    bool needsReparse = false;

    ImGui::Text("Theme Settings");
    ImGui::Separator();

    if (ImGui::Checkbox("Dark Theme", &settings.darkTheme)) {
        needsReparse = true;
    }

    if (ImGui::Checkbox("Custom Reading Area Background", &settings.customBackground)) {
        needsReparse = true;
    }

    if (settings.customBackground) {
        ImGui::Indent();
        ImGui::Text("Reading area background color:");
        ImGui::Text("(Only colors the text area, not the full window)");
        if (ImGui::ColorEdit4("Reading Background", (float*)&settings.backgroundColor)) {
            needsReparse = true;
        }
        ImGui::Unindent();
    }

    ImGui::Spacing();
    ImGui::Text("Text Colors");
    ImGui::Separator();

    if (ImGui::ColorEdit3("Text Color", (float*)&settings.textColor)) {
        needsReparse = true;
    }

    if (ImGui::ColorEdit3("Header Color", (float*)&settings.headerColor)) {
        needsReparse = true;
    }

    ImGui::Spacing();
    ImGui::Text("Preview:");
    ImGui::Separator();

    // Show a preview of the color scheme
    ImGui::PushStyleColor(ImGuiCol_Text, settings.textColor);
    ImGui::TextWrapped("This is sample body text with current settings.");
    ImGui::PopStyleColor();

    ImGui::PushStyleColor(ImGuiCol_Text, settings.headerColor);
    ImGui::Text("Sample Header Text");
    ImGui::PopStyleColor();

    if (settings.customBackground) {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Reading area will have the custom background color");
    }

    if (needsReparse) {
        contentNeedsReparsing = true;
    }
}

void ChapterManager::RenderReadingSettings() {
    bool needsReparse = false;

    ImGui::Text("Layout Settings");
    ImGui::Separator();

    // Reading width
    const char* widths[] = { "Narrow (45%)", "Medium (65%)", "Wide (100%)" };
    if (ImGui::Combo("Reading Width", &settings.readingWidth, widths, 3)) {
        needsReparse = true;
    }

    // Text alignment
    const char* alignments[] = { "Left", "Center", "Justify" };
    ImGui::Combo("Text Alignment", &settings.textAlignment, alignments, 3);

    // Margin size
    if (ImGui::SliderFloat("Margin Size", &settings.marginSize, 10.0f, 50.0f, "%.0f px")) {
        needsReparse = true;
    }

    ImGui::Spacing();
    ImGui::Text("Reading Experience");
    ImGui::Separator();

    ImGui::Checkbox("Show Scrollbar", &settings.showScrollbar);
    ImGui::Checkbox("Smooth Scrolling", &settings.smoothScrolling);

    ImGui::Spacing();
    ImGui::Text("Chapter Navigation");
    ImGui::Separator();

    if (ImGui::Button("◄ Previous") && settings.currentChapter > 1) {
        OpenChapter(settings.currentChapter - 1);
    }

    ImGui::SameLine();
    ImGui::Text("%d / %zu", settings.currentChapter, chapters.size());

    ImGui::SameLine();
    if (ImGui::Button("Next ►") && settings.currentChapter < (int)chapters.size()) {
        OpenChapter(settings.currentChapter + 1);
    }

    if (needsReparse) {
        contentNeedsReparsing = true;
    }
}

// Enhanced Content Rendering with Settings
void ChapterManager::RenderContentOnly() {
    bool resetscroll = false;
    if (contentNeedsReparsing) {
        resetscroll = true;
    }


    ParseMarkdownContent();

    if (chapters.empty()) {
        ImVec2 center = ImVec2(ImGui::GetContentRegionAvail().x * 0.5f, ImGui::GetContentRegionAvail().y * 0.5f);
        ImGui::SetCursorPos(center);
        ImGui::Text("No chapter loaded");
        return;
    }

    // Calculate font scale based on settings (using Library's normal font size as base)
    float fontScale = settings.fontSize / 18.0f; // 18.0f is your Library normal font size

    // Apply theme colors
    ImVec4 textColor = settings.darkTheme ? settings.textColor : ImVec4(0.1f, 0.1f, 0.1f, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, textColor);

    ImVec2 availableSize = ImGui::GetContentRegionAvail();

    // Create scrollable content area
    ImGuiWindowFlags childFlags = ImGuiWindowFlags_None;
    if (!settings.showScrollbar) {
        childFlags |= ImGuiWindowFlags_NoScrollbar;
    }

    ImGui::BeginChild("ReadingContent", availableSize, false, childFlags);

    // Apply font scaling to the entire child window
    ImGui::SetWindowFontScale(fontScale);
    
    if (resetscroll) {
        ImGui::SetScrollHereY(settings.scrollPosition);
    }
    

    // Calculate reading width based on settings
    float fullWidth = ImGui::GetContentRegionAvail().x;
    float readingWidth = fullWidth * GetWidthMultiplier();
    float leftMargin = (fullWidth - readingWidth) * 0.5f;

    // Use columns for layout
    ImGui::Columns(3, "ReadingLayout", false);
    ImGui::SetColumnWidth(0, leftMargin);
    ImGui::SetColumnWidth(1, readingWidth);
    ImGui::SetColumnWidth(2, leftMargin);

    // Move to content column
    ImGui::NextColumn();

    // Apply custom background to the reading area ONLY
    if (settings.customBackground) {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 contentStart = ImGui::GetCursorScreenPos();
        contentStart.x -= 10.0f;
        ImVec2 contentEnd = ImVec2(contentStart.x + readingWidth + 20.0f, contentStart.y + availableSize.y);
        drawList->AddRectFilled(contentStart, contentEnd, ImGui::ColorConvertFloat4ToU32(settings.backgroundColor));
    }

    // Apply line spacing via ItemSpacing
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, settings.lineSpacing * 4.0f));

    // Add top padding
    ImGui::Dummy(ImVec2(0, settings.marginSize));

    // Render content with font scaling (no font switching)
    for (size_t i = 0; i < parsedContent.size(); i++) {
        const auto& element = parsedContent[i];

        switch (element.type) {
        case TextElement::HEADER1:
        {
            ImGui::Dummy(ImVec2(0, settings.lineSpacing * 15.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, settings.headerColor);

            // Use temporary font scaling for headers
            ImGui::SetWindowFontScale(fontScale * settings.headerFontScale);
            ImGui::TextWrapped("%s", element.text.c_str());
            ImGui::SetWindowFontScale(fontScale); // Reset to base scale

            ImGui::PopStyleColor();
            ImGui::Dummy(ImVec2(0, settings.lineSpacing * 20.0f));
            break;
        }

        case TextElement::HEADER2:
        {
            ImGui::Dummy(ImVec2(0, settings.lineSpacing * 12.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, settings.headerColor);

            ImGui::SetWindowFontScale(fontScale * settings.header2FontScale);
            ImGui::TextWrapped("%s", element.text.c_str());
            ImGui::SetWindowFontScale(fontScale);

            ImGui::PopStyleColor();
            ImGui::Dummy(ImVec2(0, settings.lineSpacing * 15.0f));
            break;
        }

        case TextElement::HEADER3:
        {
            ImGui::Dummy(ImVec2(0, settings.lineSpacing * 10.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, settings.headerColor);

            ImGui::SetWindowFontScale(fontScale * settings.header3FontScale);
            ImGui::TextWrapped("%s", element.text.c_str());
            ImGui::SetWindowFontScale(fontScale);

            ImGui::PopStyleColor();
            ImGui::Dummy(ImVec2(0, settings.lineSpacing * 12.0f));
            break;
        }

        case TextElement::BOLD:
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
            ImGui::TextWrapped("%s", element.text.c_str());
            ImGui::PopStyleColor();
            if (i + 1 < parsedContent.size() &&
                parsedContent[i + 1].type != TextElement::LINE_BREAK &&
                parsedContent[i + 1].type != TextElement::PARAGRAPH_BREAK) {
                ImGui::SameLine(0.0f, 0.0f);
            }
            break;
        }

        case TextElement::ITALIC:
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 1.0f, 1.0f));
            ImGui::TextWrapped("%s", element.text.c_str());
            ImGui::PopStyleColor();
            if (i + 1 < parsedContent.size() &&
                parsedContent[i + 1].type != TextElement::LINE_BREAK &&
                parsedContent[i + 1].type != TextElement::PARAGRAPH_BREAK) {
                ImGui::SameLine(0.0f, 0.0f);
            }
            break;
        }

        case TextElement::TEXT:
        {
            if (!element.text.empty()) {
                ImGui::TextWrapped("%s", element.text.c_str());
                if (i + 1 < parsedContent.size() &&
                    parsedContent[i + 1].type != TextElement::LINE_BREAK &&
                    parsedContent[i + 1].type != TextElement::PARAGRAPH_BREAK) {
                    ImGui::SameLine(0.0f, 0.0f);
                }
            }
            break;
        }

        case TextElement::LINE_BREAK:
            break;

        case TextElement::PARAGRAPH_BREAK:
        {
            ImGui::Dummy(ImVec2(0, settings.lineSpacing * 15.0f));
            break;
        }
        }
    }

    // Add bottom padding
    ImGui::Dummy(ImVec2(0, settings.marginSize * 2));

    // Reset font scale before ending child
    ImGui::SetWindowFontScale(1.0f);

    // Pop style vars
    ImGui::PopStyleVar(); // ItemSpacing for line spacing

    // End columns
    ImGui::Columns(1);

    // Remember scroll position
    settings.scrollPosition = ImGui::GetScrollY();

    ImGui::EndChild();

    // Pop colors
    ImGui::PopStyleColor(); // Text color
}

// Existing functions with minimal changes needed for compilation
bool ChapterManager::LoadChapter(const std::string& filePath) {
    try {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            std::cout << "Failed to open chapter file: " << filePath << std::endl;
            return false;
        }

        json j;
        file >> j;
        file.close();

        Chapter chapter = j.get<Chapter>();

        bool found = false;
        for (auto& ch : chapters) {
            if (ch.chapterNumber == chapter.chapterNumber) {
                ch = chapter;
                found = true;
                break;
            }
        }

        if (!found) {
            chapters.push_back(chapter);
        }

        std::sort(chapters.begin(), chapters.end(),
            [](const Chapter& a, const Chapter& b) {
                return a.chapterNumber < b.chapterNumber;
            });

        contentNeedsReparsing = true;
        std::cout << "Loaded chapter: " << chapter.title << std::endl;
        return true;

    }
    catch (const std::exception& e) {
        std::cout << "Error loading chapter: " << e.what() << std::endl;
        return false;
    }
}

bool ChapterManager::SaveChapter(const Chapter& chapter, const std::string& novelName) {
    try {
        std::string novelDir = "Novels/" + novelName + "/chapters";
        std::filesystem::create_directories(novelDir);

        std::string filename = novelDir + "/chapter" + std::to_string(chapter.chapterNumber) + ".json";

        json j = chapter;
        std::ofstream file(filename);
        if (!file.is_open()) {
            std::cout << "Failed to create chapter file: " << filename << std::endl;
            return false;
        }

        file << j.dump(4);
        file.close();

        std::cout << "Saved chapter to: " << filename << std::endl;
        return true;

    }
    catch (const std::exception& e) {
        std::cout << "Error saving chapter: " << e.what() << std::endl;
        return false;
    }
}

void ChapterManager::ParseMarkdownContent() {
    if (!contentNeedsReparsing) return;

    parsedContent.clear();

    if (chapters.empty() || settings.currentChapter < 1 ||
        settings.currentChapter > chapters.size()) {
        contentNeedsReparsing = false;
        return;
    }

    const Chapter& current = chapters[settings.currentChapter - 1];
    std::string content = current.content;

    std::istringstream stream(content);
    std::string line;
    bool lastLineWasEmpty = false;

    while (std::getline(stream, line)) {
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);

        if (line.empty()) {
            if (!lastLineWasEmpty) {
                parsedContent.emplace_back(TextElement::PARAGRAPH_BREAK, "");
                lastLineWasEmpty = true;
            }
            continue;
        }

        lastLineWasEmpty = false;

        if (line.length() >= 4 && line.substr(0, 3) == "### ") {
            parsedContent.emplace_back(TextElement::HEADER3, line.substr(4));
        }
        else if (line.length() >= 3 && line.substr(0, 2) == "## ") {
            parsedContent.emplace_back(TextElement::HEADER2, line.substr(3));
        }
        else if (line.length() >= 2 && line.substr(0, 1) == "# ") {
            parsedContent.emplace_back(TextElement::HEADER1, line.substr(2));
        }
        else if (line.length() >= 2 && (line.substr(0, 2) == "- " || line.substr(0, 2) == "* ")) {
            parsedContent.emplace_back(TextElement::TEXT, "• " + line.substr(2));
            parsedContent.emplace_back(TextElement::LINE_BREAK, "");
        }
        else {
            ParseInlineFormatting(line);
            parsedContent.emplace_back(TextElement::LINE_BREAK, "");
        }
    }

    contentNeedsReparsing = false;
}

void ChapterManager::ParseInlineFormatting(const std::string& line) {
    if (line.empty()) {
        parsedContent.emplace_back(TextElement::PARAGRAPH_BREAK, "");
        return;
    }

    std::string currentText = line;
    size_t pos = 0;

    while (pos < currentText.length()) {
        size_t boldStart = currentText.find("**", pos);
        size_t italicStart = currentText.find("*", pos);

        if (italicStart != std::string::npos && italicStart == boldStart) {
            italicStart = currentText.find("*", boldStart + 2);
        }

        if (boldStart != std::string::npos &&
            (italicStart == std::string::npos || boldStart < italicStart)) {

            if (boldStart > pos) {
                std::string beforeText = currentText.substr(pos, boldStart - pos);
                if (!beforeText.empty()) {
                    parsedContent.emplace_back(TextElement::TEXT, beforeText);
                }
            }

            size_t boldEnd = currentText.find("**", boldStart + 2);
            if (boldEnd != std::string::npos) {
                std::string boldText = currentText.substr(boldStart + 2, boldEnd - boldStart - 2);
                if (!boldText.empty()) {
                    parsedContent.emplace_back(TextElement::BOLD, boldText);
                }
                pos = boldEnd + 2;
            }
            else {
                std::string remainingText = currentText.substr(boldStart);
                if (!remainingText.empty()) {
                    parsedContent.emplace_back(TextElement::TEXT, remainingText);
                }
                break;
            }
        }
        else if (italicStart != std::string::npos) {
            if (italicStart > pos) {
                std::string beforeText = currentText.substr(pos, italicStart - pos);
                if (!beforeText.empty()) {
                    parsedContent.emplace_back(TextElement::TEXT, beforeText);
                }
            }

            size_t italicEnd = italicStart + 1;
            while (italicEnd < currentText.length()) {
                if (currentText[italicEnd] == '*') {
                    if (italicEnd + 1 < currentText.length() && currentText[italicEnd + 1] == '*') {
                        italicEnd += 2;
                        continue;
                    }
                    else {
                        break;
                    }
                }
                italicEnd++;
            }

            if (italicEnd < currentText.length() && currentText[italicEnd] == '*') {
                std::string italicText = currentText.substr(italicStart + 1, italicEnd - italicStart - 1);
                if (!italicText.empty()) {
                    parsedContent.emplace_back(TextElement::ITALIC, italicText);
                }
                pos = italicEnd + 1;
            }
            else {
                std::string remainingText = currentText.substr(italicStart);
                if (!remainingText.empty()) {
                    parsedContent.emplace_back(TextElement::TEXT, remainingText);
                }
                break;
            }
        }
        else {
            std::string remainingText = currentText.substr(pos);
            if (!remainingText.empty()) {
                parsedContent.emplace_back(TextElement::TEXT, remainingText);
            }
            break;
        }
    }
}

void ChapterManager::RenderContent() {
    ParseMarkdownContent();

    if (chapters.empty()) {
        ImGui::Text("No chapter loaded");
        return;
    }

    // This is a simplified version - main rendering happens in RenderContentOnly
    RenderContentOnly();
}

void ChapterManager::Render() {
    if (settings.darkTheme) {
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.06f, 0.06f, 0.06f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
    }
    else {
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.98f, 0.98f, 0.98f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
    }

    if (ImGui::BeginMenuBar()) {
        ImGui::PushID("ReadingMenuBar");

        if (ImGui::MenuItem("Settings##ReadingSettings")) {
            showSettings = !showSettings;
        }

        ImGui::Separator();

        if (!chapters.empty()) {
            const Chapter& current = chapters[settings.currentChapter - 1];
            ImGui::Text("Chapter %d: %s", current.chapterNumber, current.title.c_str());
        }

        ImGui::PopID();
        ImGui::EndMenuBar();
    }

    RenderContent();
    RenderSettingsPanel();

    if (settings.darkTheme || !settings.darkTheme) {
        ImGui::PopStyleColor(2);
    }
}

void ChapterManager::OpenChapter(int chapterNumber) {
    if (chapterNumber >= 1 && chapterNumber <= (int)chapters.size()) {
        settings.currentChapter = chapterNumber;
        settings.scrollPosition = 0.0f;
        contentNeedsReparsing = true;
    }
    else {
        std::cout << "Failed at opening chapter" << std::endl;
    }
}

void ChapterManager::SetNovelTitle(const std::string& title) {
    novelTitle = title;
}

void ChapterManager::LoadChaptersFromDirectory(const std::string& novelName) {
    std::string chaptersDir = "Novels/" + novelName + "/chapters";

    if (!std::filesystem::exists(chaptersDir)) {
        std::cout << "Chapters directory doesn't exist: " << chaptersDir << std::endl;
        return;
    }

    chapters.clear();

    for (const auto& entry : std::filesystem::directory_iterator(chaptersDir)) {
        if (entry.path().extension() == ".json") {
            LoadChapter(entry.path().string());
        }
    }

    if (!chapters.empty()) {
        settings.currentChapter = 1;
        contentNeedsReparsing = true;
        novelTitle = novelName;
    }
}

void ChapterManager::RenderEnhancedSettingsPanel() {
    if (!showSettings) return;

    // Larger settings window
    ImGui::SetNextWindowSize(ImVec2(650, 750), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(100, 100), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("📖 Reading Settings", &showSettings, ImGuiWindowFlags_AlwaysVerticalScrollbar)) {

        // Header with current chapter info
        if (!chapters.empty() && settings.currentChapter >= 1 && settings.currentChapter <= (int)chapters.size()) {
            const Chapter& current = chapters[settings.currentChapter - 1];
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.9f, 1.0f, 1.0f));
            ImGui::Text("📚 %s - Chapter %d: %s", novelTitle.c_str(), current.chapterNumber, current.title.c_str());
            ImGui::PopStyleColor();
            ImGui::Separator();
        }

        if (ImGui::BeginTabBar("SettingsTabs", ImGuiTabBarFlags_FittingPolicyScroll)) {

            if (ImGui::BeginTabItem("🔤 Typography")) {
                RenderTypographyTab();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("🎨 Appearance")) {
                RenderAppearanceTab();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("📐 Layout")) {
                RenderLayoutTab();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("🧭 Navigation")) {
                RenderNavigationTab();
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        ImGui::Separator();

        // Action buttons with better styling
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
        if (ImGui::Button("💾 Save Settings", ImVec2(130, 35))) {
            SaveSettings();
        }
        ImGui::PopStyleColor(2);

        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.4f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.5f, 0.3f, 1.0f));
        if (ImGui::Button("🔄 Reset to Defaults", ImVec2(150, 35))) {
            settings = ReadingSettings(); // Reset to improved defaults
            contentNeedsReparsing = true;
            LoadReadingFonts(); // Reload fonts with new settings
        }
        ImGui::PopStyleColor(2);

        ImGui::SameLine();
        if (ImGui::Button("❌ Close", ImVec2(80, 35))) {
            showSettings = false;
        }
    }
    ImGui::End();
}

void ChapterManager::RenderTypographyTab() {
    bool needsReparse = false;
    bool needsFontReload = false;

    ImGui::Text("📝 Font Configuration");
    ImGui::Separator();

    // Font family selection
    if (!availableFontNames.empty()) {
        int currentFont = settings.fontFamily;
        if (currentFont >= availableFontNames.size()) currentFont = 0;

        ImGui::Text("Font Family:");
        ImGui::SetNextItemWidth(300);
        if (ImGui::Combo("##FontFamily", &currentFont, [](void* data, int idx, const char** out_text) {
            auto* names = static_cast<std::vector<std::string>*>(data);
            if (idx >= 0 && idx < names->size()) {
                *out_text = (*names)[idx].c_str();
                return true;
            }
            return false;
            }, &availableFontNames, (int)availableFontNames.size())) {
            settings.fontFamily = currentFont;
            needsFontReload = true;
            needsReparse = true;
        }
    }

    ImGui::Spacing();

    // Font size
    ImGui::Text("Font Size:");
    ImGui::SetNextItemWidth(300);
    float oldFontSize = settings.fontSize;
    if (ImGui::SliderFloat("##FontSize", &settings.fontSize, 12.0f, 36.0f, "%.1f px")) {
        if (abs(settings.fontSize - oldFontSize) > 0.5f) {
            needsFontReload = true;
            needsReparse = true;
        }
    }

    // Line spacing
    ImGui::Text("Line Spacing:");
    ImGui::SetNextItemWidth(300);
    if (ImGui::SliderFloat("##LineSpacing", &settings.lineSpacing, 1.0f, 3.0f, "%.1f")) {
        needsReparse = true;
    }

    // Handle font changes properly
    if (needsFontReload) {
        std::cout << "Typography settings changed, notifying Library" << std::endl;
        if (libraryPtr) {
            libraryPtr->OnReadingSettingsChanged();
        }
        lastFontSize = settings.fontSize;
    }

    if (needsReparse) {
        contentNeedsReparsing = true;
    }

    // Simplified preview to avoid font conflicts
    ImGui::Spacing();
    ImGui::Text("👁️ Live Preview");
    ImGui::Separator();

    ImGui::BeginChild("FontPreview", ImVec2(0, 200), true);
    ImGui::TextWrapped("The quick brown fox jumps over the lazy dog. This preview shows the current settings.");
    ImGui::EndChild();
}

void ChapterManager::RenderAppearanceTab() {
    bool needsReparse = false;

    ImGui::Text("🌙 Theme Settings");
    ImGui::Separator();

    if (ImGui::Checkbox("Dark Theme", &settings.darkTheme)) {
        needsReparse = true;
    }

    ImGui::Spacing();

    ImGui::Text("🎨 Reading Area Background");
    ImGui::Separator();

    if (ImGui::Checkbox("Custom Reading Background", &settings.customBackground)) {
        needsReparse = true;
    }

    if (settings.customBackground) {
        ImGui::Indent();
        ImGui::Text("Background Color:");
        ImGui::SetNextItemWidth(300);
        if (ImGui::ColorEdit4("##ReadingBG", (float*)&settings.backgroundColor, ImGuiColorEditFlags_NoAlpha)) {
            needsReparse = true;
        }

        // Quick color presets
        ImGui::Text("Quick Presets:");
        ImGui::SameLine();
        if (ImGui::SmallButton("Dark")) {
            settings.backgroundColor = ImVec4(0.12f, 0.12f, 0.14f, 1.0f); // Much closer to main background
            needsReparse = true;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Warm")) {
            settings.backgroundColor = ImVec4(0.20f, 0.18f, 0.16f, 1.0f);
            needsReparse = true;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Cool")) {
            settings.backgroundColor = ImVec4(0.15f, 0.17f, 0.20f, 1.0f);
            needsReparse = true;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Sepia")) {
            settings.backgroundColor = ImVec4(0.25f, 0.23f, 0.20f, 1.0f);
            needsReparse = true;
        }

        ImGui::Unindent();
    }

    ImGui::Spacing();
    ImGui::Text("📝 Text Colors");
    ImGui::Separator();

    ImGui::Text("Body Text Color:");
    ImGui::SetNextItemWidth(300);
    if (ImGui::ColorEdit3("##TextColor", (float*)&settings.textColor)) {
        needsReparse = true;
    }

    ImGui::Text("Header Color:");
    ImGui::SetNextItemWidth(300);
    if (ImGui::ColorEdit3("##HeaderColor", (float*)&settings.headerColor)) {
        needsReparse = true;
    }

    // Color preview
    ImGui::Spacing();
    ImGui::Text("👁️ Color Preview");
    ImGui::Separator();

    ImGui::BeginChild("ColorPreview", ImVec2(0, 150), true);

    if (settings.customBackground) {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 start = ImGui::GetCursorScreenPos();
        ImVec2 end = ImVec2(start.x + ImGui::GetContentRegionAvail().x, start.y + 120);
        drawList->AddRectFilled(start, end, ImGui::ColorConvertFloat4ToU32(settings.backgroundColor));
    }

    ImGui::PushStyleColor(ImGuiCol_Text, settings.textColor);
    ImGui::TextWrapped("This is how your body text will appear with the current color settings.");
    ImGui::PopStyleColor();

    ImGui::PushStyleColor(ImGuiCol_Text, settings.headerColor);
    ImGui::Text("This is how headers will appear");
    ImGui::PopStyleColor();

    ImGui::EndChild();

    if (needsReparse) {
        contentNeedsReparsing = true;
    }
}

void ChapterManager::RenderLayoutTab() {
    bool needsReparse = false;

    ImGui::Text("📐 Reading Layout");
    ImGui::Separator();

    // Reading width with visual representation
    ImGui::Text("Reading Width:");
    const char* widths[] = { "Narrow (55%)", "Medium (75%)", "Wide (90%)" };
    ImGui::SetNextItemWidth(300);
    if (ImGui::Combo("##ReadingWidth", &settings.readingWidth, widths, 3)) {
        needsReparse = true;
    }

    // Visual width indicator
    ImGui::Text("Width Preview:");
    float previewWidth = ImGui::GetContentRegionAvail().x;
    float targetWidth = previewWidth * (settings.readingWidth == 0 ? 0.55f :
        settings.readingWidth == 1 ? 0.75f : 0.90f);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 start = ImGui::GetCursorScreenPos();
    ImVec2 end = ImVec2(start.x + targetWidth, start.y + 20);
    drawList->AddRectFilled(start, end, IM_COL32(100, 150, 200, 100));
    ImGui::Dummy(ImVec2(0, 25));

    ImGui::Spacing();

    // Text alignment
    ImGui::Text("Text Alignment:");
    const char* alignments[] = { "Left", "Center", "Justify" };
    ImGui::SetNextItemWidth(300);
    ImGui::Combo("##TextAlignment", &settings.textAlignment, alignments, 3);

    ImGui::Spacing();

    // Margin settings
    ImGui::Text("Margin Size:");
    ImGui::SetNextItemWidth(300);
    if (ImGui::SliderFloat("##MarginSize", &settings.marginSize, 15.0f, 60.0f, "%.0f px")) {
        needsReparse = true;
    }

    ImGui::Spacing();
    ImGui::Text("📜 Scrolling & Navigation");
    ImGui::Separator();

    ImGui::Checkbox("Show Scrollbar", &settings.showScrollbar);
    ImGui::Checkbox("Smooth Scrolling", &settings.smoothScrolling);

    if (needsReparse) {
        contentNeedsReparsing = true;
    }
}

void ChapterManager::RenderNavigationTab() {
    ImGui::Text("🧭 Chapter Navigation");
    ImGui::Separator();

    if (!chapters.empty()) {
        ImGui::Text("Current: Chapter %d of %zu", settings.currentChapter, chapters.size());
        ImGui::ProgressBar((float)settings.currentChapter / (float)chapters.size(),
            ImVec2(300, 0), "");

        ImGui::Spacing();

        // Navigation buttons
        ImGui::BeginGroup();
        if (ImGui::Button("⏮️ First", ImVec2(80, 35)) && settings.currentChapter > 1) {
            OpenChapter(1);
        }
        ImGui::SameLine();
        if (ImGui::Button("◀️ Previous", ImVec2(100, 35)) && settings.currentChapter > 1) {
            OpenChapter(settings.currentChapter - 1);
        }
        ImGui::SameLine();
        if (ImGui::Button("Next ▶️", ImVec2(100, 35)) && settings.currentChapter < (int)chapters.size()) {
            OpenChapter(settings.currentChapter + 1);
        }
        ImGui::SameLine();
        if (ImGui::Button("Last ⏭️", ImVec2(80, 35)) && settings.currentChapter < (int)chapters.size()) {
            OpenChapter((int)chapters.size());
        }
        ImGui::EndGroup();

        ImGui::Spacing();

        // Chapter jump
        ImGui::Text("Jump to Chapter:");
        ImGui::SetNextItemWidth(200);
        int jumpChapter = settings.currentChapter;
        if (ImGui::InputInt("##JumpChapter", &jumpChapter, 1, 10)) {
            if (jumpChapter >= 1 && jumpChapter <= (int)chapters.size()) {
                OpenChapter(jumpChapter);
            }
        }

        ImGui::Spacing();
        ImGui::Text("📊 Reading Statistics");
        ImGui::Separator();

        float completion = (float)settings.currentChapter / (float)chapters.size() * 100.0f;
        ImGui::Text("Novel Completion: %.1f%%", completion);
        ImGui::Text("Chapters Read: %d", settings.currentChapter - 1);
        ImGui::Text("Chapters Remaining: %d", (int)chapters.size() - settings.currentChapter + 1);
    }
    else {
        ImGui::Text("No chapters loaded");
    }
}