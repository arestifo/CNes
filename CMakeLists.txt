cmake_minimum_required(VERSION 3.17)
project(CNES C)

set(CMAKE_C_STANDARD 11)
find_package(SDL2 REQUIRED)

add_executable(CNES main.c include/types.h cart.c cpu.c include/cpu.h
        window.c include/window.h mem.c include/mem.h include/nes.h include/util.h util.c)

# SDL2
target_include_directories(CNES PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(CNES ${SDL2_LIBRARIES})