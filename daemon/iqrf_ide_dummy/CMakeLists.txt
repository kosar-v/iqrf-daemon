project(iqrf_ide_dummy)

set(iqrf_ide_dummy_SRC_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_SOURCE_DIR}/UdpChannel)
include_directories(${CMAKE_SOURCE_DIR}/UdpMessaging)

add_executable(${PROJECT_NAME} ${iqrf_ide_dummy_SRC_FILES} ${iqrf_ide_dummy_INC_FILES})

if (WIN32) 
	target_link_libraries(${PROJECT_NAME} UdpMessaging UdpChannel Ws2_32)
else()
	target_link_libraries(${PROJECT_NAME} UdpMessaging UdpChannel pthread)
endif()
