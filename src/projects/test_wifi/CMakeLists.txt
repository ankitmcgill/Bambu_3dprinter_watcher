# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Git Branch & Hash
execute_process(
    COMMAND
        bash -c "git rev-parse --abbrev-ref HEAD"
    OUTPUT_VARIABLE
        GIT_BRANCH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
execute_process(
    COMMAND
        bash -c "git rev-parse --short HEAD"
    OUTPUT_VARIABLE
        GIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
execute_process(
    COMMAND
        bash -c "git tag --points-at HEAD"
    OUTPUT_VARIABLE
        GIT_TAG
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
add_definitions(-DGIT_BRANCH="${GIT_BRANCH}" -DGIT_HASH="${GIT_HASH}" -DGIT_TAG="${GIT_TAG}")

set(EXTRA_COMPONENT_DIRS
    # Driver Code
    "../../common/driver/"

    # Module Code
    "../../common/module/"

    # Util Code
    "../../common/util/"

    # Others
    "../../common/others/"

    # Project Defines
    "./project_defines/"
)

# Project Version
set(PROJECT_VER "0.1")

# "Trim" the build. Include the minimal set of components, main, and anything it depends on.
idf_build_set_property(MINIMAL_BUILD ON)
project(Test_Wifi)