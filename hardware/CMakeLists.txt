project(hardware)

set(BOARD TRADFRI)

if (NOT DEFINED BOARD)
    message("No board defined, skipping hardware:${BOARD}")
    set(HAS_HARDWARE FALSE PARENT_SCOPE)
    return()
endif ()

set(HAS_HARDWARE TRUE PARENT_SCOPE)

message("Board defined, adding hardware library for: ${CPU_FAMILY_U}_${BOARD}")

add_library(${PROJECT_NAME}
		kit/common/drivers/retargetserial.c
		kit/common/drivers/retargetio.c)

include_directories(${PROJECT_NAME}
        halconfig/inc/hal-config
        kit/${CPU_FAMILY_U}_${BOARD}/config
        kit/common/bsp
        kit/common/drivers
        kit/common/halconfig
        module/config
        ../cmsis/Include
        ../emlib/inc
        ../device/${CPU_FAMILY_U}/Include)

target_include_directories(${PROJECT_NAME} INTERFACE
        halconfig/inc/hal-config
        kit/${CPU_FAMILY_U}_${BOARD}/config
        kit/common/bsp
        kit/common/drivers
        kit/common/halconfig
        module/config
        ../device/${CPU_FAMILY_U}/Include)
