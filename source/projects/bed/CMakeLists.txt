add_pd_external(
    PROJECT_SOURCE
        bed_pd.c
)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    include(${CMAKE_SOURCE_DIR}/source/max-sdk-base/script/max-pretarget.cmake)

    add_max_external(
        PROJECT_SOURCE
            bed_max.c
    )

    include(${CMAKE_SOURCE_DIR}/source/max-sdk-base/script/max-posttarget.cmake)
endif()