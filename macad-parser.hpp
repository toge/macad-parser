#ifndef MACAD_PARSER_HPP
#define MACAD_PARSER_HPP

#include <array>
#include <bit>
#include <concepts>
#include <cstdint>
#include <cstring>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>

#include "simde/x86/avx2.h"

namespace macad_parser {

/**
 * @brief パースエラーの種類
 */
enum class parse_error {
  invalid_length,
  invalid_delimiter,
  invalid_character,
};

/**
 * @brief MACアドレス文字列の長さ（デリミタを含む）
 */
auto constexpr MAC_ADDRESS_STRING_LENGTH = 17uz;

/**
 * @brief EUI-64アドレス文字列の長さ（デリミタを含む）
 */
auto constexpr EUI64_STRING_LENGTH = 23uz;

/**
 * @brief MACアドレスパースオプションのデフォルト設定
 */
struct parse_mac_options {
  static constexpr auto validate_delimiters = false;
  static constexpr auto validate_hex        = false;
  static constexpr auto delimiter           = ':';
  static constexpr auto uppercase           = true;
};

/**
 * @brief デリミタと16進数文字の厳密な検証を行うオプション設定
 */
struct parse_mac_options_strict {
  static constexpr auto validate_delimiters = true;
  static constexpr auto validate_hex        = true;
  static constexpr auto delimiter           = ':';
  static constexpr auto uppercase           = true;
};

// Helper to provide default values for Options members
namespace detail {
  template <typename T>
  concept HasValidateDelimiters = requires {
    { T::validate_delimiters } -> std::convertible_to<bool>;
  };

  template <typename T>
  auto constexpr validate_delimiters_v = [] {
    if constexpr (HasValidateDelimiters<T>) {
      return static_cast<bool>(T::validate_delimiters);
    }
    return false;
  }();

  template <typename T>
  concept HasValidateHex = requires {
    { T::validate_hex } -> std::convertible_to<bool>;
  };

  template <typename T>
  auto constexpr validate_hex_v = [] {
    if constexpr (HasValidateHex<T>) {
      return static_cast<bool>(T::validate_hex);
    }
    return false;
  }();

  template <typename T>
  concept HasDelimiter = requires {
    { T::delimiter } -> std::convertible_to<char>;
  };

  template <typename T>
  auto constexpr delimiter_v = [] {
    if constexpr (HasDelimiter<T>) {
      return static_cast<char>(T::delimiter);
    }
    return ':';
  }();

  template <typename T>
  concept HasUppercase = requires {
    { T::uppercase } -> std::convertible_to<bool>;
  };

  template <typename T>
  auto constexpr uppercase_v = [] {
    if constexpr (HasUppercase<T>) {
      return static_cast<bool>(T::uppercase);
    }
    return true;
  }();

