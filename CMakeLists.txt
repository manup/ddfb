cmake_minimum_required(VERSION 3.5)
project (ddfb)


add_library(uECC STATIC
    vendor/micro-ecc/uECC.c
)

target_include_directories(uECC
    PUBLIC
        vendor/micro-ecc/
)

add_executable(ddfb ddfb.c)
set_property(TARGET ddfb PROPERTY C_STANDARD 11)

target_link_libraries(ddfb PRIVATE uECC)

if (WIN32)
	target_link_libraries(ddfb PRIVATE bcrypt)
endif (WIN32)
