find_package(Threads REQUIRED)

add_library(msquic-inc INTERFACE)
target_include_directories(msquic-inc INTERFACE inc)
target_link_libraries(msquic-inc INTERFACE Threads::Threads msquic)
