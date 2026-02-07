#include <string>

#include "catch2/catch_all.hpp"

#include "macad-parser.hpp"

// Test with only delimiter defined - all other members should use defaults
struct opt_only_delimiter {
  static constexpr char delimiter = '-';
};

// Test with only uppercase defined - all other members should use defaults
struct opt_only_uppercase {
  static constexpr bool uppercase = false;
};

// Test with only validate_delimiters defined - all other members should use defaults
struct opt_only_validate_delimiters {
  static constexpr bool validate_delimiters = true;
};

// Test with only validate_hex defined - all other members should use defaults
struct opt_only_validate_hex {
  static constexpr bool validate_hex = true;
};

// Test with empty struct - all members should use defaults
struct opt_empty {
};

// Test with two members defined
struct opt_delimiter_and_uppercase {
  static constexpr char delimiter = '-';
  static constexpr bool uppercase = false;
};

// Test with validate_delimiters and delimiter
struct opt_validate_and_delimiter {
  static constexpr bool validate_delimiters = true;
  static constexpr char delimiter = '-';
};

TEST_CASE("partial options - only delimiter defined") {
  SECTION("parse with custom delimiter") {
    auto const a = std::string{"01-23-45-67-89-AB"};
    // Should parse without validation (validate_delimiters defaults to false)
    auto const result = macad_parser::parse_mac_address<opt_only_delimiter>(a);
    REQUIRE(result.has_value());
    REQUIRE(result.value() == 0x0123456789AB);
  }

  SECTION("format with custom delimiter") {
    auto const mac = 0xAABBCCDDEEFFull;
    auto const result = macad_parser::format_mac_address<opt_only_delimiter>(mac);
    // Should use dash delimiter and uppercase (default is true)
    REQUIRE(result == "AA-BB-CC-DD-EE-FF");
  }
}

TEST_CASE("partial options - only uppercase defined") {
  SECTION("parse with default delimiter") {
    auto const a = std::string{"AA:BB:CC:DD:EE:FF"};
    // Should parse with default delimiter ':'
    auto const result = macad_parser::parse_mac_address<opt_only_uppercase>(a);
    REQUIRE(result.has_value());
    REQUIRE(result.value() == 0xAABBCCDDEEFF);
  }

  SECTION("format with lowercase") {
    auto const mac = 0xAABBCCDDEEFFull;
    auto const result = macad_parser::format_mac_address<opt_only_uppercase>(mac);
    // Should use default delimiter ':' and lowercase
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
    // Should validate delimiter (which defaults to ':')
    auto const result = macad_parser::parse_mac_address<opt_only_validate_delimiters>(a);
    REQUIRE_FALSE(result.has_value());
  }

  SECTION("format with defaults") {
    auto const mac = 0xAABBCCDDEEFFull;
    auto const result = macad_parser::format_mac_address<opt_only_validate_delimiters>(mac);
    // Should use defaults: ':' and uppercase
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
    // Should use defaults: ':' and uppercase
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
    // Should accept since validate_delimiters defaults to false
    auto const result = macad_parser::parse_mac_address<opt_empty>(a);
    REQUIRE(result.has_value());
  }

  SECTION("format with all defaults") {
    auto const mac = 0xAABBCCDDEEFFull;
    auto const result = macad_parser::format_mac_address<opt_empty>(mac);
    // Should use defaults: ':' and uppercase
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
