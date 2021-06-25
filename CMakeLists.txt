cmake_minimum_required(VERSION 3.17)
project(CNES C)

set(CMAKE_C_STANDARD 11)
#set(CMAKE_C_FLAGS "-Ofast -Wall")
set(CMAKE_C_FLAGS "-Og -Wall")
find_package(SDL2 REQUIRED)

add_executable(CNES src/main.c include/types.h src/cart.c src/cpu.c include/cpu.h
        src/window.c include/window.h src/mem.c include/mem.h include/nes.h include/util.h src/util.c src/ppu.c include/ppu.h src/nes.c include/mapper.h include/args.h include/apu.h src/apu.c)

# SDL2
target_include_directories(CNES PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(CNES ${SDL2_LIBRARIES})