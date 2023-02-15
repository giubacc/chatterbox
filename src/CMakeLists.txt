cmake_minimum_required(VERSION 3.12)
project(chatterbox_binary VERSION 0.0.0)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  add_compile_options(-Wall -std=c++17)
endif()

add_definitions(-DV8_COMPRESS_POINTERS
                -DV8_ENABLE_SANDBOX)

set(CONTRIB_PATH "../contrib" CACHE STRING "the path where contrib resources are placed")

include_directories(${CONTRIB_PATH}
                    ${CONTRIB_PATH}/jsoncpp/include
                    ${CONTRIB_PATH}/rapidyaml-build
                    ${CONTRIB_PATH}/restclient-cpp/include
                    ${CONTRIB_PATH}/cryptopp
                    ${CONTRIB_PATH}/spdlog/include
                    ${CONTRIB_PATH}/clipp/include
                    ${CONTRIB_PATH}/v8/include)

link_directories(${CONTRIB_PATH}/restclient-cpp/.libs
                 ${CONTRIB_PATH}/jsoncpp-build/lib
                 ${CONTRIB_PATH}/rapidyaml-build/lib
                 ${CONTRIB_PATH}/cryptopp
                 ${CONTRIB_PATH}/v8/out/x86.release/obj)

add_executable(chatterbox
               main.cpp
               utils.cpp
               crypto.cpp
               jsenv.cpp
               request.cpp
               conversation.cpp
               scenario.cpp)

target_link_libraries(chatterbox
                      "librestclient-cpp.a"
                      "libjsoncpp.a"
                      ryml
                      cryptopp
                      v8_monolith
                      curl
                      dl
                      pthread)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
