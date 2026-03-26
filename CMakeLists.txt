cmake_minimum_required(VERSION 3.20)
project(Zenscii LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(zenscii main.cpp)

option(IMG2ASCII_BUILD_GUI "Build Qt GUI (img2ascii-gui)" ON)
if (IMG2ASCII_BUILD_GUI)
  find_package(Qt6 QUIET COMPONENTS Widgets Gui)
  if (Qt6_FOUND)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTOUIC ON)
    set(CMAKE_AUTORCC ON)

    add_executable(zenscii-gui
      gui/main_gui.cpp
      gui/MainWindow.cpp
      gui/MainWindow.h
    )
    target_link_libraries(zenscii-gui PRIVATE Qt6::Widgets Qt6::Gui)
  else()
    message(STATUS "Qt6 not found; GUI target will not be built.")
  endif()
endif()

