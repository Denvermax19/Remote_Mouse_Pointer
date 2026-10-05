cmake_minimum_required(VERSION 3.20)
project(ScreenPointer)

set(CMAKE_CXX_STANDARD 17)

find_package(ixwebsocket CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

add_executable(ScreenPointer main.cpp)

target_link_libraries(ScreenPointer PRIVATE
    d2d1.lib
    dxgi.lib
    dwmapi.lib
    shcore.lib
    user32.lib
    bcrypt.lib
    ixwebsocket::ixwebsocket
    nlohmann_json::nlohmann_json
)

add_executable(ClickDiag click_diag.cpp)

target_link_libraries(ClickDiag PRIVATE
    user32.lib
    dwmapi.lib
)