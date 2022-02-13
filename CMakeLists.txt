cmake_minimum_required(VERSION 3.17)
project(CNES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "-Og -Wall -Winline")
#set(CMAKE_C_FLAGS "-Ofast -Wall")
find_package(SDL2 REQUIRED)

file(GLOB CNES_SRC CONFIGURE_DEPENDS "src/*.c" "src/mappers/*.c" "src/include/*.h")

add_executable(CNES ${CNES_SRC})

# SDL2
target_include_directories(CNES PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(CNES ${SDL2_LIBRARIES})