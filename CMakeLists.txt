cmake_minimum_required(VERSION 3.16)
project(WZMediaPlayer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# 设置CMAKE_PREFIX_PATH以优先查找Homebrew安装的库（OpenAL, fmt）
if(APPLE)
    list(APPEND CMAKE_PREFIX_PATH
        "/opt/homebrew/opt/openal-soft"
        "/opt/homebrew/opt/fmt"
    )
endif()

# Qt6 components
find_package(Qt6 REQUIRED COMPONENTS
    Core Gui Widgets OpenGL OpenGLWidgets Network
    Multimedia MultimediaWidgets ShaderTools LinguistTools
)

# FFmpeg via pkg-config
find_package(PkgConfig REQUIRED)
pkg_check_modules(AVCODEC REQUIRED libavcodec)
pkg_check_modules(AVFORMAT REQUIRED libavformat)
pkg_check_modules(AVUTIL REQUIRED libavutil)
pkg_check_modules(SWRESAMPLE REQUIRED libswresample)
pkg_check_modules(SWSCALE REQUIRED libswscale)
pkg_check_modules(AVFILTER REQUIRED libavfilter)
pkg_check_modules(AVDEVICE REQUIRED libavdevice)

# OpenAL (使用NO_MODULE优先使用Homebrew提供的CMake配置文件)
find_package(OpenAL REQUIRED NO_MODULE)

# GLEW
find_package(GLEW REQUIRED)

# spdlog and fmt (system)
find_package(spdlog REQUIRED)
find_package(fmt REQUIRED)

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
    # OpenGLVideoWidget.cpp excluded: unused alternative widget, conflicts with QWidget-based VideoWidgetBase
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

# 为 macOS 创建 .app bundle
if(APPLE)
    set_target_properties(WZMediaPlayer PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.leadwit.WZMediaPlayer"
        MACOSX_BUNDLE_BUNDLE_VERSION "1.0.0"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0.0"
    )
endif()

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
    ${AVCODEC_INCLUDE_DIRS}
    ${AVFORMAT_INCLUDE_DIRS}
    ${AVUTIL_INCLUDE_DIRS}
    ${SWRESAMPLE_INCLUDE_DIRS}
    ${SWSCALE_INCLUDE_DIRS}
    ${AVFILTER_INCLUDE_DIRS}
    ${AVDEVICE_INCLUDE_DIRS}
    # GLEW
    ${GLEW_INCLUDE_DIRS}
)

# ---------- Compile definitions ----------

target_compile_definitions(WZMediaPlayer PRIVATE
    SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE
    SPDLOG_FMT_EXTERNAL
)

# ---------- Link library directories ----------

target_link_directories(WZMediaPlayer PRIVATE
    # FFmpeg
    ${AVCODEC_LIBRARY_DIRS}
    ${AVFORMAT_LIBRARY_DIRS}
    ${AVUTIL_LIBRARY_DIRS}
    ${SWRESAMPLE_LIBRARY_DIRS}
    ${SWSCALE_LIBRARY_DIRS}
    ${AVFILTER_LIBRARY_DIRS}
    ${AVDEVICE_LIBRARY_DIRS}
)

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
    ${AVCODEC_LIBRARIES}
    ${AVFORMAT_LIBRARIES}
    ${AVUTIL_LIBRARIES}
    ${SWRESAMPLE_LIBRARIES}
    ${SWSCALE_LIBRARIES}
    ${AVFILTER_LIBRARIES}
    ${AVDEVICE_LIBRARIES}
    # OpenAL (使用modern CMake target，会自动处理include directories)
    OpenAL::OpenAL
    # GLEW & OpenGL
    GLEW::GLEW
    OpenGL::GL
    OpenGL::GLU
    # spdlog & fmt
    spdlog::spdlog
    fmt::fmt
    # System
    pthread
)

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
target_link_libraries(PlaybackStateMachineTest PRIVATE
    spdlog::spdlog fmt::fmt pthread
)

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
target_link_libraries(ErrorRecoveryManagerTest PRIVATE
    spdlog::spdlog fmt::fmt pthread
)

enable_testing()
add_test(NAME PlaybackStateMachineTest COMMAND PlaybackStateMachineTest)
add_test(NAME ErrorRecoveryManagerTest COMMAND ErrorRecoveryManagerTest)
