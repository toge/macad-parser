# smoke_no_exceptions: always built with -fno-exceptions to verify
# exception-free compilation (frozenchars-style). No Catch2 dependency.
add_executable(smoke_no_exceptions smoke_no_exceptions.cpp)
target_link_libraries(smoke_no_exceptions PRIVATE macad-parser::macad-parser)
target_compile_features(smoke_no_exceptions PRIVATE ${STD_CPP})
target_compile_options(smoke_no_exceptions PRIVATE $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-fno-exceptions>)

if(CMAKE_SYSTEM_NAME STREQUAL "WASI")
  find_program(WASMTIME_EXECUTABLE wasmtime)
  if(WASMTIME_EXECUTABLE)
    add_test(NAME smoke_no_exceptions COMMAND ${WASMTIME_EXECUTABLE} run smoke_no_exceptions)
  endif()
else()
  add_test(NAME smoke_no_exceptions COMMAND smoke_no_exceptions)
endif()

# Skip Catch2 tests on wasm32 (exceptions required by Catch2)
if(CMAKE_SYSTEM_PROCESSOR STREQUAL "wasm32")
  return()
endif()

find_package(Catch2 REQUIRED CONFIG)

file(GLOB test_src test_*.cpp)

add_executable(all_test main.cpp ${test_src})

target_link_libraries(all_test PRIVATE Catch2::Catch2 Catch2::Catch2WithMain macad-parser::macad-parser)
target_compile_features(all_test PRIVATE ${STD_CPP})

if(EMSCRIPTEN)
    # Catch2 テストのメモリ需要がデフォルトの INITIAL_MEMORY (16MB) を超えるため
    # ランタイムのメモリ拡張を許可する
    target_link_options(all_test PRIVATE -sALLOW_MEMORY_GROWTH)
endif()

add_test(NAME all_test COMMAND all_test -r junit)
