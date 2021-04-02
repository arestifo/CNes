cmake_minimum_required(VERSION 3.17)
project(CNES C)

set(CMAKE_C_STANDARD 11)
find_package(SDL2 REQUIRED)

add_executable(CNES main.c types.h nes_cart.c nes_cpu.c nes_cpu.h nes_ppu.c nes_ppu.h cnes_window.c cnes_window.h nes_mem.c nes_mem.h)

# SDL2
target_include_directories(CNES PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(CNES ${SDL2_LIBRARIES})