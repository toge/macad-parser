// freestanding モード検証。Catch2 を使わず -ffreestanding -fno-exceptions -fno-rtti
// -nostdlib++ (libstdc++ リンクなし) でビルド・実行できることを確認する。
// コア機能 (パース・バッファ出力) は動的確保を行わないため libstdc++ なしで完結する。
// ライブラリが std::string や動的確保に逆戻りしたらコンパイルエラーまたは
// リンクエラー (operator new 等) で失敗する。

// simde は未使用でも <cmath> を取り込むため、GCC 16 のように <cmath> を hosted 専用と
// する実装では -ffreestanding で失敗する。HUGE_VAL を先に定義して simde の
// 「<cmath> 済み」検出ブランチへ誘導し、<cmath> の取り込み自体を回避する。
// (macad-parser のコア機能は math 関数を一切使わないため問題にならない)
// hosted 環境 (wasm 含む) では本物の <cmath> を使わせるため無効化する。
// SIMDE_NO_NATIVE でネイティブ x86 ヘッダ (mm_malloc.h の malloc 参照) の取り込みも抑止する。
#if defined(__STDC_HOSTED__) && !__STDC_HOSTED__
#define HUGE_VAL (__builtin_huge_val())
#endif
#define SIMDE_NO_NATIVE 1

#include <cstdio>

#include "macad-parser.hpp"

// wasm32-unknown-unknown ではヘッダ側で自動有効化される。それ以外は明示が必要。
#ifndef MACAD_PARSER_FREESTANDING
#error "MACAD_PARSER_FREESTANDING is not defined (build with -DENABLE_FREESTANDING=ON)"
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

    // 5. バッファ出力 (動的確保なし)
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

    // 6. ラウンドトリップ (小文字オプション)
    {
        char buf[MAC_ADDRESS_STRING_LENGTH];
        format_mac_address_to_buffer<lower_opts_impl>(0xAABBCCDDEEFFull, std::span<char, MAC_ADDRESS_STRING_LENGTH>{buf});
        CHECK(parse_mac_address<lower_opts_impl>(std::string_view{buf, MAC_ADDRESS_STRING_LENGTH}) == 0xAABBCCDDEEFFull);
    }

    if (failed == 0) std::printf("freestanding_check: all ok\n");
    return failed == 0 ? 0 : 1;
}
