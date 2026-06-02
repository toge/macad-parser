#include <string_view>

#include "catch2/catch_all.hpp"

#include "macad-parser.hpp"

// Verify that parse_mac_address is usable in a constant expression context
static_assert(macad_parser::parse_mac_address("AA:BB:CC:DD:EE:FF").has_value());
static_assert(macad_parser::parse_mac_address("AA:BB:CC:DD:EE:FF").value() == 0xAABBCCDDEEFF);
static_assert(macad_parser::parse_mac_address("aa:bb:cc:dd:ee:ff").value() == 0xAABBCCDDEEFF);
static_assert(macad_parser::parse_mac_address("01:23:45:67:89:AB").value() == 0x0123456789AB);
static_assert(!macad_parser::parse_mac_address("short").has_value());
static_assert(!macad_parser::parse_mac_address("").has_value());

// Verify strict options at compile time
static_assert(!macad_parser::parse_mac_address<macad_parser::parse_mac_options_strict>("01-23-45-67-89-AB").has_value());
static_assert(!macad_parser::parse_mac_address<macad_parser::parse_mac_options_strict>("01:23:45:67:89:AG").has_value());
static_assert(macad_parser::parse_mac_address<macad_parser::parse_mac_options_strict>("01:23:45:67:89:AB").has_value());

// Verify that format_mac_address is usable in a constant expression context
// (static_assert uses transient allocations, which work even when SSO threshold is exceeded)
static_assert(macad_parser::format_mac_address(0xAABBCCDDEEFFull) == "AA:BB:CC:DD:EE:FF");
static_assert(macad_parser::format_mac_address(0x0123456789ABull) == "01:23:45:67:89:AB");
static_assert(macad_parser::format_mac_address(0x000000000000ull) == "00:00:00:00:00:00");
static_assert(macad_parser::format_mac_address(0xFFFFFFFFFFFFull) == "FF:FF:FF:FF:FF:FF");

// Verify upper bits are masked at compile time
static_assert(macad_parser::format_mac_address(0xFFFFAABBCCDDEEFFull) == "AA:BB:CC:DD:EE:FF");

struct opt_constexpr_dash : public macad_parser::parse_mac_options {
  static constexpr char delimiter = '-';
};

struct opt_constexpr_lowercase : public macad_parser::parse_mac_options {
  static constexpr bool uppercase = false;
};

static_assert(macad_parser::format_mac_address<opt_constexpr_dash>(0x0123456789ABull) == "01-23-45-67-89-AB");
static_assert(macad_parser::format_mac_address<opt_constexpr_lowercase>(0xAABBCCDDEEFFull) == "aa:bb:cc:dd:ee:ff");

TEST_CASE("constexpr parse_mac_address") {
  SECTION("basic uppercase") {
    static_assert(macad_parser::parse_mac_address("AA:BB:CC:DD:EE:FF").has_value());
    static_assert(macad_parser::parse_mac_address("AA:BB:CC:DD:EE:FF").value() == 0xAABBCCDDEEFF);
    SUCCEED();
  }

  SECTION("lowercase") {
    static_assert(macad_parser::parse_mac_address("aa:bb:cc:dd:ee:ff").value() == 0xAABBCCDDEEFF);
    SUCCEED();
  }

  SECTION("non-symmetric") {
    static_assert(macad_parser::parse_mac_address("01:23:45:67:89:AB").value() == 0x0123456789AB);
    SUCCEED();
  }

  SECTION("rejects short string") {
    static_assert(!macad_parser::parse_mac_address("AA:BB:CC").has_value());
    SUCCEED();
  }

  SECTION("rejects empty string") {
    static_assert(!macad_parser::parse_mac_address("").has_value());
    SUCCEED();
  }
}

TEST_CASE("constexpr parse_mac_address strict validation") {
  SECTION("rejects wrong delimiter") {
    static_assert(!macad_parser::parse_mac_address<macad_parser::parse_mac_options_strict>("01-23-45-67-89-AB").has_value());
    SUCCEED();
  }

  SECTION("rejects non-hex character") {
    static_assert(!macad_parser::parse_mac_address<macad_parser::parse_mac_options_strict>("01:23:45:67:89:AG").has_value());
    SUCCEED();
  }

  SECTION("accepts valid MAC") {
    static_assert(macad_parser::parse_mac_address<macad_parser::parse_mac_options_strict>("01:23:45:67:89:AB").has_value());
    static_assert(macad_parser::parse_mac_address<macad_parser::parse_mac_options_strict>("01:23:45:67:89:AB").value() == 0x0123456789AB);
    SUCCEED();
  }
}

TEST_CASE("constexpr format_mac_address") {
  SECTION("basic uppercase") {
    static_assert(macad_parser::format_mac_address(0xAABBCCDDEEFFull) == "AA:BB:CC:DD:EE:FF");
    SUCCEED();
  }

  SECTION("non-symmetric") {
    static_assert(macad_parser::format_mac_address(0x0123456789ABull) == "01:23:45:67:89:AB");
    SUCCEED();
  }

  SECTION("all zeros") {
    static_assert(macad_parser::format_mac_address(0x000000000000ull) == "00:00:00:00:00:00");
    SUCCEED();
  }

  SECTION("all ones") {
    static_assert(macad_parser::format_mac_address(0xFFFFFFFFFFFFull) == "FF:FF:FF:FF:FF:FF");
    SUCCEED();
  }

  SECTION("upper bits masked") {
    static_assert(macad_parser::format_mac_address(0xFFFFAABBCCDDEEFFull) == "AA:BB:CC:DD:EE:FF");
    SUCCEED();
  }
}

TEST_CASE("constexpr format_mac_address with options") {
  SECTION("dash delimiter") {
    static_assert(macad_parser::format_mac_address<opt_constexpr_dash>(0x0123456789ABull) == "01-23-45-67-89-AB");
    SUCCEED();
  }

  SECTION("lowercase") {
    static_assert(macad_parser::format_mac_address<opt_constexpr_lowercase>(0xAABBCCDDEEFFull) == "aa:bb:cc:dd:ee:ff");
    SUCCEED();
  }
}

TEST_CASE("constexpr round-trip") {
  SECTION("parse then format") {
    static_assert(macad_parser::format_mac_address(macad_parser::parse_mac_address("AA:BB:CC:DD:EE:FF").value()) == "AA:BB:CC:DD:EE:FF");
    SUCCEED();
  }

  SECTION("format then parse") {
    static_assert(macad_parser::parse_mac_address(macad_parser::format_mac_address(0x0123456789ABull)).value() == 0x0123456789AB);
    SUCCEED();
  }
}
