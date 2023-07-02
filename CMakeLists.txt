cmake_minimum_required(VERSION 3.13)

project(ddfb VERSION 0.8.1)

include(FetchContent)

# --- fetch deCONZ library from GitHub --------------------------------

FetchContent_Declare(
  libuecc
  GIT_REPOSITORY https://github.com/kmackay/micro-ecc.git
  GIT_TAG        v1.1
)

# FetchContent_MakeAvailable(uecc)
# FetchContent_MakeAvailable requires CMake 3.14, but Debian Buster has only 3.13
FetchContent_GetProperties(libuecc)
if (NOT libuecc_POPULATED)
    FetchContent_Populate(libuecc)
endif()

add_library(uECC STATIC ${libuecc_SOURCE_DIR}/uECC.c)
target_include_directories(uECC PUBLIC ${libuecc_SOURCE_DIR})

#----------------------------------------------------------------------

add_executable(ddfb ddfb.c)

target_link_libraries(ddfb PRIVATE uECC)

if (CMAKE_HOST_UNIX)
    target_compile_definitions(ddfb PRIVATE PL_POSIX)
    target_link_libraries(ddfb PRIVATE m)

#    add_custom_command(TARGET ddfb POST_BUILD COMMAND ${CMAKE_STRIP} $<TARGET_FILE:ddfb>)

endif (CMAKE_HOST_UNIX)

if (WIN32)
	target_link_libraries(ddfb PRIVATE bcrypt)
endif (WIN32)
