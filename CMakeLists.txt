cmake_minimum_required(VERSION 3.28)
project(backend LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-stdlib=libc++ $<$<CONFIG:Debug>:-fsanitize=address,leak,undefined,alignment>)
add_link_options(-stdlib=libc++ -lc++abi $<$<CONFIG:Debug>:-fsanitize=address,leak,undefined,alignment>)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_CLANG_TIDY clang-tidy -p)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)

add_subdirectory(lib)

set(SOURCES
  src/io/socket.cpp
)

set(MODULES
  src/app/app.cppm
  src/app/local_data_source.cppm
  src/app/protocol.cppm
  src/io/socket.cppm
  src/util/util.cppm
)

add_executable(backend src/main.cpp ${SOURCES})
target_sources(backend PRIVATE FILE_SET CXX_MODULES FILES ${MODULES})
target_link_libraries(backend PRIVATE msquic-inc yalantinglibs)

enable_testing()
add_subdirectory(test)
