#include <string>

#include "catch2/catch_all.hpp"

#include "macad-parser.hpp"

TEST_CASE("mac address parser safe") {
  auto const a = std::string{"AA:BB:CC:DD:EE:FF"};

  auto const result = macad_parser::parse_mac_address(a);
  REQUIRE(result.has_value());
  REQUIRE(result.value() == 0xAABBCCDDEEFF);
}

TEST_CASE("mac address parser safe lower case") {
  auto const a = std::string{"aa:bb:cc:dd:ee:ff"};

  auto const result = macad_parser::parse_mac_address(a);
  REQUIRE(result.has_value());
  REQUIRE(result.value() == 0xAABBCCDDEEFF);
}

TEST_CASE("mac address parser safe (non symmetric)") {
  auto const a = std::string{"01:23:45:67:89:AB"};

  auto const result = macad_parser::parse_mac_address(a);
  REQUIRE(result.has_value());
  REQUIRE(result.value() == 0x0123456789AB);
}

TEST_CASE("mac address parser safe (strict validation)") {
  SECTION("rejects wrong delimiters") {
    auto const a = std::string{"01-23-45-67-89-AB"};
    auto const result = macad_parser::parse_mac_address<macad_parser::parse_mac_options_strict>(a);
    REQUIRE_FALSE(result.has_value());
  }

  SECTION("rejects non-hex safe characters") {
    auto const a = std::string{"01:23:45:67:89:AG"};
    auto const result = macad_parser::parse_mac_address<macad_parser::parse_mac_options_strict>(a);
    REQUIRE_FALSE(result.has_value());
  }
}

struct opt_delimiter : public macad_parser::parse_mac_options_strict {
  static constexpr char delimiter = '-';
};

TEST_CASE("custom delimiter safe (strict validation)") {
  SECTION("accepts custom delimiter") {
    auto const a = std::string{"01-23-45-67-89-AB"};
    auto const result = macad_parser::parse_mac_address<opt_delimiter>(a);
    REQUIRE(result.has_value());
    REQUIRE(result.value() == 0x0123456789AB);
  }

  SECTION("rejects wrong delimiters") {
    auto const a = std::string{"01:23:45:67:89:AF"};
    auto const result = macad_parser::parse_mac_address<opt_delimiter>(a);
    REQUIRE_FALSE(result.has_value());
  }
}
