add_pd_external(
    PROJECT_SOURCE
        moogvcf~pd.c
)

include(${CMAKE_SOURCE_DIR}/source/max-sdk-base/script/max-pretarget.cmake)

add_max_external(
    PROJECT_SOURCE
        moogvcf~max.c
)

include(${CMAKE_SOURCE_DIR}/source/max-sdk-base/script/max-posttarget.cmake)
