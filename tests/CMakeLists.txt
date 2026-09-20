find_package(Threads REQUIRED)

add_executable(test_core test_core.c)
target_include_directories(test_core PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_core core)

add_executable(test_framing test_framing.c)
target_include_directories(test_framing PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_framing core)

add_executable(test_transport test_transport.c)
target_include_directories(test_transport PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_transport core Threads::Threads)

add_executable(test_tls test_tls.c)
target_include_directories(test_tls PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_tls core hostsupport)

add_executable(test_proto test_proto.c)
target_include_directories(test_proto PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_proto core)

add_executable(test_session test_session.c)
target_include_directories(test_session PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_session core hostsupport)

add_executable(test_control test_control.c)
target_include_directories(test_control PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_control core hostsupport)

add_executable(test_discover test_discover.c)
target_include_directories(test_discover PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_discover core)

add_executable(test_input test_input.c)
target_include_directories(test_input PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_input core)

add_test(NAME core_test COMMAND test_core)
add_test(NAME framing_test COMMAND test_framing)
add_test(NAME transport_test COMMAND test_transport)
add_test(NAME tls_test COMMAND test_tls)
add_test(NAME proto_test COMMAND test_proto)
add_test(NAME session_test COMMAND test_session)
add_test(NAME control_test COMMAND test_control)
add_test(NAME discover_test COMMAND test_discover)
add_test(NAME input_test COMMAND test_input)
add_test(NAME harness_mock_test COMMAND aa_harness --mock --output ${CMAKE_BINARY_DIR}/mock_out.h264)
