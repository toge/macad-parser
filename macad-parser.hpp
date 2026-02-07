#ifndef MACAD_PARSER_HPP
#define MACAD_PARSER_HPP

#include <cstdint>
#include <string_view>
#include <optional>
#include <bit>
#include <array>
#include <cstring>
#include <string>
#include <type_traits>
#include <concepts>

#include "simde/x86/avx2.h"

namespace macad_parser {

/**
 * @brief MACアドレスパースオプションのデフォルト設定
 */
struct parse_mac_options {
  static constexpr bool validate_delimiters = false;
  static constexpr bool validate_hex = false;
  static constexpr char delimiter = ':';
  static constexpr bool uppercase = true;
};

/**
 * @brief デリミタと16進数文字の厳密な検証を行うオプション設定
 */
struct parse_mac_options_strict {
  static constexpr bool validate_delimiters = true;
  static constexpr bool validate_hex = true;
  static constexpr char delimiter = ':';
  static constexpr bool uppercase = true;
};

// Helper traits to provide default values for Options members
namespace detail {
  // Helper to check if a member exists and get its value or default
  template <typename T>
  struct has_validate_delimiters : std::bool_constant<false> {};
  
  template <typename T>
    requires requires { { T::validate_delimiters } -> std::convertible_to<bool>; }
  struct has_validate_delimiters<T> : std::bool_constant<T::validate_delimiters> {};
  
  template <typename T>
  struct has_validate_hex : std::bool_constant<false> {};
  
  template <typename T>
    requires requires { { T::validate_hex } -> std::convertible_to<bool>; }
  struct has_validate_hex<T> : std::bool_constant<T::validate_hex> {};
  
  template <typename T>
  struct has_delimiter {
    static constexpr char value = ':';
  };
  
  template <typename T>
    requires requires { { T::delimiter } -> std::convertible_to<char>; }
  struct has_delimiter<T> {
    static constexpr char value = T::delimiter;
  };
  
  template <typename T>
  struct has_uppercase : std::bool_constant<true> {};
  
  template <typename T>
    requires requires { { T::uppercase } -> std::convertible_to<bool>; }
  struct has_uppercase<T> : std::bool_constant<T::uppercase> {};
  
  // Helper variables for easier access
  template <typename T>
  inline constexpr bool validate_delimiters_v = has_validate_delimiters<T>::value;
  
  template <typename T>
  inline constexpr bool validate_hex_v = has_validate_hex<T>::value;
  
  template <typename T>
  inline constexpr char delimiter_v = has_delimiter<T>::value;
  
