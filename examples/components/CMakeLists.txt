include_directories(${CMAKE_SOURCE_DIR} ${Boost_INCLUDE_DIRS} ${Libmsgpack_INCLUDE_DIRS})
link_libraries(${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})

set(CALLER_SOURCES caller.cpp)
set(CALLEE_SOURCES callee.cpp)
set(PUBLISHER_SOURCES publisher.cpp)
set(SUBSCRIBER_SOURCES subscriber.cpp)

add_executable(caller ${CALLER_SOURCES})
add_executable(callee ${CALLEE_SOURCES})
add_executable(publisher ${PUBLISHER_SOURCES})
add_executable(subscriber ${SUBSCRIBER_SOURCES})

target_link_libraries(caller example_utilities)
target_link_libraries(callee example_utilities)
target_link_libraries(publisher example_utilities)
target_link_libraries(subscriber example_utilities)
