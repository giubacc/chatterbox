
cmake_minimum_required(VERSION 3.0.0)
project(chatterbox VERSION 0.0.0)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  add_compile_options(-Wall)
endif()

add_definitions(-DV8_COMPRESS_POINTERS
                -DV8_ENABLE_SANDBOX)

include_directories(src/contrib
                    src/contrib/jsoncpp/include
                    src/contrib/restclient-cpp/include
                    src/contrib/cryptopp
                    src/contrib/spdlog/include
                    src/contrib/clipp/include
                    src/contrib/v8/include)

link_directories(src/contrib/restclient-cpp/.libs
                 src/contrib/jsoncpp-build/lib
                 src/contrib/cryptopp
                 src/contrib/v8/out/x86.release/obj
                 )

add_executable(chatterbox
               src/main.cpp
               src/crypto.cpp
               src/jsenv.cpp
               src/chatterbox.cpp)

target_link_libraries(chatterbox
                      restclient-cpp
                      jsoncpp
                      cryptopp
                      v8_monolith
                      pthread)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)

