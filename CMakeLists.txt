cmake_minimum_required(VERSION 3.19)
project(LightBrowser VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets WebEngineWidgets WebEngineCore Network WebChannel)

qt_standard_project_setup()

qt_add_executable(LightBrowser
    src/main.cpp
    src/BrowserWindow.cpp
    src/BrowserWindow.h
    src/WebPage.cpp
    src/WebPage.h
    src/WebView.cpp
    src/WebView.h
    src/TabView.cpp
    src/TabView.h
    src/DownloadManager.cpp
    src/DownloadManager.h
    src/HistoryManager.cpp
    src/HistoryManager.h
    src/NewTabPage.cpp
    src/NewTabPage.h
    src/SearchProvider.cpp
    src/SearchProvider.h
    src/OmniboxBridge.cpp
    src/OmniboxBridge.h
    src/BookmarkManager.cpp
    src/BookmarkManager.h
    src/AddressSuggestionPopup.cpp
    src/AddressSuggestionPopup.h
    src/UrlRedirectManager.cpp
    src/UrlRedirectManager.h
    src/UrlMappingSettingsDialog.cpp
    src/UrlMappingSettingsDialog.h
    src/ActualUrlBar.cpp
    src/ActualUrlBar.h
    resources/app.rc
    resources/icons.qrc
)

target_link_libraries(LightBrowser PRIVATE
    Qt6::Widgets
    Qt6::WebEngineWidgets
    Qt6::WebEngineCore
    Qt6::Network
    Qt6::WebChannel
)

set_target_properties(LightBrowser PROPERTIES
    WIN32_EXECUTABLE ON
)
