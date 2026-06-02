#include <array>
#include <string_view>

#include "catch2/catch_all.hpp"
#include "macad-parser.hpp"

TEST_CASE("new features: constexpr parsing") {
  static constexpr auto mac_str = std::string_view{"AA:BB:CC:DD:EE:FF"};
  static constexpr auto result = macad_parser::parse_mac_address(mac_str);
  
  STATIC_REQUIRE(result.has_value());
  STATIC_REQUIRE(result.value() == 0xAABBCCDDEEFF);
}

TEST_CASE("new features: parse_mac_address_to_bytes") {
  auto const mac_str = std::string_view{"01:23:45:67:89:AB"};
  auto const result = macad_parser::parse_mac_address_to_bytes(mac_str);
  
  REQUIRE(result.has_value());
  auto const expected = std::array<std::uint8_t, 6>{0x01, 0x23, 0x45, 0x67, 0x89, 0xAB};
  REQUIRE(result.value() == expected);
}

struct opt_lower { static constexpr bool uppercase = false; };

TEST_CASE("new features: EUI-64") {
  SECTION("parse EUI-64") {
    auto const eui_str = std::string_view{"01:23:45:67:89:AB:CD:EF"};
    auto const result = macad_parser::parse_eui64_address(eui_str);
    REQUIRE(result.has_value());
    REQUIRE(result.value() == 0x0123456789ABCDEFull);
  }

  SECTION("format EUI-64") {
    auto const eui_val = 0x0123456789ABCDEFull;
    auto const result = macad_parser::format_eui64_address(eui_val);
    REQUIRE(result == "01:23:45:67:89:AB:CD:EF");
  }

  SECTION("format EUI-64 lowercase") {
    auto const eui_val = 0x0123456789ABCDEFull;
    auto const result = macad_parser::format_eui64_address<opt_lower>(eui_val);
    REQUIRE(result == "01:23:45:67:89:ab:cd:ef");
  }
}

TEST_CASE("new features: no delimiter") {
  auto const mac_str = std::string_view{"AABBCCDDEEFF"};
  auto const result = macad_parser::parse_mac_address_no_delimiter(mac_str);
  
  REQUIRE(result.has_value());
  REQUIRE(result.value() == 0xAABBCCDDEEFF);
}

TEST_CASE("new features: constexpr formatting") {
  static constexpr auto mac_val = 0x112233445566ull;
  
  auto constexpr get_mac_str = []() {
    std::array<char, 17> buf{};
    macad_parser::format_mac_address_to_buffer(mac_val, buf);
    return buf;
  };
  
  static constexpr auto buf = get_mac_str();
  STATIC_REQUIRE(buf[0] == '1');
  STATIC_REQUIRE(buf[1] == '1');
  STATIC_REQUIRE(buf[2] == ':');
  STATIC_REQUIRE(buf[15] == '6');
  STATIC_REQUIRE(buf[16] == '6');
}
