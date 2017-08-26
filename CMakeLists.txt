CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
project(funkeymonkey-pyrainput)

set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

include_directories(include)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

add_library(pyrainput SHARED pyrainput.cpp)
install(TARGETS pyrainput DESTINATION lib/funkeymonkey)
