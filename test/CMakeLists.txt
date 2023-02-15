cmake_minimum_required(VERSION 3.12)
project(chatterbox_test VERSION 0.0.0)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  add_compile_options(-Wall -std=c++17)
endif()

add_definitions(-DV8_COMPRESS_POINTERS
                -DV8_ENABLE_SANDBOX)

set(CHATTERBOX_PATH "../src" CACHE STRING "the path where chatterbox sources are placed")
set(CONTRIB_PATH "../contrib" CACHE STRING "the path where contrib resources are placed")

include_directories(${CHATTERBOX_PATH}
                    ${CONTRIB_PATH}
                    ${CONTRIB_PATH}/jsoncpp/include
                    ${CONTRIB_PATH}/rapidyaml-build
                    ${CONTRIB_PATH}/restclient-cpp/include
                    ${CONTRIB_PATH}/cryptopp
                    ${CONTRIB_PATH}/spdlog/include
                    ${CONTRIB_PATH}/clipp/include
                    ${CONTRIB_PATH}/v8/include
                    ${CONTRIB_PATH}/googletest/googletest/include)

link_directories(${CONTRIB_PATH}/restclient-cpp/.libs
                 ${CONTRIB_PATH}/jsoncpp-build/lib
                 ${CONTRIB_PATH}/rapidyaml-build/lib
                 ${CONTRIB_PATH}/cryptopp
                 ${CONTRIB_PATH}/v8/out/x86.release/obj
                 ${CONTRIB_PATH}/googletest/build/lib)

add_executable(chatterbox_test
               test.cpp
               ${CHATTERBOX_PATH}/utils.cpp
               ${CHATTERBOX_PATH}/crypto.cpp
               ${CHATTERBOX_PATH}/jsenv.cpp
               ${CHATTERBOX_PATH}/request.cpp
               ${CHATTERBOX_PATH}/conversation.cpp
               ${CHATTERBOX_PATH}/scenario.cpp)

target_link_libraries(chatterbox_test
                      "librestclient-cpp.a"
                      "libjsoncpp.a"
                      ryml
                      cryptopp
                      v8_monolith
                      curl
                      gtest
                      dl
                      pthread)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
