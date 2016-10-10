find_path(LZ4_INCLUDE_DIR lz4.h)

find_library(LZ4_LIBRARIES NAMES lz4)

if (LZ4_INCLUDE_DIR AND LZ4_LIBRARIES)
    set(LZ4_FOUND TRUE)
    message(STATUS "Found LZ4 library: ${LZ4_LIBRARIES}")
else ()
    message(STATUS "No lz4 found.  Using internal sources.")
endif ()
