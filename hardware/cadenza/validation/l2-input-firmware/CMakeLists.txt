cmake_minimum_required(VERSION 3.16)

# Keep this validation image isolated and small; it is not CadenzaOS firmware.
set(COMPONENTS main)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(cadenza_l2_input_validation)
