include_directories(${CMAKE_SOURCE_DIR} ${Boost_INCLUDE_DIR})
link_libraries(${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})

set(PARAMETERS_SOURCES parameters.cpp)
set(PARAMETERS_HEADERS parameters.hpp)
add_library(examples_parameters ${PARAMETERS_SOURCES} ${PARAMETERS_HEADERS})

add_library(acs acs/getInterfaceAddresses.hpp
                acs/getInterfaceAddresses.cpp
                acs/CServiceDiscoveryManager.hpp
                acs/CServiceDiscoveryManager.cpp
                acs/CDomainInfo.hpp
                acs/CDomainInfo.cpp
                acs/CInterfaceChangeNotifier.hpp
                acs/CInterfaceChangeNotifier.cpp
                acs/CBackplane.hpp
                acs/CBackplane.cpp)
include_directories(${CMAKE_SOURCE_DIR}/examples)


set(CALLER_SOURCES caller.cpp)
set(PROXY_CALLER_SOURCES proxyCaller.cpp)
set(CALLEE_SOURCES callee.cpp)
set(PUBLISHER_SOURCES publisher.cpp)
set(SUBSCRIBER_SOURCES subscriber.cpp)

add_executable(caller ${CALLER_SOURCES} ${PUBLIC_HEADERS})
add_executable(proxyCaller ${PROXY_CALLER_SOURCES} ${PUBLIC_HEADERS})
add_executable(callee ${CALLEE_SOURCES} ${PUBLIC_HEADERS})
add_executable(backplane backplane.cpp ${PUBLIC_HEADERS} )
add_executable(publisher ${PUBLISHER_SOURCES} ${PUBLIC_HEADERS})
add_executable(subscriber ${SUBSCRIBER_SOURCES} ${PUBLIC_HEADERS})

target_link_libraries(caller examples_parameters)
target_link_libraries(proxyCaller examples_parameters)
target_link_libraries(callee examples_parameters)
target_link_libraries(backplane examples_parameters acs)
target_link_libraries(publisher examples_parameters)
target_link_libraries(subscriber examples_parameters)
