cmake_minimum_required(VERSION 3.15 FATAL_ERROR)
project(ryml LANGUAGES CXX)

# add a library using the amalgamated header
add_library(ryml ryml.cpp ${SINGLE_HEADER})
