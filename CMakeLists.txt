cmake_minimum_required(VERSION 3.16)
project(WZMediaPlayer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

if(WIN32)
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
    set(CMAKE_PREFIX_PATH "D:/DevTools/Qt/6.6.3/msvc2019_64" ${CMAKE_PREFIX_PATH})
    if(MSVC_VERSION LESS 1930)
        set(CMAKE_GENERATOR_TOOLSET "v142" CACHE STRING "Platform Toolset" FORCE)
    endif()
endif()

# Qt6 components
find_package(Qt6 REQUIRED COMPONENTS
    Core Gui Widgets OpenGL OpenGLWidgets Network
    Multimedia MultimediaWidgets ShaderTools LinguistTools
)

# Platform-specific FFmpeg detection
if(WIN32)
    set(VCPKG_FFMPEG_PATH "D:/github/vcpkg/packages/ffmpeg_x64-windows")
    set(VCPKG_INSTALLED_PATH "D:/github/vcpkg/installed/x64-windows")
    
    find_path(FFMPEG_INCLUDE_DIR
        NAMES libavcodec/avcodec.h
        PATHS
            ${VCPKG_INSTALLED_PATH}/include
            ${VCPKG_FFMPEG_PATH}/include
            $ENV{FFMPEG_HOME}/include
            $ENV{FFMPEG_ROOT}/include
            ${CMAKE_SOURCE_DIR}/3rdparty/ffmpeg/include
        PATH_SUFFIXES ffmpeg
    )

    find_library(AVCODEC_LIBRARY
        NAMES avcodec
        PATHS
            ${VCPKG_INSTALLED_PATH}/lib
            ${VCPKG_FFMPEG_PATH}/lib
            $ENV{FFMPEG_HOME}/lib
            $ENV{FFMPEG_ROOT}/lib
            C:/ffmpeg/lib
            D:/ffmpeg/lib
            ${CMAKE_SOURCE_DIR}/3rdparty/ffmpeg/lib
    )
    find_library(AVFORMAT_LIBRARY
        NAMES avformat
        PATHS
            ${VCPKG_INSTALLED_PATH}/lib
            ${VCPKG_FFMPEG_PATH}/lib
            $ENV{FFMPEG_HOME}/lib
            $ENV{FFMPEG_ROOT}/lib
            C:/ffmpeg/lib
            D:/ffmpeg/lib
            ${CMAKE_SOURCE_DIR}/3rdparty/ffmpeg/lib
    )
    find_library(AVUTIL_LIBRARY
        NAMES avutil
        PATHS
            ${VCPKG_INSTALLED_PATH}/lib
            ${VCPKG_FFMPEG_PATH}/lib
            $ENV{FFMPEG_HOME}/lib
            $ENV{FFMPEG_ROOT}/lib
            C:/ffmpeg/lib
            D:/ffmpeg/lib
            ${CMAKE_SOURCE_DIR}/3rdparty/ffmpeg/lib
    )
    find_library(SWRESAMPLE_LIBRARY
        NAMES swresample
        PATHS
            ${VCPKG_INSTALLED_PATH}/lib
            ${VCPKG_FFMPEG_PATH}/lib
            $ENV{FFMPEG_HOME}/lib
            $ENV{FFMPEG_ROOT}/lib
            C:/ffmpeg/lib
            D:/ffmpeg/lib
            ${CMAKE_SOURCE_DIR}/3rdparty/ffmpeg/lib
    )
    find_library(SWSCALE_LIBRARY
        NAMES swscale
        PATHS
            ${VCPKG_INSTALLED_PATH}/lib
            ${VCPKG_FFMPEG_PATH}/lib
            $ENV{FFMPEG_HOME}/lib
            $ENV{FFMPEG_ROOT}/lib
            C:/ffmpeg/lib
            D:/ffmpeg/lib
            ${CMAKE_SOURCE_DIR}/3rdparty/ffmpeg/lib
    )
    find_library(AVFILTER_LIBRARY
        NAMES avfilter
        PATHS
            ${VCPKG_INSTALLED_PATH}/lib
            ${VCPKG_FFMPEG_PATH}/lib
            $ENV{FFMPEG_HOME}/lib
            $ENV{FFMPEG_ROOT}/lib
            C:/ffmpeg/lib
            D:/ffmpeg/lib
            ${CMAKE_SOURCE_DIR}/3rdparty/ffmpeg/lib
    )
    find_library(AVDEVICE_LIBRARY
        NAMES avdevice
        PATHS
            ${VCPKG_INSTALLED_PATH}/lib
            ${VCPKG_FFMPEG_PATH}/lib
            $ENV{FFMPEG_HOME}/lib
            $ENV{FFMPEG_ROOT}/lib
            C:/ffmpeg/lib
            D:/ffmpeg/lib
            ${CMAKE_SOURCE_DIR}/3rdparty/ffmpeg/lib
    )

    message(STATUS "FFMPEG_INCLUDE_DIR: ${FFMPEG_INCLUDE_DIR}")
    message(STATUS "AVCODEC_LIBRARY: ${AVCODEC_LIBRARY}")

    if(NOT FFMPEG_INCLUDE_DIR OR NOT AVCODEC_LIBRARY)
        message(FATAL_ERROR "FFmpeg not found! Set FFMPEG_HOME or FFMPEG_ROOT environment variable, or place FFmpeg in C:/ffmpeg or D:/ffmpeg")
    endif()

    set(FFMPEG_INCLUDE_DIRS ${FFMPEG_INCLUDE_DIR})
    set(FFMPEG_LIBRARIES
        ${AVCODEC_LIBRARY}
        ${AVFORMAT_LIBRARY}
        ${AVUTIL_LIBRARY}
        ${SWRESAMPLE_LIBRARY}
        ${SWSCALE_LIBRARY}
        ${AVFILTER_LIBRARY}
        ${AVDEVICE_LIBRARY}
    )

    message(STATUS "FFmpeg include: ${FFMPEG_INCLUDE_DIRS}")
    message(STATUS "FFmpeg libraries: ${FFMPEG_LIBRARIES}")
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(AVCODEC REQUIRED libavcodec)
    pkg_check_modules(AVFORMAT REQUIRED libavformat)
    pkg_check_modules(AVUTIL REQUIRED libavutil)
    pkg_check_modules(SWRESAMPLE REQUIRED libswresample)
    pkg_check_modules(SWSCALE REQUIRED libswscale)
    pkg_check_modules(AVFILTER REQUIRED libavfilter)
    pkg_check_modules(AVDEVICE REQUIRED libavdevice)

    set(FFMPEG_INCLUDE_DIRS
        ${AVCODEC_INCLUDE_DIRS}
        ${AVFORMAT_INCLUDE_DIRS}
        ${AVUTIL_INCLUDE_DIRS}
        ${SWRESAMPLE_INCLUDE_DIRS}
        ${SWSCALE_INCLUDE_DIRS}
        ${AVFILTER_INCLUDE_DIRS}
        ${AVDEVICE_INCLUDE_DIRS}
    )
    set(FFMPEG_LIBRARIES
        ${AVCODEC_LIBRARIES}
        ${AVFORMAT_LIBRARIES}
        ${AVUTIL_LIBRARIES}
        ${SWRESAMPLE_LIBRARIES}
        ${SWSCALE_LIBRARIES}
        ${AVFILTER_LIBRARIES}
        ${AVDEVICE_LIBRARIES}
    )
endif()

# OpenAL
if(WIN32)
    set(VCPKG_OPENAL_PATH "D:/github/vcpkg/packages/openal-soft_x64-windows")
    set(VCPKG_INSTALLED_PATH "D:/github/vcpkg/installed/x64-windows")
    find_path(OPENAL_INCLUDE_DIR
        NAMES AL/al.h
        PATHS
            ${VCPKG_INSTALLED_PATH}/include
            ${VCPKG_OPENAL_PATH}/include
            $ENV{OPENAL_HOME}/include
            $ENV{OPENAL_ROOT}/include
    )
    find_library(OPENAL_LIBRARY
        NAMES OpenAL32 openal
        PATHS
            ${VCPKG_INSTALLED_PATH}/lib
            ${VCPKG_OPENAL_PATH}/lib
            $ENV{OPENAL_HOME}/lib
            $ENV{OPENAL_ROOT}/lib
    )
    if(NOT OPENAL_INCLUDE_DIR OR NOT OPENAL_LIBRARY)
        message(FATAL_ERROR "OpenAL not found!")
    endif()
    message(STATUS "OpenAL include: ${OPENAL_INCLUDE_DIR}")
    message(STATUS "OpenAL library: ${OPENAL_LIBRARY}")
else()
    find_package(OpenAL REQUIRED)
endif()

# GLEW
if(WIN32)
    set(GLEW_ROOT "D:/DevTools/glew")
    find_path(GLEW_INCLUDE_DIR
        NAMES GL/glew.h
        PATHS
            ${GLEW_ROOT}/include
    )
    find_library(GLEW_LIBRARY
        NAMES glew32 GLEW
        PATHS
            ${GLEW_ROOT}/lib/Release/x64
            ${GLEW_ROOT}/lib
    )
    if(NOT GLEW_INCLUDE_DIR OR NOT GLEW_LIBRARY)
        message(FATAL_ERROR "GLEW not found! Please install GLEW or set GLEW_ROOT")
    endif()
    message(STATUS "GLEW include: ${GLEW_INCLUDE_DIR}")
    message(STATUS "GLEW library: ${GLEW_LIBRARY}")
    add_library(GLEW::GLEW STATIC IMPORTED)
    set_target_properties(GLEW::GLEW PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${GLEW_INCLUDE_DIR}"
        IMPORTED_LOCATION "${GLEW_LIBRARY}"
    )
else()
    find_package(GLEW REQUIRED)
endif()

# spdlog and fmt (system)
if(WIN32)
    set(VCPKG_INSTALLED_PATH "D:/github/vcpkg/installed/x64-windows")
    
    list(APPEND CMAKE_PREFIX_PATH
        "${VCPKG_INSTALLED_PATH}/share/fmt"
        "${VCPKG_INSTALLED_PATH}/share/spdlog"
    )
    
    find_package(fmt CONFIG REQUIRED)
    find_package(spdlog CONFIG REQUIRED)
else()
    find_package(spdlog REQUIRED)
    find_package(fmt REQUIRED)
endif()

# OpenGL
find_package(OpenGL REQUIRED)

# ---------- Sources ----------

set(WZMEDIA_SRC_DIR ${CMAKE_SOURCE_DIR}/WZMediaPlay)

set(SOURCES
    ${WZMEDIA_SRC_DIR}/main.cpp
    ${WZMEDIA_SRC_DIR}/MainWindow.cpp
    ${WZMEDIA_SRC_DIR}/ApplicationSettings.cpp
    ${WZMEDIA_SRC_DIR}/BroadcastReceiver.cpp
    ${WZMEDIA_SRC_DIR}/crash_handler.cpp
    ${WZMEDIA_SRC_DIR}/ErrorRecoveryManager.cpp
    ${WZMEDIA_SRC_DIR}/GlobalDef.cpp
    ${WZMEDIA_SRC_DIR}/PlaybackStateMachine.cpp
    ${WZMEDIA_SRC_DIR}/PlayController.cpp
    ${WZMEDIA_SRC_DIR}/RemoteControlServer.cpp
    ${WZMEDIA_SRC_DIR}/ShaderManager.cpp
    ${WZMEDIA_SRC_DIR}/StereoVideoWidget.cpp
    ${WZMEDIA_SRC_DIR}/SystemMonitor.cpp
    ${WZMEDIA_SRC_DIR}/TcpServerHelper.cpp
    ${WZMEDIA_SRC_DIR}/TcpSocket.cpp
    ${WZMEDIA_SRC_DIR}/ThreadSyncManager.cpp
    ${WZMEDIA_SRC_DIR}/UdpWorker.cpp
    ${WZMEDIA_SRC_DIR}/test_support/TestPipeServer.cpp
    # GUI
    ${WZMEDIA_SRC_DIR}/gui/AboutDialog.cpp
    ${WZMEDIA_SRC_DIR}/gui/AdvertisementWidget.cpp
    ${WZMEDIA_SRC_DIR}/gui/CustomSlider.cpp
    ${WZMEDIA_SRC_DIR}/gui/CustomTabStyle.cpp
    ${WZMEDIA_SRC_DIR}/gui/DrawWidget.cpp
    ${WZMEDIA_SRC_DIR}/gui/FloatButton.cpp
    ${WZMEDIA_SRC_DIR}/gui/FullscreenTipsWidget.cpp
    ${WZMEDIA_SRC_DIR}/gui/SettingsDialog.cpp
    ${WZMEDIA_SRC_DIR}/gui/SubtitleWidget.cpp
    ${WZMEDIA_SRC_DIR}/gui/SwitchButton.cpp
    ${WZMEDIA_SRC_DIR}/gui/UpShowComboBox.cpp
    # Playlist
    ${WZMEDIA_SRC_DIR}/playlist/DropListWidget.cpp
    ${WZMEDIA_SRC_DIR}/playlist/Playlist.cpp
    ${WZMEDIA_SRC_DIR}/playlist/PlayListData.cpp
    ${WZMEDIA_SRC_DIR}/playlist/PlaylistItem.cpp
    ${WZMEDIA_SRC_DIR}/playlist/PlayListManager.cpp
    ${WZMEDIA_SRC_DIR}/playlist/PlayListPage.cpp
    # Video decoder
    ${WZMEDIA_SRC_DIR}/videoDecoder/AudioThread.cpp
    ${WZMEDIA_SRC_DIR}/videoDecoder/AVClock.cpp
    ${WZMEDIA_SRC_DIR}/videoDecoder/AVThread.cpp
    ${WZMEDIA_SRC_DIR}/videoDecoder/Decoder.cpp
    ${WZMEDIA_SRC_DIR}/videoDecoder/Demuxer.cpp
    ${WZMEDIA_SRC_DIR}/videoDecoder/DemuxerThread.cpp
    ${WZMEDIA_SRC_DIR}/videoDecoder/FFDecHW.cpp
    ${WZMEDIA_SRC_DIR}/videoDecoder/FFDecSW.cpp
    ${WZMEDIA_SRC_DIR}/videoDecoder/Frame.cpp
    ${WZMEDIA_SRC_DIR}/videoDecoder/FrameBuffer.cpp
    ${WZMEDIA_SRC_DIR}/videoDecoder/hardware_decoder.cc
    ${WZMEDIA_SRC_DIR}/videoDecoder/OpenALAudio.cpp
    ${WZMEDIA_SRC_DIR}/videoDecoder/RendererFactory.cpp
    ${WZMEDIA_SRC_DIR}/videoDecoder/Statistics.cpp
    ${WZMEDIA_SRC_DIR}/videoDecoder/VideoFilter.cpp
    ${WZMEDIA_SRC_DIR}/videoDecoder/VideoOutputCommon.cpp
    ${WZMEDIA_SRC_DIR}/videoDecoder/VideoRenderer.cpp
    ${WZMEDIA_SRC_DIR}/videoDecoder/VideoThread.cpp
    ${WZMEDIA_SRC_DIR}/videoDecoder/VideoWidgetBase.cpp
    # OpenGL renderers
    ${WZMEDIA_SRC_DIR}/videoDecoder/opengl/DebugImageSaver.cpp
    ${WZMEDIA_SRC_DIR}/videoDecoder/opengl/Functions.cpp
    ${WZMEDIA_SRC_DIR}/videoDecoder/opengl/OpenGLCommon.cpp
    ${WZMEDIA_SRC_DIR}/videoDecoder/opengl/OpenGLRenderer.cpp
    ${WZMEDIA_SRC_DIR}/videoDecoder/opengl/OpenGLWidget.cpp
    ${WZMEDIA_SRC_DIR}/videoDecoder/opengl/OpenGLWindow.cpp
    ${WZMEDIA_SRC_DIR}/videoDecoder/opengl/StereoOpenGLCommon.cpp
    ${WZMEDIA_SRC_DIR}/videoDecoder/opengl/StereoOpenGLRenderer.cpp
    ${WZMEDIA_SRC_DIR}/videoDecoder/opengl/StereoOpenGLWidget.cpp
    ${WZMEDIA_SRC_DIR}/videoDecoder/opengl/StereoOpenGLWindow.cpp
    # Camera
    ${WZMEDIA_SRC_DIR}/camera/CameraManager.cpp
    ${WZMEDIA_SRC_DIR}/camera/CameraOpenGLWidget.cpp
    ${WZMEDIA_SRC_DIR}/camera/CameraRenderer.cpp
)

# ---------- UI files ----------

set(UI_FILES
    ${WZMEDIA_SRC_DIR}/MainWindow.ui
    ${WZMEDIA_SRC_DIR}/gui/AboutDialog.ui
    ${WZMEDIA_SRC_DIR}/gui/AdvertisementWidget.ui
    ${WZMEDIA_SRC_DIR}/gui/DrawWidget.ui
    ${WZMEDIA_SRC_DIR}/gui/FloatButton.ui
    ${WZMEDIA_SRC_DIR}/gui/SettingsDialog.ui
    ${WZMEDIA_SRC_DIR}/playlist/PlayListPage.ui
)

# ---------- Resource files ----------

set(QRC_FILES
    ${WZMEDIA_SRC_DIR}/MainWindow.qrc
    ${WZMEDIA_SRC_DIR}/videoDecoder/opengl/opengl.qrc
)

# ---------- Target ----------

qt_add_executable(WZMediaPlayer
    ${SOURCES}
    ${UI_FILES}
    ${QRC_FILES}
)

# ---------- Include directories ----------

target_include_directories(WZMediaPlayer PRIVATE
    ${WZMEDIA_SRC_DIR}
    ${WZMEDIA_SRC_DIR}/videoDecoder
    ${WZMEDIA_SRC_DIR}/videoDecoder/opengl
    ${WZMEDIA_SRC_DIR}/gui
    ${WZMEDIA_SRC_DIR}/playlist
    ${WZMEDIA_SRC_DIR}/camera
    ${WZMEDIA_SRC_DIR}/test_support
    ${WZMEDIA_SRC_DIR}/utils
    ${CMAKE_CURRENT_BINARY_DIR}
    # FFmpeg
    ${FFMPEG_INCLUDE_DIRS}
    # OpenAL
    ${OPENAL_INCLUDE_DIR}
    # GLEW
    ${GLEW_INCLUDE_DIRS}
)

# ---------- Compile definitions ----------

target_compile_definitions(WZMediaPlayer PRIVATE
    SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE
    SPDLOG_FMT_EXTERNAL
)

if(WIN32)
    target_compile_definitions(WZMediaPlayer PRIVATE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        Q_OS_WIN
    )
endif()

# ---------- Link libraries ----------

target_link_libraries(WZMediaPlayer PRIVATE
    # Qt6
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::OpenGL
    Qt6::OpenGLWidgets
    Qt6::Network
    Qt6::Multimedia
    Qt6::MultimediaWidgets
    Qt6::ShaderTools
    # FFmpeg
    ${FFMPEG_LIBRARIES}
    # OpenAL
    ${OPENAL_LIBRARY}
    # GLEW & OpenGL
    GLEW::GLEW
    OpenGL::GL
    OpenGL::GLU
    # spdlog & fmt
    spdlog::spdlog
    fmt::fmt
)

# Platform-specific libraries
if(WIN32)
    target_link_libraries(WZMediaPlayer PRIVATE
        ws2_32
        iphlpapi
        bcrypt
        Wtsapi32
        DbgHelp
    )
else()
    find_package(Threads REQUIRED)
    target_link_libraries(WZMediaPlayer PRIVATE Threads::Threads)
endif()

# ---------- AUTOUIC search paths ----------

set_target_properties(WZMediaPlayer PROPERTIES
    AUTOUIC_SEARCH_PATHS "${WZMEDIA_SRC_DIR};${WZMEDIA_SRC_DIR}/gui;${WZMEDIA_SRC_DIR}/playlist"
)

# ---------- Unit tests ----------

add_executable(PlaybackStateMachineTest
    ${WZMEDIA_SRC_DIR}/tests/PlaybackStateMachineTest.cpp
    ${WZMEDIA_SRC_DIR}/PlaybackStateMachine.cpp
    ${WZMEDIA_SRC_DIR}/tests/test_logger_stub.cpp
)
target_compile_definitions(PlaybackStateMachineTest PRIVATE
    STANDALONE_TEST
    SPDLOG_FMT_EXTERNAL
)
target_include_directories(PlaybackStateMachineTest PRIVATE
    ${WZMEDIA_SRC_DIR}
)
if(WIN32)
    target_link_libraries(PlaybackStateMachineTest PRIVATE spdlog::spdlog fmt::fmt)
else()
    find_package(Threads REQUIRED)
    target_link_libraries(PlaybackStateMachineTest PRIVATE spdlog::spdlog fmt::fmt Threads::Threads)
endif()

add_executable(ErrorRecoveryManagerTest
    ${WZMEDIA_SRC_DIR}/tests/ErrorRecoveryManagerTest.cpp
    ${WZMEDIA_SRC_DIR}/ErrorRecoveryManager.cpp
    ${WZMEDIA_SRC_DIR}/tests/test_logger_stub.cpp
)
target_compile_definitions(ErrorRecoveryManagerTest PRIVATE
    STANDALONE_TEST
    SPDLOG_FMT_EXTERNAL
)
target_include_directories(ErrorRecoveryManagerTest PRIVATE
    ${WZMEDIA_SRC_DIR}
)
if(WIN32)
    target_link_libraries(ErrorRecoveryManagerTest PRIVATE spdlog::spdlog fmt::fmt)
else()
    find_package(Threads REQUIRED)
    target_link_libraries(ErrorRecoveryManagerTest PRIVATE spdlog::spdlog fmt::fmt Threads::Threads)
endif()

# Seeking 状态机单元测试（enterSeeking/exitSeeking）
add_executable(SeekingStateMachineTest
    ${WZMEDIA_SRC_DIR}/tests/SeekingStateMachineTest.cpp
    ${WZMEDIA_SRC_DIR}/PlaybackStateMachine.cpp
    ${WZMEDIA_SRC_DIR}/tests/test_logger_stub.cpp
)
target_compile_definitions(SeekingStateMachineTest PRIVATE
    STANDALONE_TEST
    SPDLOG_FMT_EXTERNAL
)
target_include_directories(SeekingStateMachineTest PRIVATE
    ${WZMEDIA_SRC_DIR}
)
if(WIN32)
    target_link_libraries(SeekingStateMachineTest PRIVATE spdlog::spdlog fmt::fmt)
else()
    find_package(Threads REQUIRED)
    target_link_libraries(SeekingStateMachineTest PRIVATE spdlog::spdlog fmt::fmt Threads::Threads)
endif()

enable_testing()
add_test(NAME PlaybackStateMachineTest COMMAND PlaybackStateMachineTest)
add_test(NAME ErrorRecoveryManagerTest COMMAND ErrorRecoveryManagerTest)
add_test(NAME SeekingStateMachineTest COMMAND SeekingStateMachineTest)
