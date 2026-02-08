#include <algorithm>
#include <cctype>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "catch2/catch_all.hpp"

#include "macad-parser.hpp"

// ============================================================================
// ベースラインとなるSIMDを使わないナイーブな実装
// ============================================================================

namespace naive {

/**
 * @brief ナイーブな実装: MACアドレス文字列を48bit整数にパース
 * 
 * SIMD命令を使わず、標準ライブラリのみで実装したベースライン
 * 
 * @param mac MACアドレス文字列 (例: "AA:BB:CC:DD:EE:FF")
 * @param validate_delimiters デリミタを検証するか
 * @param validate_hex 16進数文字を検証するか
 * @param delimiter デリミタ文字
 * @return std::optional<std::uint64_t> パース結果
 */
[[nodiscard]]
auto parse_mac_address(
  std::string_view const mac, 
  bool const validate_delimiters = false,
  bool const validate_hex = false,
  char const delimiter = ':'
) noexcept -> std::optional<std::uint64_t> {
  if (mac.size() < 17) {
    return std::nullopt;
  }

  // デリミタの位置検証
  if (validate_delimiters) {
    if (mac[2] != delimiter or mac[5] != delimiter or 
        mac[8] != delimiter or mac[11] != delimiter or mac[14] != delimiter) {
      return std::nullopt;
    }
  }

  std::uint64_t result = 0;
  
  // 各バイトをパース
  auto const parse_hex_pair = [&](std::size_t const idx) -> std::optional<std::uint8_t> {
    auto const hi_char = mac[idx + 0];
    auto const lo_char = mac[idx + 1];

    // 大文字に変換
    auto const hi_upper = static_cast<char>(std::toupper(static_cast<unsigned char>(hi_char)));
    auto const lo_upper = static_cast<char>(std::toupper(static_cast<unsigned char>(lo_char)));

    // 16進数文字の検証
    if (validate_hex) {
      auto const is_hex = [](char const c) {
        return (c >= '0' and c <= '9') or (c >= 'A' and c <= 'F');
      };
      if (!is_hex(hi_upper) or !is_hex(lo_upper)) {
        return std::nullopt;
      }
    }

    // 16進数文字を数値に変換
    auto const hex_to_value = [](char const c) -> std::uint8_t {
      if (c >= '0' and c <= '9') {
        return static_cast<std::uint8_t>(c - '0');
      }
      return static_cast<std::uint8_t>(c - 'A' + 10);
    };

    auto const hi_val = hex_to_value(hi_upper);
    auto const lo_val = hex_to_value(lo_upper);
    return static_cast<std::uint8_t>((hi_val << 4) | lo_val);
  };

  // 6バイト分パース (AA:BB:CC:DD:EE:FF)
  std::size_t const positions[] = {0, 3, 6, 9, 12, 15};
  for (std::size_t i = 0; i < 6; ++i) {
    auto const byte_val = parse_hex_pair(positions[i]);
    if (!byte_val) {
      return std::nullopt;
    }
    result = (result << 8) | byte_val.value();
  }
  return result;
}

/**
 * @brief ナイーブな実装: 48bit整数をMACアドレス文字列にフォーマット
 * 
 * SIMD命令を使わず、標準ライブラリのみで実装したベースライン
 * 
 * @param mac 48bit整数値
 * @param uppercase 大文字を使用するか
 * @param delimiter デリミタ文字
 * @return std::string MACアドレス文字列
 */
[[nodiscard]]
auto format_mac_address(
  std::uint64_t const mac,
  bool const uppercase = true,
  char const delimiter = ':'
) -> std::string {
  auto const mac_48 = mac & 0xFFFFFFFFFFFFull;
  auto const hex_chars = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
  
  // 6バイト分フォーマット
  auto result = std::string(17, '\0');
  for (std::size_t i = 0; i < 6; ++i) {
    auto const byte_pos = 5 - i;
    auto const byte_val = static_cast<std::uint8_t>((mac_48 >> (byte_pos * 8)) & 0xFF);
    
    auto const str_pos = i * 3;
    result[str_pos + 0] = hex_chars[byte_val >> 4];
    result[str_pos + 1] = hex_chars[byte_val & 0x0F];
    
    if (i < 5) {
      result[str_pos + 2] = delimiter;
    }
  }
  
  return result;
}

} // namespace naive

// ============================================================================
// Benchmark Tests
// ============================================================================

// テスト用のMACアドレス
static constexpr auto TEST_MAC_STR = "AA:BB:CC:DD:EE:FF               "; // 32byte確保
static constexpr auto TEST_MAC_VAL = 0xAABBCCDDEEFFull;

// ============================================================================
// ベンチマーク用のOption
// ============================================================================

struct opt_lowercase {
  static constexpr bool uppercase = false;
};

struct opt_dash {
  static constexpr char delimiter = '-';
};

struct opt_uppercase {
  static constexpr bool uppercase = true;
};

struct opt_colon {
  static constexpr char delimiter = ':';
};

struct opt_no_validation {
  static constexpr bool validate_delimiters = false;
  static constexpr bool validate_hex = false;
};

struct opt_delimiter_only {
  static constexpr bool validate_delimiters = true;
  static constexpr bool validate_hex = false;
};

struct opt_hex_only {
  static constexpr bool validate_delimiters = false;
  static constexpr bool validate_hex = true;
};

// ============================================================================
// Parse Benchmarks
// ============================================================================

