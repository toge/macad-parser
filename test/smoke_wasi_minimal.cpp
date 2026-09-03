/**
 * @file test/smoke_wasi_minimal.cpp
 * @brief MACAD_PARSER_WASI_MINIMAL モードの検証。
 *
 * -fno-exceptions 付きでビルドされる。例外なしで全APIがコンパイル・
 * 実行できることを確認する。wasip1 では <string> を含む hosted ヘッダが
 * WASI 経由で利用可能なため、-nostdlib++ は使わず通常リンクで検証する
 * （真の bare-metal 非対応は frozenchars と同様）。
 */

#include <cstdio>
#include <string>

#include "macad-parser.hpp"

// wasm32-unknown-unknown ではヘッダ側で自動有効化される。それ以外は明示が必要。
#ifndef MACAD_PARSER_WASI_MINIMAL
#error "MACAD_PARSER_WASI_MINIMAL is not defined (build with -DENABLE_WASI_MINIMAL=ON)"
#endif

static int failed = 0;

#define CHECK(cond)                                                     \
    do {                                                                \
        if (!(cond)) {                                                  \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            ++failed;                                                   \
        }                                                               \
    } while (0)


// 小文字オプション (局所クラスには静的メンバを定義できないため名前空間スコープに置く)
struct lower_opts_impl {
    static constexpr auto validate_delimiters = false;
    static constexpr auto validate_hex        = false;
    static constexpr auto delimiter           = ':';
    static constexpr auto uppercase           = false;
};

int main() {
    using namespace macad_parser;    // 1. MAC アドレスのパース
    auto const v = parse_mac_address("AA:BB:CC:DD:EE:FF");
    CHECK(v.has_value());
    CHECK(*v == 0xAABBCCDDEEFFull);

    // 2. コンパイル時パース
    static constexpr auto mac_val = parse_mac_address("01:23:45:67:89:AB");
    static_assert(mac_val == 0x0123'4567'89ABull);

    // 3. バイト列パース
    auto const bytes = parse_mac_address_to_bytes("AA:BB:CC:DD:EE:FF");
    CHECK(bytes.has_value());
    CHECK((*bytes)[0] == 0xAA);
    CHECK((*bytes)[5] == 0xFF);

    // 4. デリミタなし形式・EUI-64
    CHECK(parse_mac_address_no_delimiter("AABBCCDDEEFF") == 0xAABBCCDDEEFFull);
    CHECK(parse_eui64_address("00:11:22:33:44:55:66:77").has_value());

    // 5. バッファ出力
    {
        char buf[MAC_ADDRESS_STRING_LENGTH];
        auto const n = format_mac_address_to_buffer(0xAABBCCDDEEFFull, std::span<char, MAC_ADDRESS_STRING_LENGTH>{buf});
        CHECK(n == MAC_ADDRESS_STRING_LENGTH);
        CHECK(buf[0] == 'A' && buf[1] == 'A' && buf[16] == 'F');
    }
    {
        char buf[EUI64_STRING_LENGTH];
        auto const n = format_eui64_to_buffer(0x0011223344556677ull, std::span<char, EUI64_STRING_LENGTH>{buf});
        CHECK(n == EUI64_STRING_LENGTH);
        CHECK(buf[0] == '0' && buf[22] == '7');
    }

    // 6. std::string 返し（wasip1 では WASI 経由で利用可能なため維持）
    {
        auto const s = format_mac_address(0xAABBCCDDEEFFull);
        CHECK(s == "AA:BB:CC:DD:EE:FF");
        auto const e = format_eui64_address(0x0011223344556677ull);
        CHECK(e.size() == EUI64_STRING_LENGTH);
    }

    // 7. ラウンドトリップ (小文字オプション)
    {
        char buf[MAC_ADDRESS_STRING_LENGTH];
        format_mac_address_to_buffer<lower_opts_impl>(0xAABBCCDDEEFFull, std::span<char, MAC_ADDRESS_STRING_LENGTH>{buf});
        CHECK(parse_mac_address<lower_opts_impl>(std::string_view{buf, MAC_ADDRESS_STRING_LENGTH}) == 0xAABBCCDDEEFFull);
    }

    if (failed == 0) std::printf("smoke_wasi_minimal: all ok\n");
    return failed == 0 ? 0 : 1;
}
