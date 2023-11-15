cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(app LANGUAGES C VERSION 0.1.0)
zephyr_compile_options(-I${CMAKE_CURRENT_SOURCE_DIR}/../wolf)

target_sources(app PRIVATE
    src/main.c
    src/selftest.c
    src/debug.c
    src/lpc.c
    src/tpm/plat/cancel.c
    src/tpm/plat/clock.c
    src/tpm/plat/entropy.c
    src/tpm/plat/locality.c
    src/tpm/plat/nv.c
    src/tpm/plat/physical_presence.c
    src/tpm/plat/power.c
    src/tpm/plat/run_command.c
    src/tpm/plat/unique.c
    src/tpm/init.c
)
target_include_directories(app PRIVATE include)
target_link_libraries(app PRIVATE ms-tpm-20-ref)
target_compile_definitions(app PRIVATE WOLFSSL_USER_SETTINGS)
