cmake_minimum_required(VERSION 3.19)
project(Rhythm LANGUAGES CXX)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Widgets Multimedia Sql Network Concurrent)

set(CMAKE_AUTORCC ON)

qt_standard_project_setup()

qt_add_executable(Rhythm
    WIN32 MACOSX_BUNDLE
    main.cpp
    Rhythm.cpp
    Rhythm.h
    Rhythm.ui
    Resource.qrc
    TitleBar.h TitleBar.cpp
    Song.h
    TrackModel.h TrackModel.cpp
    MusicLibrary.h MusicLibrary.cpp
    PlayerController.h PlayerController.cpp
    MarqueeLabel.h MarqueeLabel.cpp

    SquareImageLabel.h SquareImageLabel.cpp
    SongCard.h SongCard.cpp
    QueueItemWidget.h QueueItemWidget.cpp
    Lyrics.h Lyrics.cpp
    HoverSlider.h HoverSlider.cpp
    HoverIconButton.h HoverIconButton.cpp
    AudioAnalyzer.h AudioAnalyzer.cpp
    AudioSpectrumWidget.h AudioSpectrumWidget.cpp
    LyricsView.h LyricsView.cpp
    DesktopLyrics.h DesktopLyrics.cpp
    UnlockButton.h UnlockButton.cpp
)

target_include_directories(Rhythm PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(Rhythm
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt6::Multimedia
        Qt6::Sql
        Qt6::Network
        Qt6::Concurrent
)

install(TARGETS Rhythm
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET Rhythm
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
