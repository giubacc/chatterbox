cmake_minimum_required(VERSION 3.12)
project(chatterbox_test VERSION 0.0.0)

option(ADD_DEBUG_SYMBOLS "Add debug symbols" OFF)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  add_compile_options(-Wall -std=c++20)
  if(ADD_DEBUG_SYMBOLS)
    add_compile_options(-g)
  endif()
endif()

add_definitions(-DV8_COMPRESS_POINTERS
                -DV8_ENABLE_SANDBOX)

set(CHATTERBOX_PATH "../src" CACHE STRING "the path where chatterbox sources are placed")
set(CONTRIB_PATH "../contrib" CACHE STRING "the path where contrib resources are placed")

include_directories(${CHATTERBOX_PATH}
                    ${CONTRIB_PATH}
                    ${CONTRIB_PATH}/rapidyaml-build
                    ${CONTRIB_PATH}/pistache/include
                    ${CONTRIB_PATH}/restclient-cpp/include
                    ${CONTRIB_PATH}/cryptopp
                    ${CONTRIB_PATH}/spdlog/include
                    ${CONTRIB_PATH}/clipp/include
                    ${CONTRIB_PATH}/v8/include
                    ${CONTRIB_PATH}/googletest/googletest/include)

link_directories(${CONTRIB_PATH}/restclient-cpp/.libs
                 ${CONTRIB_PATH}/rapidyaml-build/lib
                 ${CONTRIB_PATH}/pistache/build/src
                 ${CONTRIB_PATH}/cryptopp
                 ${CONTRIB_PATH}/v8/out/x64.release/obj
                 ${CONTRIB_PATH}/googletest/build/lib)

add_executable(cbx_test
               test.cpp
               ${CHATTERBOX_PATH}/utils.cpp
               ${CHATTERBOX_PATH}/crypto.cpp
               ${CHATTERBOX_PATH}/jsenv.cpp
               ${CHATTERBOX_PATH}/request.cpp
               ${CHATTERBOX_PATH}/conversation.cpp
               ${CHATTERBOX_PATH}/scenario.cpp
               ${CHATTERBOX_PATH}/endpoint.cpp
               ${CHATTERBOX_PATH}/cbox.cpp)

target_link_libraries(cbx_test
                      "librestclient-cpp.a"
                      ryml
                      pistache
                      cryptopp
                      v8_monolith
                      curl
                      gtest
                      dl
                      pthread
                      crypto
                      ssl)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
