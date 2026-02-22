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

TEST_CASE("mac address parser safe (invalid input)") {
  SECTION("rejects empty string") {
    auto const result = macad_parser::parse_mac_address(std::string{});
    REQUIRE_FALSE(result.has_value());
  }

  SECTION("rejects single character") {
    auto const result = macad_parser::parse_mac_address(std::string{"A"});
    REQUIRE_FALSE(result.has_value());
  }

  SECTION("rejects string shorter than 17 chars") {
    auto const result = macad_parser::parse_mac_address(std::string{"AA:BB:CC"});
    REQUIRE_FALSE(result.has_value());
  }

  SECTION("rejects string of exactly 16 chars (one short)") {
    auto const result = macad_parser::parse_mac_address(std::string{"AA:BB:CC:DD:EE:F"});
    REQUIRE_FALSE(result.has_value());
  }

  SECTION("rejects non-MAC string of sufficient length") {
    auto const result = macad_parser::parse_mac_address<macad_parser::parse_mac_options_strict>(std::string{"not a mac address"});
    REQUIRE_FALSE(result.has_value());
  }

  SECTION("rejects all non-hex characters with strict validation") {
    auto const result = macad_parser::parse_mac_address<macad_parser::parse_mac_options_strict>(std::string{"GG:HH:II:JJ:KK:LL"});
    REQUIRE_FALSE(result.has_value());
  }

  SECTION("rejects hex without delimiters with strict delimiter validation") {
    auto const result = macad_parser::parse_mac_address<macad_parser::parse_mac_options_strict>(std::string{"AABBCCDDEEFF12345"});
    REQUIRE_FALSE(result.has_value());
  }

  SECTION("rejects spaces as delimiter with strict validation") {
    auto const result = macad_parser::parse_mac_address<macad_parser::parse_mac_options_strict>(std::string{"AA BB CC DD EE FF"});
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
