# smoke_wasi_minimal は常時ビルド可能（hosted 依存なし）
# WASI_MINIMAL の契約 (例外なしでビルド可能) を hosted でも検証する
add_executable(smoke_wasi_minimal smoke_wasi_minimal.cpp)
target_link_libraries(smoke_wasi_minimal PRIVATE macad-parser::macad-parser)
target_compile_features(smoke_wasi_minimal PRIVATE ${STD_CPP})
target_compile_definitions(smoke_wasi_minimal PRIVATE MACAD_PARSER_WASI_MINIMAL=1)
target_compile_options(smoke_wasi_minimal PRIVATE $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-fno-exceptions>)
add_test(NAME smoke_wasi_minimal COMMAND smoke_wasi_minimal)

if(ENABLE_WASI_MINIMAL)
  # WASI_MINIMAL ビルドでは hosted なテストはスキップ
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
