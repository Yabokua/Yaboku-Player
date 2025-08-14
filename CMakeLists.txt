cmake_minimum_required(VERSION 3.10)
project(yaboku_player)

add_definitions(-DPROJECT_ROOT_DIR="${CMAKE_SOURCE_DIR}")

set(CMAKE_CXX_STANDARD 17)

# Определяем ОС
if(WIN32)
    set(OS_WINDOWS TRUE)
elseif(APPLE)
    set(OS_MACOS TRUE)
else()
    set(OS_LINUX TRUE)
endif()

add_subdirectory(external/glfw)

# Найти необходимые пакеты
if(NOT WIN32)
    find_package(PkgConfig REQUIRED)
    # PortAudio
    pkg_check_modules(PORTAUDIO REQUIRED portaudio-2.0)
    # TagLib
    find_path(TAGLIB_INCLUDE_DIR tag.h PATH_SUFFIXES taglib)
    find_library(TAGLIB_LIBRARY tag)
    if(NOT TAGLIB_INCLUDE_DIR OR NOT TAGLIB_LIBRARY)
        message(FATAL_ERROR "Taglib не найден.")
    endif()
else()
    # Для Windows - предполагаем, что библиотеки установлены через vcpkg или подобное
    find_package(PkgConfig)
    if(PkgConfig_FOUND)
        pkg_check_modules(PORTAUDIO portaudio-2.0)
        pkg_check_modules(TAGLIB taglib)
    endif()
endif()

# ImGui исходники
file(GLOB IMGUI_SRC
    external/imgui/*.cpp
    external/imgui/backends/imgui_impl_glfw.cpp
    external/imgui/backends/imgui_impl_opengl3.cpp
)

add_library(tinyfiledialogs STATIC external/tinyfiledialogs/tinyfiledialogs.c)

# Создание ресурсного файла для Windows
if(OS_WINDOWS)
    configure_file(
        "${CMAKE_SOURCE_DIR}/resources/app.rc.in"
        "${CMAKE_BINARY_DIR}/app.rc"
        @ONLY
    )
    set(RESOURCE_FILES "${CMAKE_BINARY_DIR}/app.rc")
endif()

add_executable(yaboku_player
    src/main.cpp
    src/ui.cpp
    src/player.cpp
    src/ui_style.cpp
    src/texture_loader.cpp  
    src/lyrics.cpp  
    src/get_artist_info.cpp
    src/equalizer_ui.cpp
    src/eq.cpp
    ${IMGUI_SRC}
    ${RESOURCE_FILES}
)

target_include_directories(yaboku_player PRIVATE
    include
    external/imgui
    external/imgui/backends
    external/glfw/include
    external/tinyfiledialogs
    external/json/include
    ${TAGLIB_INCLUDE_DIR}
    ${PORTAUDIO_INCLUDE_DIRS}
)

# Линковка библиотек в зависимости от ОС
if(OS_WINDOWS)
    target_link_libraries(yaboku_player PRIVATE
        glfw
        opengl32
        tinyfiledialogs
        # Добавьте пути к Windows библиотекам
        # portaudio taglib curl
    )
elseif(OS_MACOS)
    target_link_libraries(yaboku_player PRIVATE
        glfw
        "-framework OpenGL"
        "-framework Cocoa"
        "-framework IOKit"
        "-framework CoreVideo"
        tinyfiledialogs
        curl
        ${TAGLIB_LIBRARY}
        ${PORTAUDIO_LIBRARIES}
    )
else() # Linux
    target_link_libraries(yaboku_player PRIVATE
        glfw
        GL
        Xi
        Xrandr
        Xinerama
        tinyfiledialogs
        curl
        ${TAGLIB_LIBRARY}
        ${PORTAUDIO_LIBRARIES}
    )
endif()

# Компилятор флаги для PortAudio
if(PORTAUDIO_FOUND)
    target_compile_options(yaboku_player PRIVATE ${PORTAUDIO_CFLAGS_OTHER})
endif()

# Установка в зависимости от ОС
if(OS_LINUX)
    install(TARGETS yaboku_player DESTINATION bin)
    
    # Установка .desktop файла
    configure_file(
        "${CMAKE_SOURCE_DIR}/resources/yaboku_player.desktop.in"
        "${CMAKE_BINARY_DIR}/yaboku_player.desktop"
        @ONLY
    )
    install(FILES "${CMAKE_BINARY_DIR}/yaboku_player.desktop"
            DESTINATION share/applications)
    
    # Установка иконок
    install(FILES "${CMAKE_SOURCE_DIR}/resources/icons/icon.png"
            DESTINATION share/pixmaps
            RENAME yaboku_player.png)
    install(FILES "${CMAKE_SOURCE_DIR}/resources/icons/icon.png"
            DESTINATION share/icons/hicolor/48x48/apps
            RENAME yaboku_player.png)

elseif(OS_MACOS)
    # Создание .app bundle для macOS
    set_target_properties(yaboku_player PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "Yaboku Player"
        MACOSX_BUNDLE_BUNDLE_VERSION "1.0"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0"
        MACOSX_BUNDLE_IDENTIFIER "com.yourcompany.yabokuplayer"
        MACOSX_BUNDLE_ICON_FILE "icon.icns"
    )
    
    # Копирование иконки в bundle
    set(ICON_FILE "${CMAKE_SOURCE_DIR}/resources/icons/icon.icns")
    set_source_files_properties(${ICON_FILE} PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources")
    target_sources(yaboku_player PRIVATE ${ICON_FILE})

elseif(OS_WINDOWS)
    # Для Windows иконка уже включена через .rc файл
    install(TARGETS yaboku_player DESTINATION .)
    
    # Копирование ресурсов
    install(DIRECTORY "${CMAKE_SOURCE_DIR}/resources/"
            DESTINATION resources)
endif()

