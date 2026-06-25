
cmake_minimum_required( VERSION 3.16 )
project( VoxPitch LANGUAGES CXX )
if( WIN32 AND NOT Qt6_DIR )
    set( Qt6_DIR "D:/ProgramFiles/Qt/6.11.0/mingw_64/lib/cmake/Qt6" )
endif()

set( CMAKE_CXX_STANDARD 17 )
set( CMAKE_CXX_STANDARD_REQUIRED ON )
set( CMAKE_AUTOMOC ON)

if( APPLE )
    # Homebrew's Qt6 FindWrapOpenGL.cmake falls back to "-framework AGL" when the
    # (long removed) AGL framework can't be found, which breaks the link step on
    # recent macOS SDKs. Pre-create the imported target so Qt's module is skipped.
    find_package( OpenGL REQUIRED )
    add_library( WrapOpenGL::WrapOpenGL INTERFACE IMPORTED )
    target_link_libraries( WrapOpenGL::WrapOpenGL INTERFACE OpenGL::GL )
endif()

find_package( Qt6 COMPONENTS REQUIRED
    Multimedia 
    Charts 
    Widgets
)

add_executable( VoxPitch
    main.cpp
    audiocapturer.cpp
    audioplayer.cpp
    pitchdetector.cpp
    mainwindow.cpp
    pianokeyboard.cpp
    pitchtrainerwindow.cpp
    audiofileanalysiswindow.cpp
    pannablechartview.cpp
    skipbutton.cpp
)

target_link_libraries( VoxPitch PRIVATE 
    Qt6::Multimedia 
    Qt6::Charts
    Qt6::Widgets
)