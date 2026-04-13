# getlocked version
get_filename_component(PROJECT_CMAKE_DIR "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
get_filename_component(PROJECT_ROOT_DIR "${PROJECT_CMAKE_DIR}" DIRECTORY)
include("${PROJECT_CMAKE_DIR}/Utilities.cmake")

# FORCED FALLBACK: Using system compiler instead of bundled one
# include any-gcc-arm-none-eabi toolchain and pass in ARM_TOOLCHAIN_DIR (empty)
include("${PROJECT_ROOT_DIR}/cmake/AnyGccArmNoneEabi.cmake")
