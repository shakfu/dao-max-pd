include(${CMAKE_CURRENT_SOURCE_DIR}/../../max-sdk-base/script/max-pretarget.cmake)

#############################################################
# MAX EXTERNAL
#############################################################

include_directories( 
	"${MAX_SDK_INCLUDES}"
	"${MAX_SDK_MSP_INCLUDES}"
	"${MAX_SDK_JIT_INCLUDES}"
)

file(GLOB PROJECT_SRC
	"*.h"
	"*.c"
	"*.cpp"
)

add_library( 
	${PROJECT_NAME} 
	MODULE
	${PROJECT_SRC}
)

target_compile_options(
	${PROJECT_NAME}
	PUBLIC
	$<$<PLATFORM_ID:Darwin>:-fvisibility=hidden>
	$<$<PLATFORM_ID:Darwin>:-Wmost>
	$<$<PLATFORM_ID:Darwin>:-Wno-four-char-constants>
	$<$<PLATFORM_ID:Darwin>:-Wno-unknown-pragmas>
)

target_compile_definitions(
	${PROJECT_NAME}
	PUBLIC
	TARGET_IS_MAX=1
	DENORM_WANT_FIX=1
	NO_TRANSLATION_SUPPORT=1
)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../max-sdk-base/script/max-posttarget.cmake)
