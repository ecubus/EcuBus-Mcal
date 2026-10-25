# ##############################################################################
# vendor/ecubus/components/Can/CMakeLists.txt
#
# Copyright (c) 2025 Li Auto Inc. and its affiliates
# Licensed under the Apache License, Version 2.0(the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#     http://www.apache.org/licenses/LICENSE-2.0
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# ##############################################################################

if(CONFIG_MODULE_CAN)
    nuttx_add_library(pc_can_driver STATIC)

    set(CSRCS)
    file(GLOB CAN_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
    )
    list(APPEND CSRCS
        ${CAN_SOURCES}
        ${GENDATA_DIR}/Can/Can_PBcfg.c
    )

    set(INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/inc
        ${NUTTX_APPS_DIR}/components/include
        ${NUTTX_APPS_DIR}/components/include/autosar
        ${NUTTX_APPS_DIR}/components/rt_framework/inc
        ${NUTTX_APPS_DIR}/components/communication/net_dev/canif/inc
        ${GENDATA_DIR}/Can
        ${GENDATA_DIR}/CanIf
        ${GENDATA_DIR}/Memmap
    )

    target_sources(pc_can_driver PRIVATE ${CSRCS})
    target_include_directories(pc_can_driver PRIVATE ${INCLUDE_DIRS})
    target_include_directories(pc_can_driver PUBLIC ${CMAKE_CURRENT_LIST_DIR}/inc)

    if(WIN32)
        target_link_libraries(pc_can_driver PRIVATE ws2_32)
    endif()
endif()
