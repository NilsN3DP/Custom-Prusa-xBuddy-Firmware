add_executable(
  autofeeder_tests
  ${CMAKE_CURRENT_SOURCE_DIR}/autofeeder_tests.cpp
  ${CMAKE_SOURCE_DIR}/src/common/feature/autofeeder/autofeeder.cpp
  ${CMAKE_SOURCE_DIR}/src/common/feature/autofeeder/autofeeder_load.cpp
  ${CMAKE_SOURCE_DIR}/src/common/feature/autofeeder/autofeeder_protocol.cpp
  )
target_include_directories(
  autofeeder_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/src/common
  )

add_catch_test(autofeeder_tests)
