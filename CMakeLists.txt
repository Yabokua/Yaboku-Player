cmake_minimum_required(VERSION 3.10)
project(yaboku_player)

add_definitions(-DPROJECT_ROOT_DIR="${CMAKE_SOURCE_DIR}")

set(CMAKE_CXX_STANDARD 17)

# ОС
if(WIN32)
    set(OS_WINDOWS TRUE)
elseif(APPLE)
    set(OS_MACOS TRUE)
else()
    set(OS_LINUX TRUE)
endif()

add_subdirectory(external/glfw)

# try to find system lib, if not found - use built-in ones
if(NOT WIN32)
    find_package(PkgConfig REQUIRED)
    # PortAudio
    pkg_check_modules(PORTAUDIO REQUIRED portaudio-2.0 portaudio)
    
    # TagLib
    pkg_check_modules(TAGLIB taglib)
    if(NOT TAGLIB_FOUND)
        message(STATUS "System TagLib not found, using bundled version")
        add_subdirectory(external/taglib)
        set(TAGLIB_LIBRARIES tag)
        set(TAGLIB_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/external/taglib ${CMAKE_SOURCE_DIR}/external/taglib/taglib)
    else()
        message(STATUS "Using system TagLib")
        set(TAGLIB_LIBRARIES ${TAGLIB_LIBRARIES})
        set(TAGLIB_INCLUDE_DIRS ${TAGLIB_INCLUDE_DIRS})
    endif()
else()
    # PortAudio for Windows
    find_library(PORTAUDIO_LIBRARIES NAMES portaudio_static portaudio)
    find_path(PORTAUDIO_INCLUDE_DIRS portaudio.h)

    if(NOT PORTAUDIO_LIBRARIES)
        message(FATAL_ERROR "PortAudio not found on Windows. Please install it or place in external/portaudio")
    endif()

    add_subdirectory(external/taglib)
    set(TAGLIB_LIBRARIES tag)
    set(TAGLIB_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/external/taglib ${CMAKE_SOURCE_DIR}/external/taglib/taglib)
endif()

# ImGui
file(GLOB IMGUI_SRC
    external/imgui/*.cpp
    external/imgui/backends/imgui_impl_glfw.cpp
    external/imgui/backends/imgui_impl_opengl3.cpp
)

# tinyfiledialogs
add_library(tinyfiledialogs STATIC external/tinyfiledialogs/tinyfiledialogs.c)

# Create recursive for Windows
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
    ${TAGLIB_INCLUDE_DIRS}
    ${PORTAUDIO_INCLUDE_DIRS}
)

# link lib on ОС
if(OS_WINDOWS)
    target_link_libraries(yaboku_player PRIVATE
        glfw
        opengl32
        tinyfiledialogs
        ${PORTAUDIO_LIBRARIES}
        ${TAGLIB_LIBRARIES}
        ws2_32
        winmm
        curl    
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
        ${TAGLIB_LIBRARIES}
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
        ${TAGLIB_LIBRARIES}
        ${PORTAUDIO_LIBRARIES}
    )
endif()

# Compile flags for PortAudio
if(PORTAUDIO_FOUND)
    target_compile_options(yaboku_player PRIVATE ${PORTAUDIO_CFLAGS_OTHER})
endif()

# Clonpile flag for old version 
if(TAGLIB_FOUND)
    target_compile_options(yaboku_player PRIVATE ${TAGLIB_CFLAGS_OTHER})
endif()

# INSTALL
if(OS_LINUX)
    install(TARGETS yaboku_player DESTINATION bin)
    
    # .desktop
    configure_file(
        "${CMAKE_SOURCE_DIR}/resources/yaboku_player.desktop.in"
        "${CMAKE_BINARY_DIR}/yaboku_player.desktop"
        @ONLY
    )
    install(FILES "${CMAKE_BINARY_DIR}/yaboku_player.desktop"
            DESTINATION share/applications)
    
    # Icons
    install(FILES "${CMAKE_SOURCE_DIR}/resources/icons/icon.png"
            DESTINATION share/pixmaps
            RENAME yaboku_player.png)
    install(FILES "${CMAKE_SOURCE_DIR}/resources/icons/icon.png"
            DESTINATION share/icons/hicolor/48x48/apps
            RENAME yaboku_player.png)

elseif(OS_MACOS)
    # create .app bundle for macOS
    set_target_properties(yaboku_player PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "Yaboku Player"
        MACOSX_BUNDLE_BUNDLE_VERSION "1.0"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0"
        MACOSX_BUNDLE_IDENTIFIER "com.yourcompany.yabokuplayer"
        MACOSX_BUNDLE_ICON_FILE "icon.icns"
    )
    
    # Copy icons in bundle
    set(ICON_FILE "${CMAKE_SOURCE_DIR}/resources/icons/icon.icns")
    set_source_files_properties(${ICON_FILE} PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources")
    target_sources(yaboku_player PRIVATE ${ICON_FILE})

elseif(OS_WINDOWS)
    install(TARGETS yaboku_player DESTINATION .)
    install(DIRECTORY "${CMAKE_SOURCE_DIR}/resources/"
            DESTINATION resources)
endif()

