if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    unset(CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "requires >= 10.15" FORCE)
    set(CMAKE_EXPORT_COMPILE_COMMANDS True)
endif()

function(add_pd_external)

    set(options
        DEBUG
        GENERIC_EXT # exclude architecture in extension
    )
    set(oneValueArgs 
        PROJECT_NAME
        PROJECT_TARGET
        FLOATSIZE # default 32
        EXTENSION
    )
    set(multiValueArgs
        PROJECT_SOURCE
        OTHER_SOURCE
        INCLUDE_DIRS
        LINK_LIBS
        LINK_DIRS
        COMPILE_DEFINITIONS
        COMPILE_OPTIONS
        LINK_OPTIONS
        PATCH_FILES
        DATA_FILES
    )

    cmake_parse_arguments(
        PD
        "${options}"
        "${oneValueArgs}"
        "${multiValueArgs}"
        ${ARGN}
    )

    if(NOT DEFINED PD_PROJECT_NAME)
        string(REGEX REPLACE "(.*)/" "" PD_PROJECT_NAME "${CMAKE_CURRENT_SOURCE_DIR}")
    endif()
    string(REPLACE "~" "_tilde" PD_PROJECT_NAME_TILDE "${PD_PROJECT_NAME}")
    project(${PD_PROJECT_NAME_TILDE})

    if(NOT DEFINED PD_PROJECT_TARGET)
        set(PD_PROJECT_TARGET "pd.${PD_PROJECT_NAME_TILDE}")
    endif()

    message(STATUS "PD_PROJECT_NAME: ${PD_PROJECT_NAME}")
    message(STATUS "PD_PROJECT_NAME_TILDE: ${PD_PROJECT_NAME_TILDE}")
    message(STATUS "PD_PROJECT_TARGET: ${PD_PROJECT_TARGET}")
    if(DEBUG)
        message(STATUS "PD_ARCH_EXT: ${PD_ARCH_EXT}")
        message(STATUS "PD_PROJECT_SOURCE: ${PD_PROJECT_SOURCE}")
        message(STATUS "PD_OTHER_SOURCE: ${PD_OTHER_SOURCE}")
        message(STATUS "PD_INCLUDE_DIRS: ${PD_INCLUDE_DIRS}")
        message(STATUS "PD_COMPILE_DEFINITIONS: ${PD_COMPILE_DEFINITIONS}")
        message(STATUS "PD_COMPILE_OPTIONS: ${PD_COMPILE_OPTIONS}")
        message(STATUS "PD_LINK_LIBS: ${PD_LINK_LIBS}")
        message(STATUS "PD_LINK_DIRS: ${PD_LINK_DIRS}")
        message(STATUS "PD_LINK_OPTIONS: ${PD_LINK_OPTIONS}")
        message(STATUS "PD_PATCH_FILES: ${PD_PATCH_FILES}")
        message(STATUS "PD_DATA_FILES: ${PD_DATA_FILES}")
    endif()

    set(PD_DIR "${CMAKE_SOURCE_DIR}/source/pd")
    set(EXTERNALS_DIR "${CMAKE_SOURCE_DIR}/externals/pd/${PD_PROJECT_NAME}")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${EXTERNALS_DIR}")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG "${EXTERNALS_DIR}")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE "${EXTERNALS_DIR}")



    if(NOT DEFINED PD_PROJECT_SOURCE)
        file(GLOB PD_PROJECT_SOURCE
            "*.h"
            "*.c"
            "*.cpp"
        )
    endif()

    if(NOT DEFINED PD_PATCH_FILES)
        file(GLOB PD_PATCH_FILES
            "*.pd"
        )
    endif()

    if(NOT DEFINED PD_DATA_FILES)
        file(GLOB PD_DATA_FILES
            "*.wav"
            "*.aif"
            "*.aiff"
        )
    endif()

    include_directories( 
        ${PD_DIR}/include
    )

    add_library( 
        ${PD_PROJECT_TARGET} 
        MODULE
        ${PD_PROJECT_SOURCE}
        ${PD_OTHER_SOURCE}
    )

    set_target_properties(
        ${PD_PROJECT_TARGET}
        PROPERTIES
        PREFIX ""
        OUTPUT_NAME "${PD_PROJECT_NAME}"
        LIBRARY_OUTPUT_DIRECTORY ${EXTERNALS_DIR}
    )

    foreach(patch_file ${PD_PATCH_FILES})
        file(COPY ${patch_file}  
            DESTINATION ${EXTERNALS_DIR}
        )
    endforeach()

    foreach(data_file ${PD_DATA_FILES})
        file(COPY ${data_file}  
            DESTINATION ${EXTERNALS_DIR}
        )
    endforeach()

    if (NOT DEFINED PD_FLOATSIZE)
        set(PD_FLOATSIZE 32)
    endif()

    if(GENERIC_EXT)
        if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
            set_target_properties(${PD_PROJECT_TARGET} PROPERTIES SUFFIX ".pd_darwin")
        elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
            set_target_properties(${PD_PROJECT_TARGET} PROPERTIES SUFFIX ".pd_linux")
        elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
            set_target_properties(${PD_PROJECT_TARGET} PROPERTIES SUFFIX ".dll")
        endif()
    else()
        # section extracted from pd.cmake
        # https://github.com/pure-data/pd.cmake
        # this section has GPL license
        # https://github.com/pure-data/pd.cmake/blob/main/LICENSE 
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64" AND CMAKE_SYSTEM_NAME MATCHES "Linux")
            set(CMAKE_SYSTEM_NAME Linux)
            set(CMAKE_SYSTEM_PROCESSOR aarch64)
            find_program(AARCH64_GCC NAMES aarch64-linux-gnu-gcc)
            find_program(AARCH64_GXX NAMES aarch64-linux-gnu-g++)
            if(AARCH64_GCC AND AARCH64_GXX)
                set(CMAKE_C_COMPILER ${AARCH64_GCC})
                set(CMAKE_CXX_COMPILER ${AARCH64_GXX})
            else()
                message(FATAL_ERROR "Cross-compilers for aarch64 architecture not found.")
            endif()
        elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm" AND CMAKE_SYSTEM_NAME MATCHES "Linux")
            set(CMAKE_SYSTEM_NAME Linux)
            set(CMAKE_SYSTEM_PROCESSOR arm)
            find_program(ARM_GCC NAMES arm-linux-gnueabihf-gcc)
            find_program(ARM_GXX NAMES arm-linux-gnueabihf-g++)
            if(ARM_GCC AND ARM_GXX)
                set(CMAKE_C_COMPILER ${ARM_GCC})
                set(CMAKE_CXX_COMPILER ${ARM_GXX})
            else()
                message(FATAL_ERROR "ERROR: Cross-compilers for ARM architecture not found.")
            endif()
        endif()

        if(PD_EXTENSION)
            set_target_properties(${PD_PROJECT_TARGET} PROPERTIES SUFFIX ".${PD_EXTENSION}")
        else()
            if(NOT (PD_FLOATSIZE EQUAL 64 OR PD_FLOATSIZE EQUAL 32))
                message(FATAL_ERROR "PD_FLOATSIZE must be 32 or 64")
            endif()

            if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
                if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
                    set(PD_EXTENSION ".darwin-arm64-${PD_FLOATSIZE}.so")
                else()
                    set(PD_EXTENSION ".darwin-amd64-${PD_FLOATSIZE}.so")
                endif()

            elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
                if(CMAKE_SIZEOF_VOID_P EQUAL 4) # 32-bit
                    if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm")
                        set(PD_EXTENSION ".linux-arm-${PD_FLOATSIZE}.so")
                    endif()
                else()
                    if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
                        set(PD_EXTENSION ".linux-arm64-${PD_FLOATSIZE}.so")
                    else()
                        set(PD_EXTENSION ".linux-amd64-${PD_FLOATSIZE}.so")
                    endif()
                endif()
            elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
                if(CMAKE_SIZEOF_VOID_P EQUAL 4)
                    set(PD_EXTENSION ".windows-i386-${PD_FLOATSIZE}.dll")
                else()
                    set(PD_EXTENSION ".windows-amd64-${PD_FLOATSIZE}.dll")
                endif()
            endif()

            if(NOT PD_EXTENSION)
                message(
                    FATAL_ERROR
                        "Not possible to determine the extension of the library, please set PD_EXTENSION"
                )
            endif()
            set_target_properties(${PD_PROJECT_TARGET} PROPERTIES SUFFIX ${PD_EXTENSION})
        endif()
    endif()

    target_include_directories(
        ${PD_PROJECT_TARGET}
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PD_INCLUDE_DIRS}
        /sw/include
    )

    target_compile_definitions(
        ${PD_PROJECT_TARGET}
        PRIVATE
        ${PD_COMPILE_DEFINITIONS}
        PD_FLOATSIZE=${PD_FLOATSIZE}
        $<$<CONFIG:Release>:NDEBUG>
        $<$<PLATFORM_ID:Darwin>:UNIX>
        $<$<PLATFORM_ID:Darwin>:MACOSX>
        $<$<PLATFORM_ID:Linux>:UNIX>
    )

    target_compile_options(
        ${PD_PROJECT_TARGET}
        PRIVATE
        ${PD_COMPILE_OPTIONS}
        $<$<PLATFORM_ID:Linux>:-fPIC>
        $<$<PLATFORM_ID:Linux>:-fcheck-new>
    )

    target_link_directories(
        ${PD_PROJECT_TARGET} 
        PRIVATE
        ${PD_LINK_DIRS}
        $<$<PLATFORM_ID:Windows>:${PD_DIR}/lib>
    )

    target_link_options(
        ${PD_PROJECT_TARGET}
        PRIVATE
        ${PD_LINK_OPTIONS}
        $<$<PLATFORM_ID:Darwin>:-undefined dynamic_lookup>
        $<$<PLATFORM_ID:Linux>:-rdynamic>
        $<$<PLATFORM_ID:Linux>:-shared>
        $<$<PLATFORM_ID:Linux>:-fPIC>
    )

    target_link_libraries(
        ${PD_PROJECT_TARGET} 
        PRIVATE
        "${PD_LINK_LIBS}"
        $<$<PLATFORM_ID:Windows>:pd>
        $<$<PLATFORM_ID:Linux,Darwin>:-lm>
    )

endfunction()
