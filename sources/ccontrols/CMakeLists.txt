cmake_minimum_required(VERSION 3.16)

project(CControls LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(CControls STATIC
    control.cpp
    control_collection.cpp
    panel.cpp
    button.cpp
    text_input.cpp
    textbox.cpp
    label.cpp
    toggle_button.cpp
    focus_manager.cpp
)

target_include_directories(CControls PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
