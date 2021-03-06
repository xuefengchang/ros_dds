cmake_minimum_required(VERSION 2.8)
project(realtime)
set (CMAKE_CXX_FLAGS "-std=c++11")

find_package(rttest REQUIRED)

link_directories(${rttest_LIBRARY_DIR})
include_directories(${rttest_INCLUDE_DIRS})

find_package(opensplice REQUIRED)
include_directories(${OPENSPLICE_INCLUDE_DIRS})

ospl_add_idl(idl/LargeMsg.idl)

add_executable(ospl_lms ospl_lms.cpp)
target_link_libraries(ospl_lms ${OPENSPLICE_LIBRARIES} ${rttest_LIBRARIES})

add_executable(ospl_lmp ospl_lmp.cpp)
target_link_libraries(ospl_lmp ${OPENSPLICE_LIBRARIES} ${rttest_LIBRARIES})

find_package(Threads)
if(NOT Threads_FOUND)
	message(WARNING "pthread not found. Real-time intra-process pub/sub example will not be built.")
elseif(Threads_FOUND)
	add_executable(ospl_intraproc ospl_intraproc.cpp)
	target_link_libraries(ospl_intraproc ${OPENSPLICE_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} ${rttest_LIBRARIES})
endif()
