#include <string>

#include "catch2/catch_all.hpp"

#include "macad-parser.hpp"

// デリミタ定義のみの定義 - 他のメンバはデフォルト値を使用
struct opt_only_delimiter {
  static constexpr char delimiter = '-';
};

// 大文字小文字の定義のみ - 他のメンバはデフォルト値を使用
struct opt_only_uppercase {
  static constexpr bool uppercase = false;
};

// デリミタ検証の定義のみ - 他のメンバはデフォルト値を使用
struct opt_only_validate_delimiters {
  static constexpr bool validate_delimiters = true;
};

// 16進数文字の検証の定義のみ - 他のメンバはデフォルト値を使用
struct opt_only_validate_hex {
  static constexpr bool validate_hex = true;
};

// 空の構造体 - 全てのメンバはデフォルト値を使用
struct opt_empty {
};

// 2つのメンバが定義された構造体
struct opt_delimiter_and_uppercase {
  static constexpr char delimiter = '-';
  static constexpr bool uppercase = false;
};

// デリミタ検証とデリミタの定義がある構造体
struct opt_validate_and_delimiter {
  static constexpr bool validate_delimiters = true;
  static constexpr char delimiter = '-';
};

TEST_CASE("partial options - only delimiter defined") {
  SECTION("parse with custom delimiter") {
    auto const a = std::string{"01-23-45-67-89-AB"};
    // 検証なしのパース
    auto const result = macad_parser::parse_mac_address<opt_only_delimiter>(a);
    REQUIRE(result.has_value());
    REQUIRE(result.value() == 0x0123456789AB);
  }

  SECTION("format with custom delimiter") {
    auto const mac = 0xAABBCCDDEEFFull;
    auto const result = macad_parser::format_mac_address<opt_only_delimiter>(mac);
    // -デリミタと大文字使用の指定
    REQUIRE(result == "AA-BB-CC-DD-EE-FF");
  }
}

TEST_CASE("partial options - only uppercase defined") {
  SECTION("parse with default delimiter") {
    auto const a = std::string{"AA:BB:CC:DD:EE:FF"};
    // デフォルトのデリミタ ':' でパース
    auto const result = macad_parser::parse_mac_address<opt_only_uppercase>(a);
    REQUIRE(result.has_value());
    REQUIRE(result.value() == 0xAABBCCDDEEFF);
  }

  SECTION("format with lowercase") {
    auto const mac = 0xAABBCCDDEEFFull;
    auto const result = macad_parser::format_mac_address<opt_only_uppercase>(mac);
    // デフォルトのデリミタ ':' と小文字を使用
    REQUIRE(result == "aa:bb:cc:dd:ee:ff");
  }
}

TEST_CASE("partial options - only validate_delimiters defined") {
  SECTION("accepts valid delimiters") {
    auto const a = std::string{"01:23:45:67:89:AB"};
    auto const result = macad_parser::parse_mac_address<opt_only_validate_delimiters>(a);
    REQUIRE(result.has_value());
    REQUIRE(result.value() == 0x0123456789AB);
  }

  SECTION("rejects invalid delimiters") {
    auto const a = std::string{"01-23-45-67-89-AB"};
    // デリミタ検証を行う（デフォルトは ':'）
    auto const result = macad_parser::parse_mac_address<opt_only_validate_delimiters>(a);
    REQUIRE_FALSE(result.has_value());
  }

  SECTION("format with defaults") {
    auto const mac = 0xAABBCCDDEEFFull;
    auto const result = macad_parser::format_mac_address<opt_only_validate_delimiters>(mac);
    // デフォルトのデリミタ ':' と大文字を使用
    REQUIRE(result == "AA:BB:CC:DD:EE:FF");
  }
}

