#include <string>

#include "catch2/catch_all.hpp"

#include "macad-parser.hpp"

TEST_CASE("format mac address from integer") {
  auto const mac = 0xAABBCCDDEEFFull;
  auto const result = macad_parser::format_mac_address(mac);
  REQUIRE(result == "AA:BB:CC:DD:EE:FF");
}

TEST_CASE("format mac address (non symmetric)") {
  auto const mac = 0x0123456789ABull;
  auto const result = macad_parser::format_mac_address(mac);
  REQUIRE(result == "01:23:45:67:89:AB");
}

TEST_CASE("format mac address (all zeros)") {
  auto const mac = 0x000000000000ull;
  auto const result = macad_parser::format_mac_address(mac);
  REQUIRE(result == "00:00:00:00:00:00");
}

TEST_CASE("format mac address (all ones)") {
  auto const mac = 0xFFFFFFFFFFFFull;
  auto const result = macad_parser::format_mac_address(mac);
  REQUIRE(result == "FF:FF:FF:FF:FF:FF");
}

TEST_CASE("format mac address (boundary values)") {
  SECTION("minimum value") {
    auto const mac = 0x000000000000ull;
    auto const result = macad_parser::format_mac_address(mac);
    REQUIRE(result == "00:00:00:00:00:00");
  }

  SECTION("maximum 48-bit value") {
    auto const mac = 0xFFFFFFFFFFFFull;
    auto const result = macad_parser::format_mac_address(mac);
    REQUIRE(result == "FF:FF:FF:FF:FF:FF");
  }

  SECTION("value with upper bits set (should be masked)") {
    auto const mac = 0xFFFFAABBCCDDEEFFull;
    auto const result = macad_parser::format_mac_address(mac);
    REQUIRE(result == "AA:BB:CC:DD:EE:FF");
  }
}

TEST_CASE("format mac address (various patterns)") {
  SECTION("pattern 1") {
    auto const mac = 0x112233445566ull;
    auto const result = macad_parser::format_mac_address(mac);
    REQUIRE(result == "11:22:33:44:55:66");
  }

  SECTION("pattern 2") {
    auto const mac = 0xFEDCBA987654ull;
    auto const result = macad_parser::format_mac_address(mac);
    REQUIRE(result == "FE:DC:BA:98:76:54");
  }

  SECTION("pattern 3 - mixed digits and letters") {
    auto const mac = 0xA1B2C3D4E5F6ull;
    auto const result = macad_parser::format_mac_address(mac);
    REQUIRE(result == "A1:B2:C3:D4:E5:F6");
  }
}

struct opt_delimiter_dash : public macad_parser::parse_mac_options {
  static constexpr char delimiter = '-';
};

struct opt_delimiter_space : public macad_parser::parse_mac_options {
  static constexpr char delimiter = ' ';
};

struct opt_lowercase : public macad_parser::parse_mac_options {
  static constexpr bool uppercase = false;
};

struct opt_lowercase_dash : public macad_parser::parse_mac_options {
  static constexpr char delimiter = '-';
  static constexpr bool uppercase = false;
};

TEST_CASE("format mac address with custom delimiter") {
  SECTION("dash delimiter") {
    auto const mac = 0x0123456789ABull;
    auto const result = macad_parser::format_mac_address<opt_delimiter_dash>(mac);
    REQUIRE(result == "01-23-45-67-89-AB");
  }

  SECTION("space delimiter") {
    auto const mac = 0xAABBCCDDEEFFull;
    auto const result = macad_parser::format_mac_address<opt_delimiter_space>(mac);
    REQUIRE(result == "AA BB CC DD EE FF");
  }
}

TEST_CASE("format mac address with lowercase") {
  SECTION("lowercase default delimiter") {
    auto const mac = 0xAABBCCDDEEFFull;
    auto const result = macad_parser::format_mac_address<opt_lowercase>(mac);
    REQUIRE(result == "aa:bb:cc:dd:ee:ff");
  }

  SECTION("lowercase with dash delimiter") {
    auto const mac = 0x0123456789ABull;
    auto const result = macad_parser::format_mac_address<opt_lowercase_dash>(mac);
    REQUIRE(result == "01-23-45-67-89-ab");
  }

  SECTION("lowercase various patterns") {
    auto const mac = 0xFEDCBA987654ull;
    auto const result = macad_parser::format_mac_address<opt_lowercase>(mac);
    REQUIRE(result == "fe:dc:ba:98:76:54");
  }
}

TEST_CASE("round-trip conversion") {
  SECTION("parse then format") {
    auto const original = std::string{"AA:BB:CC:DD:EE:FF"};
    auto const parsed = macad_parser::parse_mac_address(original);
    REQUIRE(parsed.has_value());
    auto const formatted = macad_parser::format_mac_address(parsed.value());
    REQUIRE(formatted == original);
  }

  SECTION("format then parse") {
    auto const original = 0x0123456789ABull;
    auto const formatted = macad_parser::format_mac_address(original);
    auto const parsed = macad_parser::parse_mac_address(formatted);
    REQUIRE(parsed.has_value());
    REQUIRE(parsed.value() == original);
  }

  SECTION("round-trip with various values") {
    std::vector<std::uint64_t> test_values = {
      0x000000000000ull,
      0x000000000001ull,
      0x0000000000FFull,
      0x00000000FFFFull,
      0x000000FFFFFFull,
      0x0000FFFFFFFFull,
      0x00FFFFFFFFFFull,
      0xFFFFFFFFFFFFull,
      0x123456789ABCull,
      0xFEDCBA987654ull,
    };

    for (auto const& val : test_values) {
      auto const formatted = macad_parser::format_mac_address(val);
      auto const parsed = macad_parser::parse_mac_address(formatted);
      REQUIRE(parsed.has_value());
      REQUIRE(parsed.value() == val);
    }
  }
}
