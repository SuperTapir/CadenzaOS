cmake_minimum_required(VERSION 3.16)

# This bring-up target is intentionally ESP32-S3-only.
set(IDF_TARGET "esp32s3" CACHE STRING "ESP-IDF target")
# Build only the validation app and its declared dependencies.  This keeps the
# breadboard tool independent of unrelated Wi-Fi/TLS components.
set(COMPONENTS main)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(cadenza_l1_display_bringup)
