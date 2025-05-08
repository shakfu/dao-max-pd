add_pd_external(
    PROJECT_SOURCE
        mirror~pd.c
)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    include(${CMAKE_SOURCE_DIR}/source/max-sdk-base/script/max-pretarget.cmake)

    add_max_external(
        PROJECT_SOURCE
            mirror~max.c
    )

    include(${CMAKE_SOURCE_DIR}/source/max-sdk-base/script/max-posttarget.cmake)
endif()