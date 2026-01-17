cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Global fix for GCC 14 warnings in Bootloader and Components
idf_build_set_property(COMPILE_OPTIONS "-Wno-error" "-Wno-format" APPEND)

project(ndn_mesh_node)