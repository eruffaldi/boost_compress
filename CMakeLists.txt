set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR})

#add_definitions(-march=native)

add_definitions(-std=c++11)
find_package(Boost COMPONENTS system iostreams REQUIRED)
link_libraries(${Boost_LIBRARIES})
include_directories(${Boost_INCLUDE_DIRS})

find_package(LZ4 REQUIRED)
link_libraries(${LZ4_LIBRARY})
include_directories(${LZ4_INCLUDES})

link_libraries(z)

add_executable(test test.cpp lz4_filter.cpp)
target_link_libraries(test lz4)