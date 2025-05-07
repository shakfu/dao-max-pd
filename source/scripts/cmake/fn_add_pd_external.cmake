if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    unset(CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "requires >= 10.15" FORCE)
    set(CMAKE_EXPORT_COMPILE_COMMANDS True)
endif()

function(add_pd_external)

    set(options
        DEBUG
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
        PATCH_FILES
    )

    cmake_parse_arguments(
        PD
        "${options}"
        "${oneValueArgs}"
        "${multiValueArgs}"
        ${ARGN}
    )

    if(NOT DEFINED PD_PROJECT_NAME)
        string(REGEX REPLACE "(.*)/" "" THIS_FOLDER_NAME "${CMAKE_CURRENT_SOURCE_DIR}")
        string(REPLACE "~" "_tilde" THIS_FOLDER_NAME "${THIS_FOLDER_NAME}")
        string(REPLACE "_tilde" "" STEM "${THIS_FOLDER_NAME}")
        project(${THIS_FOLDER_NAME})

        if ("${PROJECT_NAME}" MATCHES ".*_tilde")
            string(REGEX REPLACE "_tilde" "~" EXTERN_OUTPUT_NAME_DEFAULT "${PROJECT_NAME}")
        else ()
            set(EXTERN_OUTPUT_NAME_DEFAULT "${PROJECT_NAME}")
        endif ()
        set(PD_PROJECT_NAME pd.${PROJECT_NAME})
    endif()

    message(STATUS "PROJECT_NAME: ${PD_PROJECT_NAME}")
    if(DEBUG)
        message(STATUS "PROJECT_SOURCE: ${PD_PROJECT_SOURCE}")
        message(STATUS "OTHER_SOURCE: ${PD_OTHER_SOURCE}")
        message(STATUS "PD_INCLUDE_DIRS: ${PD_INCLUDE_DIRS}")
        message(STATUS "PD_COMPILE_DEFINITIONS: ${PD_COMPILE_DEFINITIONS}")
        message(STATUS "PD_COMPILE_OPTIONS: ${PD_COMPILE_OPTIONS}")
        message(STATUS "PD_LINK_LIBS: ${PD_LINK_LIBS}")
        message(STATUS "PD_LINK_DIRS: ${PD_LINK_DIRS}")
        message(STATUS "PD_LINK_OPTIONS: ${PD_LINK_OPTIONS}")
        message(STATUS "PD_PATCH_FILES: ${PD_PATCH_FILES}")
    endif()

    set(DEPS_DIR "${CMAKE_SOURCE_DIR}/build/install")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/externals/pd/${STEM}")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
    set(EXTERNAL_DIR "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
    set(SUPPORT_DIR "${CMAKE_SOURCE_DIR}/support")


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


    include_directories( 
        ${CMAKE_SOURCE_DIR}/source/include
    )

    add_library( 
        ${PD_PROJECT_NAME} 
        MODULE
        ${PD_PROJECT_SOURCE}
        ${PD_OTHER_SOURCE}
    )

    set_target_properties(
        ${PD_PROJECT_NAME}
        PROPERTIES
        PREFIX ""
        OUTPUT_NAME "${PROJECT_NAME}"
        LIBRARY_OUTPUT_DIRECTORY ${EXTERNAL_DIR}
    )

    foreach(patch_file ${PD_PATCH_FILES})
        file(COPY ${patch_file}  
            DESTINATION ${EXTERNAL_DIR}
        )
    endforeach()



    if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        set_target_properties(${PD_PROJECT_NAME} PROPERTIES SUFFIX ".pd_darwin")
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
        set_target_properties(${PD_PROJECT_NAME} PROPERTIES SUFFIX ".pd_windows")
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set_target_properties(${PD_PROJECT_NAME} PROPERTIES SUFFIX ".pd_linux")
    endif()


    target_include_directories(
        ${PD_PROJECT_NAME}
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PD_INCLUDE_DIRS}
        /sw/include
    )

    target_compile_definitions(
        ${PD_PROJECT_NAME}
        PRIVATE
        ${PD_COMPILE_DEFINITIONS}
        TARGET_IS_PD=1
        PD_FLOATSIZE=32
        $<$<PLATFORM_ID:Darwin>:NDEBUG>
        $<$<PLATFORM_ID:Darwin>:UNIX>
        $<$<PLATFORM_ID:Darwin>:MACOSX>
        # $<$<PLATFORM_ID:Darwin>:NDEBUG>
        $<$<PLATFORM_ID:Linux>:UNIX>
    )

    target_compile_options(
        ${PD_PROJECT_NAME}
        PRIVATE
        ${PD_COMPILE_OPTIONS}
        $<$<PLATFORM_ID:Linux>:-fPIC>
        $<$<PLATFORM_ID:Linux>:-fcheck-new>
    )

    target_link_directories(
        ${PD_PROJECT_NAME} 
        PRIVATE
        ${PD_LINK_DIRS}
    )

    target_link_options(
        ${PD_PROJECT_NAME}
        PRIVATE
        ${PD_LINK_OPTIONS}
        # $<$<PLATFORM_ID:Darwin>:-dynamiclib>
        $<$<PLATFORM_ID:Darwin>:-undefined dynamic_lookup>
        # $<$<PLATFORM_ID:Darwin>:-undefined suppress>
        # $<$<PLATFORM_ID:Darwin>:-flat_namespace>
        # $<$<PLATFORM_ID:Darwin>:-bundle>
        $<$<PLATFORM_ID:Linux>:-rdynamic>
        $<$<PLATFORM_ID:Linux>:-shared>
        $<$<PLATFORM_ID:Linux>:-fPIC>
    )

    target_link_libraries(
        ${PD_PROJECT_NAME} 
        PRIVATE
        "${PD_LINK_LIBS}"
        # "$<$<PLATFORM_ID:Darwin>:-framework CoreAudio>"
        # "$<$<PLATFORM_ID:Darwin>:-framework CoreMIDI>"
        # "$<$<PLATFORM_ID:Darwin>:-framework CoreFoundation>"    
        # "$<$<PLATFORM_ID:Darwin>:-framework IOKit>"
        # "$<$<PLATFORM_ID:Darwin>:-framework Carbon>"
        # "$<$<PLATFORM_ID:Darwin>:-framework AppKit>"
        # "$<$<PLATFORM_ID:Darwin>:-framework Foundation>"
        # "$<$<PLATFORM_ID:Darwin>:-F/System/Library/PrivateFrameworks>"
        # "$<$<PLATFORM_ID:Darwin>:-weak_framework MultitouchSupport>"
        # # $<$<PLATFORM_ID:Darwin>:-lc++>
        # $<$<PLATFORM_ID:Linux>:-lstdc++>
        # $<$<PLATFORM_ID:Linux>:-lc>
        $<$<PLATFORM_ID:Linux,Darwin>:-lm>
    )

endfunction()