  template <typename T>
  inline constexpr bool uppercase_v = has_uppercase<T>::value;
}

/**
 * @brief MACアドレスを示す文字列をパースして48bit整数に変換する
 *
 * SIMDEを利用してAVX2命令を抽象化し、ARM環境でも動作するようにしたMACパース
 * 最後の48bit合成まで完全にベクトル演算（SIMDE経由）で行います
 *
 * @tparam Options パースの仕方を指定するオプション
 * @param mac_str パース対象のMACアドレス文字列 (例: "AA:BB:CC:DD:EE:FF")
 * @return std::optional<std::uint64_t>
 */
template <typename Options = parse_mac_options>
[[nodiscard]]
auto parse_mac_address_unsafe(std::string_view const mac) noexcept -> std::optional<std::uint64_t> {
  if (mac.size() < 17) {
    return std::nullopt;
  }

  // 1. ロード
  auto const chunk = simde_mm256_loadu_si256(reinterpret_cast<simde__m256i const*>(mac.data()));

  // 2. デリミタの位置検証
  if constexpr (detail::validate_delimiters_v<Options>) {
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
    auto const eq = simde_mm256_cmpeq_epi8(delim_bytes, simde_mm256_set1_epi8(detail::delimiter_v<Options>));
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
  if constexpr (detail::validate_hex_v<Options>) {
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
  auto const raw = static_cast<std::uint64_t>(simde_mm256_extract_epi64(mac_vector, 0));

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
 * @return std::optional<std::uint64_t>
 */
template <typename Options = parse_mac_options>
[[nodiscard]]
auto parse_mac_address(std::string_view const mac) noexcept -> std::optional<std::uint64_t> {
  if (mac.size() < 17) {
    return std::nullopt;
  }

  // 256bitのロードは入力長(17)を越えるため、ゼロ埋めバッファにコピーしてからロードする
  auto buf = std::array<char, 32>{};
  auto const copy_len = (mac.size() < buf.size()) ? mac.size() : buf.size();
  std::memcpy(buf.data(), mac.data(), copy_len);

  return parse_mac_address_unsafe<Options>(std::string_view{buf.data(), copy_len});
}

/**
 * @brief 48bit整数をMACアドレス文字列に変換し、指定されたバッファに書き込む
 *
 * SIMDEを利用してAVX2命令を抽象化し、ARM環境でも動作するように実装
 * 整数値から16進数文字列への変換をベクトル演算（SIMDE経由）で行います
 * メモリアロケーションを行わない版です。
 *
 * @tparam Options デリミタと大文字・小文字を指定するオプション（validate_delimitersとvalidate_hexは無視される）
 * @param mac 48bit整数値（0x0000000000000000〜0x0000FFFFFFFFFFFF）
 * @param buffer 出力先のバッファ（最低17バイトが必要）
 * @return 書き込まれた文字数（常に17）
 */
template <typename Options = parse_mac_options>
auto format_mac_address_to_buffer(std::uint64_t const mac, char* const buffer) -> std::size_t {
  // 1. 48bitに制限（上位16bitをマスク）
  auto const mac_48 = mac & 0xFFFFFFFFFFFFull;

  // 2. ビッグエンディアン形式で6バイトに展開
  // エンディアン変換後に左シフトして上位48bitを使用
  auto const swapped = std::byteswap(mac_48 << 16);

  // 3. 6バイトをSIMDレジスタにロード
  // 最初の8バイトを使用（6バイトのMACアドレス + 2バイトのパディング）
  auto buf = std::array<std::uint8_t, 32>{};
  std::memcpy(buf.data(), &swapped, 8);

  auto const mac_bytes = simde_mm256_loadu_si256(reinterpret_cast<simde__m256i const*>(buf.data()));

  // 4. ニブル変換用のルックアップテーブルを作成
  // 16進数字への変換テーブル: 0-9 -> '0'-'9', 10-15 -> 'A'-'F' or 'a'-'f'
  auto const hex_lut = detail::uppercase_v<Options> 
    ? simde_mm256_setr_epi8(
        // clang-format off
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'A', 'B', 'C', 'D', 'E', 'F',
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
        // clang-format on
      )
    : simde_mm256_setr_epi8(
        // clang-format off
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'a', 'b', 'c', 'd', 'e', 'f',
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
        // clang-format on
      );

  // 5. 各バイトを上位/下位ニブルに分離
  // 上位ニブルは右に4シフトするが、16bit境界を越えないように先にマスク
  auto const hi_nibbles = simde_mm256_srli_epi16(
      simde_mm256_and_si256(mac_bytes, simde_mm256_set1_epi8(static_cast<char>(0xF0))),
      4
  );
  auto const lo_nibbles = simde_mm256_and_si256(mac_bytes, simde_mm256_set1_epi8(0x0F));

  // 6. ルックアップテーブルを使って16進文字に変換
  auto const hi_chars = simde_mm256_shuffle_epi8(hex_lut, hi_nibbles);
  auto const lo_chars = simde_mm256_shuffle_epi8(hex_lut, lo_nibbles);

  // 7. 上位と下位を交互に配置（128bit演算に切り替え）
  // shuffle_epi8は128bitレーン内でのみ動作するため、下位128bitを使用
  auto const hi_chars_128 = simde_mm256_castsi256_si128(hi_chars);
  auto const lo_chars_128 = simde_mm256_castsi256_si128(lo_chars);
  
  // シャッフルで配置: hi を偶数位置に、lo を奇数位置に
  auto const shuffle_hi = simde_mm_setr_epi8(
    // clang-format off
     0, -1,  1, -1,  2, -1,  3, -1,
     4, -1,  5, -1, -1, -1, -1, -1
    // clang-format on
  );
  auto const shuffle_lo = simde_mm_setr_epi8(
    // clang-format off
    -1,  0, -1,  1, -1,  2, -1,  3,
    -1,  4, -1,  5, -1, -1, -1, -1
    // clang-format on
  );
  
  auto const hi_positioned = simde_mm_shuffle_epi8(hi_chars_128, shuffle_hi);
  auto const lo_positioned = simde_mm_shuffle_epi8(lo_chars_128, shuffle_lo);
  
  auto const hex_chars = simde_mm_or_si128(hi_positioned, lo_positioned);

  // 8. デリミタを挿入して最終形式に整形（128bit版）
  // 目標: XX:XX:XX:XX:XX:XX (17文字)
  // hex_charsは12バイト(0-11)を持つ: 1,1,2,2,3,3,4,4,5,5,6,6
  // 出力位置: 0,1,:,2,3,:,4,5,:,6,7,:,8,9,:,10,11
  // 128bitレジスタでは16バイトまでしか扱えないため、17バイト目は個別に処理
  
  auto const delim = simde_mm_set1_epi8(detail::delimiter_v<Options>);

  auto const shuffle_with_delim = simde_mm_setr_epi8(
    // clang-format off
     0,  1, -1,  2,  3, -1,  4,  5,
    -1,  6,  7, -1,  8,  9, -1, 10
    // clang-format on
  );

  auto const formatted = simde_mm_shuffle_epi8(hex_chars, shuffle_with_delim);

  // 9. デリミタの位置にデリミタ文字をブレンド
  // デリミタ位置は 2, 5, 8, 11, 14
  auto const delim_mask = simde_mm_setr_epi8(
    // clang-format off
     0,  0, -1,  0,  0, -1,  0,  0,
    -1,  0,  0, -1,  0,  0, -1,  0
    // clang-format on
  );

  auto const result_vec = simde_mm_blendv_epi8(formatted, delim, delim_mask);

  // 10. ベクトルから文字列を抽出（16バイト）
  simde_mm_storeu_si128(reinterpret_cast<simde__m128i*>(buffer), result_vec);
  
  // 11. 最後の文字（17バイト目）を個別に追加
  // hex_charsの位置11（最後のlo nibble）を抽出
  alignas(16) char temp_hex_storage[16];
  simde_mm_storeu_si128(reinterpret_cast<simde__m128i*>(temp_hex_storage), hex_chars);
  buffer[16] = temp_hex_storage[11];

  return 17;
}

/**
 * @brief 48bit整数をMACアドレス文字列に変換する
 *
 * SIMDEを利用してAVX2命令を抽象化し、ARM環境でも動作するように実装
 * 整数値から16進数文字列への変換をベクトル演算（SIMDE経由）で行います
 *
 * @tparam Options デリミタと大文字・小文字を指定するオプション（validate_delimitersとvalidate_hexは無視される）
 * @param mac 48bit整数値（0x0000000000000000〜0x0000FFFFFFFFFFFF）
 * @return std::string MACアドレス文字列 (例: "AA:BB:CC:DD:EE:FF" または "aa:bb:cc:dd:ee:ff")
 */
template <typename Options = parse_mac_options>
[[nodiscard]]
auto format_mac_address(std::uint64_t const mac) -> std::string {
  auto result_buf = std::array<char, 17>{};
  format_mac_address_to_buffer<Options>(mac, result_buf.data());
  return std::string{result_buf.data(), 17};
}

} // namespace macad_parser

#endif /* MACAD_PARSER_HPP */
