cmake_minimum_required(VERSION 2.8)

project(breep)

find_package( Boost 1.55 REQUIRED )

include_directories( ${Boost_INCLUDE_DIR} )
include_directories(include)

link_directories( ${Boost_LIBRARY_DIRS} )

# chat & v2
add_executable( chat "examples/chat/main.cpp" )
add_executable( chatv2 "examples/chatv2/main.cpp")

target_link_libraries( chat ${Boost_LIBRARIES} )
target_link_libraries( chatv2 ${Boost_LIBRARIES} )
