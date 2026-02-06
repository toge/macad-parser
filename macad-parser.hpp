#ifndef MACAD_PARSER_HPP
#define MACAD_PARSER_HPP

#include <cstdint>
#include <string_view>
#include <optional>
#include <bit>
#include <array>
#include <cstring>

#include "simde/x86/avx2.h"

namespace macad_parser {

/**
 * @brief MACアドレスパースオプションのデフォルト設定
 */
struct parse_mac_options {
  static constexpr bool validate_delimiters = false;
  static constexpr bool validate_hex = false;
  static constexpr char delimiter = ':';
};

/**
 * @brief デリミタと16進数文字の厳密な検証を行うオプション設定
 */
struct parse_mac_options_strict {
  static constexpr bool validate_delimiters = true;
  static constexpr bool validate_hex = true;
  static constexpr char delimiter = ':';
};

/**
 * @brief MACアドレスを示す文字列をパースして48bit整数に変換する
 *
 * SIMDEを利用してAVX2命令を抽象化し、ARM環境でも動作するようにしたMACパース
 * 最後の48bit合成まで完全にベクトル演算（SIMDE経由）で行います
 *
 * @tparam Options パースの仕方を指定するオプション
 * @param mac_str パース対象のMACアドレス文字列 (例: "AA:BB:CC:DD:EE:FF")
 * @return std::optional<uint64_t>
 */
template <typename Options = parse_mac_options>
[[nodiscard]]
auto parse_mac_address_unsafe(std::string_view const mac) noexcept -> std::optional<uint64_t> {
  if (mac.size() < 17) {
    return std::nullopt;
  }

  // 1. ロード
  auto const chunk = simde_mm256_loadu_si256(reinterpret_cast<simde__m256i const*>(mac.data()));

  // 2. デリミタの位置検証
  if constexpr (Options::validate_delimiters) {
    // デリミタの位置は 2,5,8,11,14 で、いずれも下位 128-bit lane 内に収まる
    auto const delim_idx = simde_mm256_setr_epi8(
      // clang-format off
         2,    5,    8,   11,   14, -128, -128, -128,
      -128, -128, -128, -128, -128, -128, -128, -128,
      -128, -128, -128, -128, -128, -128,  -128,  -128,
       -128,  -128,  -128,  -128,  -128,  -128,  -128,  -128
      // clang-format on
    );
    auto const delim_bytes = simde_mm256_shuffle_epi8(chunk, delim_idx);
    auto const eq = simde_mm256_cmpeq_epi8(delim_bytes, simde_mm256_set1_epi8(Options::delimiter));
    auto const mask = static_cast<unsigned>(simde_mm256_movemask_epi8(eq));
    if ((mask & 0x1Fu) != 0x1Fu) {
      return std::nullopt;
    }
  }

  // 3. シャッフル (デリミタの除去)
  // shuffle_epi8(pshufb) は 128-bit lane ごとのシャッフル。
  // 元データの index 16 (17文字目) は上位lane(オフセット16)にあるため、上位laneを下位に
  // 持ってきて必要な1バイトだけ抽出し、下位laneで作った11バイトと合成する
  auto const shuffle_idx_lo = simde_mm256_setr_epi8(
    // clang-format off
     0,  1,  3,    4,  6,  7,  9, 10,
    12, 13, 15, -128, -1, -1, -1, -1,
    -1, -1, -1,   -1, -1, -1,  -1,  -1,
     -1,  -1,  -1,    -1,  -1,  -1,  -1,  -1
    // clang-format on
  );
  auto const hex_chars_lo = simde_mm256_shuffle_epi8(chunk, shuffle_idx_lo);
  auto const chunk_hi = simde_mm256_permute2x128_si256(chunk, chunk, 0x11);
  auto const shuffle_idx_hi = simde_mm256_setr_epi8(
    // clang-format off
    -128, -128, -128, -128, -128, -128, -128, -128,
    -128, -128, -128,    0,   -1,   -1,   -1,   -1,
      -1,   -1,   -1,   -1,   -1,   -1,    -1,    -1,
       -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1
    // clang-format on
  );
  auto const hex_chars_hi = simde_mm256_shuffle_epi8(chunk_hi, shuffle_idx_hi);
  auto const hex_chars = simde_mm256_or_si256(hex_chars_lo, hex_chars_hi);

  // 4. ASCIIから数値への変換
  auto const v_mask_case = simde_mm256_set1_epi8(0x20);
  auto const upper_chars = simde_mm256_andnot_si256(v_mask_case, hex_chars);

  auto const is_digit = simde_mm256_and_si256(
      simde_mm256_cmpgt_epi8(hex_chars, simde_mm256_set1_epi8('0' - 1)),
      simde_mm256_cmpgt_epi8(simde_mm256_set1_epi8('9' + 1), hex_chars)
  );

  // 5. 16進数の文字になっているのか検証
  if constexpr (Options::validate_hex) {
    auto const is_alpha = simde_mm256_and_si256(
        simde_mm256_cmpgt_epi8(upper_chars, simde_mm256_set1_epi8('A' - 1)),
        simde_mm256_cmpgt_epi8(simde_mm256_set1_epi8('F' + 1), upper_chars)
    );
    auto const is_valid = simde_mm256_or_si256(is_digit, is_alpha);

    // 先頭12バイト (AA BB CC DD EE FF) だけが検証対象
    auto const mask = static_cast<unsigned>(simde_mm256_movemask_epi8(is_valid));
    if ((mask & 0x0FFFu) != 0x0FFFu) {
      return std::nullopt;
    }
  }

  auto const digit_val = simde_mm256_sub_epi8(hex_chars, simde_mm256_set1_epi8('0'));
  auto const alpha_val = simde_mm256_sub_epi8(upper_chars, simde_mm256_set1_epi8('A' - 10));
  auto const values = simde_mm256_blendv_epi8(alpha_val, digit_val, is_digit);

  // 6. 2文字を1バイトに結合 (High * 16 + Low)
  // vpmaddubs: (unsigned a0 * signed b0) + (unsigned a1 * signed b1)
  // [hi, lo] -> hi*16 + lo となるように (0x10, 0x01) を使う
  auto const multiplier = simde_mm256_set1_epi16(0x0110);
  auto const packed_16 = simde_mm256_maddubs_epi16(values, multiplier);

  // 7. 48bit整数をレジスタ内でパッキング
  // packed_16の各16bit要素の下位バイトを抽出して前方に詰める
  auto const final_shuffle = simde_mm256_setr_epi8(
    // clang-format off
      0,  2,  4,  6,  8, 10, -1, -1,
     -1, -1, -1, -1, -1, -1, -1, -1,
     -1, -1, -1, -1, -1, -1,  -1,  -1,
      -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1
    // clang-format on
  );

  auto const mac_vector = simde_mm256_shuffle_epi8(packed_16, final_shuffle);

  // 8. 64bit整数として抽出
  // simde_mm256_extract_epi64 は環境により挙動が重い場合があるが、意味的には直結
  auto const raw = static_cast<uint64_t>(simde_mm256_extract_epi64(mac_vector, 0));

  // 9. エンディアン変換
  return std::byteswap(raw) >> 16;
}

/**
 * @brief 安全版MACアドレスパーサ
 *
 * 入力文字列が32byte未満の場合にバッファオーバーランを防止するためのラッパー
 *
 * @tparam Options パースの仕方を指定するオプション
 * @param mac_str パース対象のMACアドレス文字列 (例: "AA:BB:CC:DD:EE:FF")
 * @return std::optional<uint64_t>
 */
template <typename Options = parse_mac_options>
[[nodiscard]]
auto parse_mac_address(std::string_view const mac) noexcept -> std::optional<uint64_t> {
  if (mac.size() < 17) {
    return std::nullopt;
  }

  // 256bitのロードは入力長(17)を越えるため、ゼロ埋めバッファにコピーしてからロードする
  auto buf = std::array<char, 32>{};
  auto const copy_len = (mac.size() < buf.size()) ? mac.size() : buf.size();
  std::memcpy(buf.data(), mac.data(), copy_len);

  return parse_mac_address_unsafe<Options>(std::string_view{buf.data(), copy_len});
}

} // namespace macad_parser

#endif /* MACAD_PARSER_HPP */
