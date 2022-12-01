cmake_minimum_required(VERSION 3.16)

project (flute-ffmpeg-tx VERSION 0.1)

set(CMAKE_CXX_STANDARD 17)

find_package(spdlog REQUIRED)
find_package(Poco REQUIRED Foundation)
find_package(Boost REQUIRED)
find_package(PkgConfig)
find_package(spdlog REQUIRED)

pkg_check_modules(Pistache REQUIRED IMPORTED_TARGET libpistache)

add_subdirectory(lib)

include_directories(
    ${PROJECT_SOURCE_DIR}/lib
)
add_executable(flute-ffmpeg-tx main.cpp src/FluteFfmpegTx.cpp src/FluteFfmpegTx.h)

target_link_libraries(flute-ffmpeg-tx
        config++
        flute
        ssl
        crypto
        spdlog::spdlog
        Poco::Foundation)

