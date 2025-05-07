
unset(CMAKE_OSX_DEPLOYMENT_TARGET)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "requires >= 10.15" FORCE)
set(CMAKE_EXPORT_COMPILE_COMMANDS True)

function(add_max_external)

    set(options
        DEBUG
        INCLUDE_COMMONSYMS
    )
    set(oneValueArgs 
        PROJECT_NAME
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
    )

    cmake_parse_arguments(
        MAX
        "${options}"
        "${oneValueArgs}"
        "${multiValueArgs}"
        ${ARGN}
    )

    if(NOT DEFINED MAX_PROJECT_NAME)
        set(MAX_PROJECT_NAME ${PROJECT_NAME})
    endif()

    message(STATUS "PROJECT_NAME: ${MAX_PROJECT_NAME}")
    if(DEBUG)
        message(STATUS "PROJECT_SOURCE: ${MAX_PROJECT_SOURCE}")
        message(STATUS "OTHER_SOURCE: ${MAX_OTHER_SOURCE}")
        message(STATUS "MAX_INCLUDE_DIRS: ${MAX_INCLUDE_DIRS}")
        message(STATUS "MAX_COMPILE_DEFINITIONS: ${MAX_COMPILE_DEFINITIONS}")
        message(STATUS "MAX_COMPILE_OPTIONS: ${MAX_COMPILE_OPTIONS}")
        message(STATUS "MAX_LINK_LIBS: ${MAX_LINK_LIBS}")
        message(STATUS "MAX_LINK_DIRS: ${MAX_LINK_DIRS}")
        message(STATUS "MAX_LINK_OPTIONS: ${MAX_LINK_OPTIONS}")
    endif()

    set(DEPS_DIR "${CMAKE_SOURCE_DIR}/build/install")
    set(EXTERNAL_DIR "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${MAX_PROJECT_NAME}.mxo")
    set(SUPPORT_DIR "${CMAKE_SOURCE_DIR}/support")

    if(NOT DEFINED MAX_PROJECT_SOURCE)
        file(GLOB MAX_PROJECT_SOURCE
            "*.h"
            "*.c"
            "*.cpp"
        )
    endif()

    include_directories( 
        "${MAX_SDK_INCLUDES}"
        "${MAX_SDK_MSP_INCLUDES}"
        "${MAX_SDK_JIT_INCLUDES}"
    )

    add_library( 
        ${MAX_PROJECT_NAME} 
        MODULE
        ${MAX_PROJECT_SOURCE}
        ${MAX_OTHER_SOURCE}
        $<$<BOOL:${MAX_INCLUDE_COMMONSYMS}>:${MAX_SDK_INCLUDES}/common/commonsyms.c>
    )

    target_include_directories(
        ${MAX_PROJECT_NAME}
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${MAX_INCLUDE_DIRS}
    )

    target_compile_definitions(
        ${MAX_PROJECT_NAME}
        PRIVATE
        ${MAX_COMPILE_DEFINITIONS}
        TARGET_IS_MAX=1
        DENORM_WANT_FIX=1
        NO_TRANSLATION_SUPPORT=1
        $<$<BOOL:${MAX_INCLUDE_COMMONSYMS}>:INCLUDE_COMMONSYMS>
        $<$<PLATFORM_ID:Darwin>:NDEBUG>
    )

    target_compile_options(
        ${MAX_PROJECT_NAME}
        PRIVATE
        ${MAX_COMPILE_OPTIONS}
        $<$<PLATFORM_ID:Windows>:/O2>
        $<$<PLATFORM_ID:Windows>:/MT>
        $<$<PLATFORM_ID:Darwin>:-Wmost>
        $<$<PLATFORM_ID:Darwin>:-Wno-four-char-constants>
        $<$<PLATFORM_ID:Darwin>:-Wno-unknown-pragmas>
        $<$<PLATFORM_ID:Darwin>:-Wdeclaration-after-statement>
        $<$<PLATFORM_ID:Darwin>:-fvisibility=hidden>
    )

    target_link_directories(
        ${MAX_PROJECT_NAME} 
        PRIVATE
        ${MAX_LINK_DIRS}
    )

    target_link_options(
        ${MAX_PROJECT_NAME}
        PRIVATE
        ${MAX_LINK_OPTIONS}
    )

    target_link_libraries(
        ${MAX_PROJECT_NAME} 
        PRIVATE
        "${MAX_LINK_LIBS}"
    )



endfunction()
