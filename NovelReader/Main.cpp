#define IMGUI_APP_IMPLEMENTATION
#include "WindowManagment.h"
#include "Library.h"

int main() {
    ImGuiApp::Config config;
    config.width = 1600;
    config.height = 900;
    config.enableValidation = true;
    config.enableDocking = true;
    config.clearColor = ImVec4(0.1f, 0.1f, 0.1f, 1.0f);

    ImGuiApp::Application app(config);

    if (!app.Initialize()) {
        return -1;
    }

    ImGuiApp::Utils::SetDarkTheme();
    ImGuiApp::Utils::SetCustomTabBarStyle();

    Library library(&app);

    library.LoadAllNovelsFromFile();

    app.SetUpdateCallback([&]() {

        library.Render();

        });

    app.Run();

    return 0;
}