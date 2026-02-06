#include "catch2/catch_all.hpp"

#include "macad-parser.hpp"

TEST_CASE("mac address parser") {
  auto const a = std::string_view{"AA:BB:CC:DD:EE:FF               "}.substr(0, 17);

  auto const result = macad_parser::parse_mac_address_unsafe(a);
  REQUIRE(result.has_value());
  REQUIRE(result.value() == 0xAABBCCDDEEFF);
}

TEST_CASE("mac address parser lower case") {
  auto const a = std::string_view{"aa:bb:cc:dd:ee:ff               "}.substr(0, 17);

  auto const result = macad_parser::parse_mac_address_unsafe(a);
  REQUIRE(result.has_value());
  REQUIRE(result.value() == 0xAABBCCDDEEFF);
}

TEST_CASE("mac address parser (non symmetric)") {
  auto const a = std::string_view{"01:23:45:67:89:AB               "}.substr(0, 17);

  auto const result = macad_parser::parse_mac_address_unsafe(a);
  REQUIRE(result.has_value());
  REQUIRE(result.value() == 0x0123456789AB);
}

TEST_CASE("mac address parser (strict validation)") {
  SECTION("rejects wrong delimiters") {
    auto const a = std::string_view{"01-23-45-67-89-AB               "}.substr(0, 17);
    auto const result = macad_parser::parse_mac_address_unsafe<macad_parser::parse_mac_options_strict>(a);
    REQUIRE_FALSE(result.has_value());
  }

  SECTION("rejects non-hex characters") {
    auto const a = std::string_view{"01:23:45:67:89:AG               "}.substr(0, 17);
    auto const result = macad_parser::parse_mac_address_unsafe<macad_parser::parse_mac_options_strict>(a);
    REQUIRE_FALSE(result.has_value());
  }
}

struct opt_delimiter : public macad_parser::parse_mac_options_strict {
  static constexpr char delimiter = '-';
};

TEST_CASE("custom delimiter (strict validation)") {
  SECTION("rejects custom delimiters") {
    auto const a = std::string_view{"01-23-45-67-89-AB               "}.substr(0, 17);
    auto const result = macad_parser::parse_mac_address_unsafe<opt_delimiter>(a);
    REQUIRE(result.has_value());
    REQUIRE(result.value() == 0x0123456789AB);
  }

  SECTION("rejects wrong delimiters") {
    auto const a = std::string_view{"01:23:45:67:89:AF               "}.substr(0, 17);
    auto const result = macad_parser::parse_mac_address_unsafe<opt_delimiter>(a);
    REQUIRE_FALSE(result.has_value());
  }
}
