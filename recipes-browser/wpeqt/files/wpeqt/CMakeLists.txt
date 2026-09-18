cmake_minimum_required(VERSION 3.16)
project(wpeqt VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2)
find_package(EGL REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_check_modules(WPEWEBKIT REQUIRED IMPORTED_TARGET wpe-webkit-2.0>=2.50)
pkg_check_modules(WPEFDO REQUIRED IMPORTED_TARGET wpebackend-fdo-1.0>=1.14)
pkg_check_modules(LIBWPE REQUIRED IMPORTED_TARGET wpe-1.0>=1.14)

qt_standard_project_setup()

qt_add_executable(wpeqt
    src/main.cpp
    src/wpeengine.h
    src/wpeengine.cpp
    src/wpeviewitem.h
    src/wpeviewitem.cpp
)

qt_add_resources(wpeqt "qml"
    PREFIX "/qt/qml/WpeQt"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/qml"
    FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/qml/Main.qml"
)

target_link_libraries(wpeqt PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    PkgConfig::WPEWEBKIT
    PkgConfig::WPEFDO
    PkgConfig::LIBWPE
    EGL::EGL
)

target_compile_definitions(wpeqt PRIVATE QT_NO_KEYWORDS)

install(TARGETS wpeqt RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(FILES wpeqt.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)