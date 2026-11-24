add_compile_definitions(UNITTEST)
add_executable(
  Marlin_phase_stepping_tests
  ${CMAKE_CURRENT_SOURCE_DIR}/float_vs_fixed.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/lut_old.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/rounding.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/lut_saturation.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/speed_model_test.cpp
  ${CMAKE_SOURCE_DIR}/lib/Marlin/Marlin/src/feature/phase_stepping/lut.cpp
  ${CMAKE_SOURCE_DIR}/lib/Marlin/Marlin/src/feature/phase_stepping/speed_model.cpp
  ${CMAKE_SOURCE_DIR}/lib/Marlin/Marlin/src/feature/phase_stepping/calibration_speed.cpp
  )
target_include_directories(
  Marlin_phase_stepping_tests
  PUBLIC . ${CMAKE_SOURCE_DIR}/include
         ${CMAKE_SOURCE_DIR}/lib/Marlin/Marlin/src/feature/phase_stepping
  )
add_catch_test(Marlin_phase_stepping_tests)