TEST_CASE("Benchmark: parse_mac_address", "[benchmark]") {
  BENCHMARK("parse (default options - no validation)") {
    return macad_parser::parse_mac_address(TEST_MAC_STR);
  };

  BENCHMARK("parse (strict options - with validation)") {
    return macad_parser::parse_mac_address<macad_parser::parse_mac_options_strict>(TEST_MAC_STR);
  };

  BENCHMARK("parse unsafe (default options - no validation)") {
    return macad_parser::parse_mac_address_unsafe(std::string_view{TEST_MAC_STR, 17});
  };

  BENCHMARK("parse unsafe (strict options - with validation)") {
    return macad_parser::parse_mac_address_unsafe<macad_parser::parse_mac_options_strict>(std::string_view{TEST_MAC_STR, 17});
  };

  BENCHMARK("parse naive (no validation) - baseline") {
    return naive::parse_mac_address(TEST_MAC_STR, false, false, ':');
  };

  BENCHMARK("parse naive (with validation) - baseline") {
    return naive::parse_mac_address(TEST_MAC_STR, true, true, ':');
  };
}

// ============================================================================
// Format Benchmarks
// ============================================================================

TEST_CASE("Benchmark: format_mac_address", "[benchmark]") {
  BENCHMARK("format (uppercase, default delimiter)") {
    return macad_parser::format_mac_address(TEST_MAC_VAL);
  };

  BENCHMARK("format (lowercase, default delimiter)") {
    return macad_parser::format_mac_address<opt_lowercase>(TEST_MAC_VAL);
  };

  BENCHMARK("format (uppercase, dash delimiter)") {
    return macad_parser::format_mac_address<opt_dash>(TEST_MAC_VAL);
  };

  BENCHMARK("format naive (uppercase) - baseline") {
    return naive::format_mac_address(TEST_MAC_VAL, true, ':');
  };

  BENCHMARK("format naive (lowercase) - baseline") {
    return naive::format_mac_address(TEST_MAC_VAL, false, ':');
  };
}

// ============================================================================
// Round-trip Benchmarks
// ============================================================================

TEST_CASE("Benchmark: round-trip (parse + format)", "[benchmark]") {
  
  BENCHMARK("round-trip SIMD (default options)") {
    auto const parsed = macad_parser::parse_mac_address(TEST_MAC_STR);
    if (parsed) {
      return macad_parser::format_mac_address(parsed.value());
    }
    return std::string{};
  };

  BENCHMARK("round-trip SIMD (strict options)") {
    auto const parsed = macad_parser::parse_mac_address<macad_parser::parse_mac_options_strict>(TEST_MAC_STR);
    if (parsed) {
      return macad_parser::format_mac_address(parsed.value());
    }
    return std::string{};
  };

  BENCHMARK("round-trip SIMD unsafe (default options)") {
    auto const parsed = macad_parser::parse_mac_address_unsafe(std::string_view{TEST_MAC_STR, 17});
    if (parsed) {
      return macad_parser::format_mac_address(parsed.value());
    }
    return std::string{};
  };

  BENCHMARK("round-trip SIMD unsafe (strict options)") {
    auto const parsed = macad_parser::parse_mac_address_unsafe<macad_parser::parse_mac_options_strict>(
      std::string_view{TEST_MAC_STR, 17}
    );
    if (parsed) {
      return macad_parser::format_mac_address(parsed.value());
    }
    return std::string{};
  };

  BENCHMARK("round-trip naive - baseline") {
    auto const parsed = naive::parse_mac_address(TEST_MAC_STR, false, false, ':');
    if (parsed) {
      return naive::format_mac_address(parsed.value(), true, ':');
    }
    return std::string{};
  };
}

// ============================================================================
// Optionsによる処理速度比較 Benchmarks
// ============================================================================

TEST_CASE("Benchmark: validate_delimiters impact", "[benchmark]") {
  BENCHMARK("parse without delimiter validation") {
    return macad_parser::parse_mac_address_unsafe<opt_no_validation>(std::string_view{TEST_MAC_STR, 17});
  };

  BENCHMARK("parse with delimiter validation") {
    return macad_parser::parse_mac_address_unsafe<opt_delimiter_only>(std::string_view{TEST_MAC_STR, 17});
  };
}

TEST_CASE("Benchmark: validate_hex impact", "[benchmark]") {
  BENCHMARK("parse without hex validation") {
    return macad_parser::parse_mac_address_unsafe<opt_no_validation>(std::string_view{TEST_MAC_STR, 17});
  };

  BENCHMARK("parse with hex validation") {
    return macad_parser::parse_mac_address_unsafe<opt_hex_only>(std::string_view{TEST_MAC_STR, 17});
  };
}

TEST_CASE("Benchmark: uppercase vs lowercase formatting", "[benchmark]") {
  BENCHMARK("format with uppercase") {
    return macad_parser::format_mac_address<opt_uppercase>(TEST_MAC_VAL);
  };

  BENCHMARK("format with lowercase") {
    return macad_parser::format_mac_address<opt_lowercase>(TEST_MAC_VAL);
  };
}

TEST_CASE("Benchmark: delimiter comparison", "[benchmark]") {
  BENCHMARK("format with colon delimiter") {
    return macad_parser::format_mac_address<opt_colon>(TEST_MAC_VAL);
  };

  BENCHMARK("format with dash delimiter") {
    return macad_parser::format_mac_address<opt_dash>(TEST_MAC_VAL);
  };
}