TEST_CASE("partial options - only validate_hex defined") {
  SECTION("accepts valid hex characters") {
    auto const a = std::string{"01:23:45:67:89:AB"};
    auto const result = macad_parser::parse_mac_address<opt_only_validate_hex>(a);
    REQUIRE(result.has_value());
    REQUIRE(result.value() == 0x0123456789AB);
  }

  SECTION("rejects invalid hex characters") {
    auto const a = std::string{"01:23:45:67:89:XY"};
    auto const result = macad_parser::parse_mac_address<opt_only_validate_hex>(a);
    REQUIRE_FALSE(result.has_value());
  }

  SECTION("format with defaults") {
    auto const mac = 0xAABBCCDDEEFFull;
    auto const result = macad_parser::format_mac_address<opt_only_validate_hex>(mac);
    // デフォルトのデリミタ ':' と大文字を使用
    REQUIRE(result == "AA:BB:CC:DD:EE:FF");
  }
}

TEST_CASE("partial options - empty struct uses all defaults") {
  SECTION("parse with all defaults") {
    auto const a = std::string{"AA:BB:CC:DD:EE:FF"};
    auto const result = macad_parser::parse_mac_address<opt_empty>(a);
    REQUIRE(result.has_value());
    REQUIRE(result.value() == 0xAABBCCDDEEFF);
  }

  SECTION("parse accepts invalid delimiters (no validation)") {
    auto const a = std::string{"01-23-45-67-89-AB"};
    // 検証なしのパース
    auto const result = macad_parser::parse_mac_address<opt_empty>(a);
    REQUIRE(result.has_value());
  }

  SECTION("format with all defaults") {
    auto const mac = 0xAABBCCDDEEFFull;
    auto const result = macad_parser::format_mac_address<opt_empty>(mac);
    // デフォルトのデリミタ ':' と大文字を使用
    REQUIRE(result == "AA:BB:CC:DD:EE:FF");
  }
}

TEST_CASE("partial options - two members defined") {
  SECTION("delimiter and uppercase") {
    auto const mac = 0x0123456789ABull;
    auto const result = macad_parser::format_mac_address<opt_delimiter_and_uppercase>(mac);
    REQUIRE(result == "01-23-45-67-89-ab");
  }

  SECTION("validate_delimiters and delimiter") {
    SECTION("accepts matching delimiter") {
      auto const a = std::string{"01-23-45-67-89-AB"};
      auto const result = macad_parser::parse_mac_address<opt_validate_and_delimiter>(a);
      REQUIRE(result.has_value());
      REQUIRE(result.value() == 0x0123456789AB);
    }

    SECTION("rejects non-matching delimiter") {
      auto const a = std::string{"01:23:45:67:89:AB"};
      auto const result = macad_parser::parse_mac_address<opt_validate_and_delimiter>(a);
      REQUIRE_FALSE(result.has_value());
    }

    SECTION("format uses specified delimiter and default uppercase") {
      auto const mac = 0xAABBCCDDEEFFull;
      auto const result = macad_parser::format_mac_address<opt_validate_and_delimiter>(mac);
      REQUIRE(result == "AA-BB-CC-DD-EE-FF");
    }
  }
}

TEST_CASE("partial options - round-trip with partial definitions") {
  SECTION("round-trip with only delimiter") {
    auto const original = 0x0123456789ABull;
    auto const formatted = macad_parser::format_mac_address<opt_only_delimiter>(original);
    REQUIRE(formatted == "01-23-45-67-89-AB");
    auto const parsed = macad_parser::parse_mac_address<opt_only_delimiter>(formatted);
    REQUIRE(parsed.has_value());
    REQUIRE(parsed.value() == original);
  }

  SECTION("round-trip with only uppercase") {
    auto const original = 0xAABBCCDDEEFFull;
    auto const formatted = macad_parser::format_mac_address<opt_only_uppercase>(original);
    REQUIRE(formatted == "aa:bb:cc:dd:ee:ff");
    auto const parsed = macad_parser::parse_mac_address<opt_only_uppercase>(formatted);
    REQUIRE(parsed.has_value());
    REQUIRE(parsed.value() == original);
  }

  SECTION("round-trip with empty options") {
    auto const original = 0xFEDCBA987654ull;
    auto const formatted = macad_parser::format_mac_address<opt_empty>(original);
    auto const parsed = macad_parser::parse_mac_address<opt_empty>(formatted);
    REQUIRE(parsed.has_value());
    REQUIRE(parsed.value() == original);
  }
}
