cmake_minimum_required(VERSION 3.16)
project(picam-detector VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

find_package(OpenCV 4 REQUIRED COMPONENTS core imgproc imgcodecs videoio dnn)
find_package(Threads REQUIRED)

add_executable(picam-detector
  src/main.cpp
  src/config.cpp
  src/state.cpp
  src/camera.cpp
  src/detector.cpp
  src/http_server.cpp
)

target_include_directories(picam-detector PRIVATE src ${OpenCV_INCLUDE_DIRS})
target_link_libraries(picam-detector PRIVATE ${OpenCV_LIBS} Threads::Threads)
target_compile_options(picam-detector PRIVATE -Wall -Wextra)

include(GNUInstallDirs)
install(TARGETS picam-detector RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
