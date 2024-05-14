add_executable(api_test api_test.cpp)
target_link_libraries(api_test PRIVATE msquic-inc yalantinglibs)

add_test(NAME api_test COMMAND api_test 127.0.0.1 8080 2 10000)