  template <std::size_t Length, typename Options>
  [[nodiscard]]
  auto constexpr parse_address_constexpr(std::string_view const addr) noexcept -> std::optional<std::uint64_t> {
    if (addr.size() < Length) {
      return std::nullopt;
    }

    auto result = std::uint64_t{0};

    for (auto i = 0uz; i < Length; ++i) {
      auto const c = addr[i];
      if (i % 3 == 2) {
        if constexpr (validate_delimiters_v<Options>) {
          if (c != delimiter_v<Options>) {
            return std::nullopt;
          }
        }
        continue;
      }

      auto val = 0u;
      if (c >= '0' && c <= '9') {
        val = static_cast<std::uint32_t>(c - '0');
      } else if (c >= 'A' && c <= 'F') {
        val = static_cast<std::uint32_t>(c - 'A' + 10);
      } else if (c >= 'a' && c <= 'f') {
        val = static_cast<std::uint32_t>(c - 'a' + 10);
      } else {
        if constexpr (validate_hex_v<Options>) {
          return std::nullopt;
        }
        val = 0;
      }
      result = (result << 4) | val;
    }

    return result;
  }
}  // namespace detail

/**
 * @brief MACアドレスを示す文字列をパースして48bit整数に変換する
 *
 * SIMDEを利用してAVX2命令を抽象化し、ARM環境でも動作するようにしたMACパース
 * 最後の48bit合成まで完全にベクトル演算（SIMDE経由）で行います
 *
 * @tparam Options パースの仕方を指定するオプション
 * @param mac パース対象のMACアドレス文字列 (例: "AA:BB:CC:DD:EE:FF")
 * @return std::optional<std::uint64_t>
 */
template <typename Options = parse_mac_options>
[[nodiscard]]
auto constexpr parse_mac_address_unsafe(std::string_view const mac) noexcept -> std::optional<std::uint64_t> {
  if consteval {
    return detail::parse_address_constexpr<MAC_ADDRESS_STRING_LENGTH, Options>(mac);
  }

  if (mac.size() < MAC_ADDRESS_STRING_LENGTH) {
    return std::nullopt;
  }

  // 1. ロード
  auto const chunk = simde_mm256_loadu_si256(reinterpret_cast<simde__m256i const*>(mac.data()));

  // 2. デリミタの位置検証
  if constexpr (detail::validate_delimiters_v<Options>) {
    auto const delim_idx = simde_mm256_setr_epi8(
      // clang-format off
         2,    5,    8,   11,   14, -128, -128, -128,
      -128, -128, -128, -128, -128, -128, -128, -128,
      -128, -128, -128, -128, -128, -128, -128, -128,
      -128, -128, -128, -128, -128, -128, -128, -128
      // clang-format on
    );
    auto const delim_bytes = simde_mm256_shuffle_epi8(chunk, delim_idx);
    auto const eq          = simde_mm256_cmpeq_epi8(delim_bytes, simde_mm256_set1_epi8(detail::delimiter_v<Options>));
    auto const mask        = static_cast<std::uint32_t>(simde_mm256_movemask_epi8(eq));
    if ((mask & 0x1Fu) != 0x1Fu) {
      return std::nullopt;
    }
  }

  // 3. シャッフル (デリミタの除去)
  auto const shuffle_idx_lo = simde_mm256_setr_epi8(
    // clang-format off
     0,  1,  3,    4,  6,  7,  9, 10,
    12, 13, 15, -128, -1, -1, -1, -1,
    -1, -1, -1,   -1, -1, -1, -1, -1,
    -1, -1, -1,   -1, -1, -1, -1, -1
    // clang-format on
  );
  auto const hex_chars_lo   = simde_mm256_shuffle_epi8(chunk, shuffle_idx_lo);
  auto const chunk_hi       = simde_mm256_permute2x128_si256(chunk, chunk, 0x11);
  auto const shuffle_idx_hi = simde_mm256_setr_epi8(
    // clang-format off
    -128, -128, -128, -128, -128, -128, -128, -128,
    -128, -128, -128,    0,   -1,   -1,   -1,   -1,
      -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
      -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1
    // clang-format on
  );
  auto const hex_chars_hi = simde_mm256_shuffle_epi8(chunk_hi, shuffle_idx_hi);
  auto const hex_chars    = simde_mm256_or_si256(hex_chars_lo, hex_chars_hi);

  // 4. ASCIIから数値への変換
  auto constexpr mask_case = std::uint8_t{0x20};
  auto const v_mask_case = simde_mm256_set1_epi8(static_cast<char>(mask_case));
  auto const upper_chars = simde_mm256_andnot_si256(v_mask_case, hex_chars);

  auto const is_digit = simde_mm256_and_si256(simde_mm256_cmpgt_epi8(hex_chars, simde_mm256_set1_epi8('0' - 1)), simde_mm256_cmpgt_epi8(simde_mm256_set1_epi8('9' + 1), hex_chars));

  // 5. 16進数の文字になっているのか検証
  if constexpr (detail::validate_hex_v<Options>) {
    auto const is_alpha = simde_mm256_and_si256(simde_mm256_cmpgt_epi8(upper_chars, simde_mm256_set1_epi8('A' - 1)), simde_mm256_cmpgt_epi8(simde_mm256_set1_epi8('F' + 1), upper_chars));
    auto const is_valid = simde_mm256_or_si256(is_digit, is_alpha);

    auto const mask = static_cast<std::uint32_t>(simde_mm256_movemask_epi8(is_valid));
    if ((mask & 0x0FFFu) != 0x0FFFu) {
      return std::nullopt;
    }
  }

  auto const digit_val = simde_mm256_sub_epi8(hex_chars, simde_mm256_set1_epi8('0'));
  auto const alpha_val = simde_mm256_sub_epi8(upper_chars, simde_mm256_set1_epi8('A' - 10));
  auto const values    = simde_mm256_blendv_epi8(alpha_val, digit_val, is_digit);

  // 6. 2文字を1バイトに結合 (High * 16 + Low)
  auto const multiplier = simde_mm256_set1_epi16(0x0110);
  auto const packed_16  = simde_mm256_maddubs_epi16(values, multiplier);

  // 7. 48bit整数をレジスタ内でパッキング
  auto const final_shuffle = simde_mm256_setr_epi8(
    // clang-format off
      0,  2,  4,  6,  8, 10, -1, -1,
     -1, -1, -1, -1, -1, -1, -1, -1,
     -1, -1, -1, -1, -1, -1, -1, -1,
     -1, -1, -1, -1, -1, -1, -1, -1
    // clang-format on
  );

  auto const mac_vector = simde_mm256_shuffle_epi8(packed_16, final_shuffle);

  // 8. 64bit整数として抽出
  auto const raw = static_cast<std::uint64_t>(simde_mm256_extract_epi64(mac_vector, 0));

  // 9. エンディアン変換
  return std::byteswap(raw) >> 16;
}

/**
 * @brief 安全版MACアドレスパーサ (constexpr対応)
 *
 * 入力文字列が32byte未満の場合にバッファオーバーランを防止するためのラッパー。
 * 定数評価コンテキストでは純粋なC++実装を使用し、実行時はSIMD実装を使用する。
 *
 * @tparam Options パースの仕方を指定するオプション
 * @param mac_str パース対象のMACアドレス文字列 (例: "AA:BB:CC:DD:EE:FF")
 * @return std::optional<std::uint64_t>
 */
template <typename Options = parse_mac_options>
[[nodiscard]]
auto constexpr parse_mac_address(std::string_view const mac) noexcept -> std::optional<std::uint64_t> {
  if (mac.size() < MAC_ADDRESS_STRING_LENGTH) {
    return std::nullopt;
  }

  if consteval {
    return detail::parse_address_constexpr<MAC_ADDRESS_STRING_LENGTH, Options>(mac);
  } else {
    auto       buf      = std::array<char, 32>{};
    auto const copy_len = (mac.size() < buf.size()) ? mac.size() : buf.size();
    std::memcpy(buf.data(), mac.data(), copy_len);
    return parse_mac_address_unsafe<Options>(std::string_view{buf.data(), copy_len});
  }
}

/**
 * @brief MACアドレスを示す文字列をパースしてバイト配列に変換する
 */
template <typename Options = parse_mac_options>
[[nodiscard]]
auto constexpr parse_mac_address_to_bytes(std::string_view const mac) noexcept -> std::optional<std::array<std::uint8_t, 6>> {
  auto const val = parse_mac_address<Options>(mac);
  if (!val) {
    return std::nullopt;
  }
  auto const v = *val;
  return std::array<std::uint8_t, 6>{
    static_cast<std::uint8_t>((v >> 40) & 0xFF),
    static_cast<std::uint8_t>((v >> 32) & 0xFF),
    static_cast<std::uint8_t>((v >> 24) & 0xFF),
    static_cast<std::uint8_t>((v >> 16) & 0xFF),
    static_cast<std::uint8_t>((v >> 8) & 0xFF),
    static_cast<std::uint8_t>(v & 0xFF)
  };
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
 * @param buffer 出力先のバッファ（17バイトが必要）
 * @return 書き込まれた文字数（常に17）
 */
template <typename Options = parse_mac_options>
auto constexpr format_mac_address_to_buffer(std::uint64_t const mac, std::span<char, MAC_ADDRESS_STRING_LENGTH> buffer) -> std::size_t {
  if consteval {
    auto constexpr hex_chars_upper = std::string_view{"0123456789ABCDEF"};
    auto constexpr hex_chars_lower = std::string_view{"0123456789abcdef"};
    auto const hex_lut = detail::uppercase_v<Options> ? hex_chars_upper : hex_chars_lower;
    auto const delim = detail::delimiter_v<Options>;
    auto const mac_48 = mac & 0xFFFFFFFFFFFFull;

    for (auto i = 0uz; i < 6; ++i) {
      auto const byte = static_cast<std::uint8_t>((mac_48 >> (8 * (5 - i))) & 0xFF);
      buffer[i * 3] = hex_lut[byte >> 4];
      buffer[i * 3 + 1] = hex_lut[byte & 0x0F];
      if (i < 5) {
        buffer[i * 3 + 2] = delim;
      }
    }
    return MAC_ADDRESS_STRING_LENGTH;
  }

  // 1. 48bitに制限（上位16bitをマスク）
  auto const mac_48 = mac & 0xFFFFFFFFFFFFull;
  // ... (rest of SIMD implementation)

  // 2. ビッグエンディアン形式で6バイトに展開
  auto const swapped = std::byteswap(mac_48 << 16);

  // 3. 6バイトをSIMDレジスタにロード
  auto buf = std::array<std::uint8_t, 32>{};
  std::memcpy(buf.data(), &swapped, 8);

  auto const mac_bytes = simde_mm256_loadu_si256(reinterpret_cast<simde__m256i const*>(buf.data()));

  // 4. ニブル変換用のルックアップテーブルを作成
  auto const hex_lut = detail::uppercase_v<Options> ? 
    simde_mm256_setr_epi8(
      // clang-format off
      '0', '1', '2', '3', '4', '5', '6', '7',
      '8', '9', 'A', 'B', 'C', 'D', 'E', 'F',
      '0', '1', '2', '3', '4', '5', '6', '7',
      '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
      // clang-format on
    )
    : 
    simde_mm256_setr_epi8(
      // clang-format off
      '0', '1', '2', '3', '4', '5', '6', '7',
      '8', '9', 'a', 'b', 'c', 'd', 'e', 'f',
      '0', '1', '2', '3', '4', '5', '6', '7',
      '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
      // clang-format on
    );

  // 5. 各バイトを上位/下位ニブルに分離
  auto const hi_nibbles = simde_mm256_srli_epi16(simde_mm256_and_si256(mac_bytes, simde_mm256_set1_epi8(static_cast<char>(0xF0))), 4);
  auto const lo_nibbles = simde_mm256_and_si256(mac_bytes, simde_mm256_set1_epi8(0x0F));

  // 6. ルックアップテーブルを使って16進文字に変換
  auto const hi_chars = simde_mm256_shuffle_epi8(hex_lut, hi_nibbles);
  auto const lo_chars = simde_mm256_shuffle_epi8(hex_lut, lo_nibbles);

  // 7. 上位と下位を交互に配置（128bit演算に切り替え）
  auto const hi_chars_128 = simde_mm256_castsi256_si128(hi_chars);
  auto const lo_chars_128 = simde_mm256_castsi256_si128(lo_chars);

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
  auto const delim = simde_mm_set1_epi8(detail::delimiter_v<Options>);

  auto const shuffle_with_delim = simde_mm_setr_epi8(
    // clang-format off
     0,  1, -1,  2,  3, -1,  4,  5,
    -1,  6,  7, -1,  8,  9, -1, 10
    // clang-format on
  );

  auto const formatted = simde_mm_shuffle_epi8(hex_chars, shuffle_with_delim);

  // 9. デリミタの位置にデリミタ文字をブレンド
  auto const delim_mask = simde_mm_setr_epi8(
    // clang-format off
     0,  0, -1,  0,  0, -1,  0,  0,
    -1,  0,  0, -1,  0,  0, -1,  0
    // clang-format on
  );

  auto const result_vec = simde_mm_blendv_epi8(formatted, delim, delim_mask);

  // 10. ベクトルから文字列を抽出（16バイト）
  simde_mm_storeu_si128(reinterpret_cast<simde__m128i*>(buffer.data()), result_vec);

  // 11. 最後の文字（17バイト目）を直接抽出して書き込み
  buffer[16] = static_cast<char>(simde_mm_extract_epi8(hex_chars, 11));

  return MAC_ADDRESS_STRING_LENGTH;
}

/**
 * @brief 48bit整数をMACアドレス文字列に変換する (constexpr対応)
 *
 * SIMDEを利用してAVX2命令を抽象化し、ARM環境でも動作するように実装
 * 整数値から16進数文字列への変換をベクトル演算（SIMDE経由）で行います。
 * 定数評価コンテキストでは純粋なC++実装を使用し、実行時はSIMD実装を使用する。
 *
 * @tparam Options デリミタと大文字・小文字を指定するオプション（validate_delimitersとvalidate_hexは無視される）
 * @param mac 48bit整数値（0x0000000000000000〜0x0000FFFFFFFFFFFF）
 * @return std::string MACアドレス文字列 (例: "AA:BB:CC:DD:EE:FF" または "aa:bb:cc:dd:ee:ff")
 */
template <typename Options = parse_mac_options>
[[nodiscard]]
auto constexpr format_mac_address(std::uint64_t const mac) -> std::string {
  auto result_buf = std::array<char, MAC_ADDRESS_STRING_LENGTH>{};
  format_mac_address_to_buffer<Options>(mac, result_buf);
  return std::string{result_buf.data(), MAC_ADDRESS_STRING_LENGTH};
}

/**
 * @brief EUI-64アドレスを示す文字列をパースして64bit整数に変換する
 */
template <typename Options = parse_mac_options>
[[nodiscard]]
auto constexpr parse_eui64_address(std::string_view const eui) noexcept -> std::optional<std::uint64_t> {
  return detail::parse_address_constexpr<EUI64_STRING_LENGTH, Options>(eui);
}

/**
 * @brief 64bit整数をEUI-64アドレス文字列に変換し、指定されたバッファに書き込む
 */
template <typename Options = parse_mac_options>
auto constexpr format_eui64_to_buffer(std::uint64_t const eui, std::span<char, EUI64_STRING_LENGTH> buffer) -> std::size_t {
  auto constexpr hex_chars_upper = std::string_view{"0123456789ABCDEF"};
  auto constexpr hex_chars_lower = std::string_view{"0123456789abcdef"};
  auto const hex_lut = detail::uppercase_v<Options> ? hex_chars_upper : hex_chars_lower;
  auto const delim = detail::delimiter_v<Options>;

  for (auto i = 0uz; i < 8; ++i) {
    auto const byte = static_cast<std::uint8_t>((eui >> (8 * (7 - i))) & 0xFF);
    buffer[i * 3] = hex_lut[byte >> 4];
    buffer[i * 3 + 1] = hex_lut[byte & 0x0F];
    if (i < 7) {
      buffer[i * 3 + 2] = delim;
    }
  }
  return EUI64_STRING_LENGTH;
}

/**
 * @brief 64bit整数をEUI-64アドレス文字列に変換する
 */
template <typename Options = parse_mac_options>
[[nodiscard]]
auto constexpr format_eui64_address(std::uint64_t const eui) -> std::string {
  auto result_buf = std::array<char, EUI64_STRING_LENGTH>{};
  format_eui64_to_buffer<Options>(eui, result_buf);
  return std::string{result_buf.data(), EUI64_STRING_LENGTH};
}

/**
 * @brief デリミタなしのMACアドレス文字列（12文字）をパースする
 */
template <typename Options = parse_mac_options>
[[nodiscard]]
auto constexpr parse_mac_address_no_delimiter(std::string_view const mac) noexcept -> std::optional<std::uint64_t> {
  if (mac.size() < 12) {
    return std::nullopt;
  }
  auto result = std::uint64_t{0};
  for (auto i = 0uz; i < 12; ++i) {
    auto const c = mac[i];
    auto val = 0u;
    if (c >= '0' && c <= '9') {
      val = static_cast<std::uint32_t>(c - '0');
    } else if (c >= 'A' && c <= 'F') {
      val = static_cast<std::uint32_t>(c - 'A' + 10);
    } else if (c >= 'a' && c <= 'f') {
      val = static_cast<std::uint32_t>(c - 'a' + 10);
    } else {
      return std::nullopt;
    }
    result = (result << 4) | val;
  }
  return result;
}

}  // namespace macad_parser

#endif /* MACAD_PARSER_HPP */
