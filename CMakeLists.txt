cmake_minimum_required(VERSION 3.10)
project(CNES C)

set(CMAKE_C_STANDARD 11)
#set(CMAKE_C_FLAGS "-Ofast -Wall")
set(CMAKE_C_FLAGS "-O0 -Wall")
find_package(SDL2 REQUIRED)

add_executable(CNES src/main.c src/include/types.h src/cart.c src/cpu.c src/include/cpu.h
        src/window.c src/include/window.h src/mem.c src/include/mem.h src/include/nes.h
        src/include/util.h src/util.c src/ppu.c src/include/ppu.h src/nes.c src/include/mappers.h
        src/include/args.h src/include/apu.h src/apu.c src/mappers.c src/mappers/nrom.c src/mappers/mmc1.c)

# SDL2
target_include_directories(CNES PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(CNES ${SDL2_LIBRARIES})