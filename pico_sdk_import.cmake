# Auto-generated helper to fetch or locate the Raspberry Pi Pico SDK.
# Uses FetchContent to download the SDK if PICO_SDK_PATH is not set.

cmake_minimum_required(VERSION 3.13)
include(FetchContent)

# Allow override via environment variable
if (DEFINED ENV{PICO_SDK_PATH})
    set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})
endif()

# Fallback to fetching the SDK alongside the project.
if (NOT PICO_SDK_PATH)
    set(PICO_SDK_PATH ${CMAKE_CURRENT_LIST_DIR}/pico-sdk)
    message(STATUS "PICO_SDK_PATH not specified; fetching Raspberry Pi Pico SDK to ${PICO_SDK_PATH}")

    set(PICO_SDK_FETCH_FROM_GIT on)
    set(FETCHCONTENT_BASE_DIR ${CMAKE_CURRENT_LIST_DIR})

    FetchContent_Declare(
        pico_sdk
        GIT_REPOSITORY https://github.com/raspberrypi/pico-sdk.git
        # RP2350 support is available from v2.0; use a recent stable tag.
        GIT_TAG 2.1.0
    )
    FetchContent_MakeAvailable(pico_sdk)
endif()

include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)
