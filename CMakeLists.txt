#
# Arm SCP/MCP Software
# Copyright (c) 2024, Arm Limited and Contributors. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

# cmake-lint: disable=C0301

add_library(secure-partition)

target_link_libraries(secure-partition
                      PUBLIC arch-sp)

target_include_directories(secure-partition
                           PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

target_sources(
    arch-sp PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/arch_interrupt.c"
		    "${CMAKE_CURRENT_SOURCE_DIR}/src/arch_mm.c"
		    "${CMAKE_CURRENT_SOURCE_DIR}/src/arch_main.c")

