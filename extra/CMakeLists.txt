# EFM32 extralib CMake file
project(extra)

#find ./src -name '*.c' | sort -u

AUX_SOURCE_DIRECTORY(./src SOURCE_FILES)
add_library(${PROJECT_NAME} ${SOURCE_FILES})

target_include_directories(${PROJECT_NAME} PUBLIC 
		inc
		../cmsis/Include
		../emlib/inc
		../device/${CPU_FAMILY_U}/Include)
target_include_directories(${PROJECT_NAME} INTERFACE inc)
#target_link_libraries(${PROJECT_NAME} extra)
target_include_directories(${PROJECT_NAME} INTERFACE
        inc
        ../device/EFR32MG1P/Include)
