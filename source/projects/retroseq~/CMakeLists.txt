add_pd_external(
    PROJECT_SOURCE
        retroseq~pd.c
)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    include(${CMAKE_SOURCE_DIR}/source/max-sdk-base/script/max-pretarget.cmake)

    add_max_external(
        PROJECT_SOURCE
            retroseq~max.c
    )

    include(${CMAKE_SOURCE_DIR}/source/max-sdk-base/script/max-posttarget.cmake)
endif()