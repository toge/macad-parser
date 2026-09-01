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

if(ENABLE_FREESTANDING)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        # freestanding 検証: Catch2 を使わず libstdc++ リンクなしでビルド・実行する。
        # コア機能が動的確保やホスト専用ライブラリに逆戻りすると
        # コンパイルエラーまたはリンクエラー (operator new 等) で失敗する。
        add_executable(freestanding_check freestanding_check.cpp)
        target_link_libraries(freestanding_check PRIVATE macad-parser::macad-parser)
        target_compile_definitions(freestanding_check PRIVATE MACAD_PARSER_FREESTANDING)
        target_compile_options(freestanding_check PRIVATE -ffreestanding -fno-exceptions -fno-rtti)
        target_link_options(freestanding_check PRIVATE -nostdlib++)
        add_test(NAME freestanding_check COMMAND freestanding_check)
    else()
        message(WARNING "ENABLE_FREESTANDING check test requires GNU/Clang")
    endif()
endif()
