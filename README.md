# macad-parser

MACアドレス文字列（例: `AA:BB:CC:DD:EE:FF`）を48bit整数（`std::uint64_t` の下位48bit）へ変換、および48bit整数をMACアドレス文字列へ変換するヘッダオンリーライブラリです。

- SIMDe（AVX2相当）でSIMD化（ARM等でもSIMDe経由で動作）
- 全ての主要機能が `constexpr` 対応（C++23以上の `if consteval` を活用し、コンパイル時パース/フォーマットが可能）
- オプションstructによるコンパイル時設定（`if constexpr`）
- デリミタ位置検証/16進検証はオプションが有効な場合のみ実行
- EUI-64のサポート
- デリミタなし形式のサポート

## 依存関係

- CMake
- C++23以上のコンパイラ（`std::byteswap`, `if consteval`, `std::expected` 等を使用）
- vcpkg（`vcpkg.json`）
  - `simde`
  - `catch2`（テスト用）

※ `CMakeLists.txt` は利用可能なら `-std=c++26`、なければ `-std=c++23` を選択します。

## ビルド

```sh
cmake -B build -DCMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake -DCMAKE_BUILD_TYPE=Release -S .
cmake --build build
```

## テスト

```sh
./build/test/all_test "~[benchmark]"
```

## 使い方

`macad-parser.hpp` をインクルードして使用します。

```cpp
#include "macad-parser.hpp"

// MACアドレス文字列を整数に変換
auto const v = macad_parser::parse_mac_address("AA:BB:CC:DD:EE:FF");
if (v) {
  // v.value() == 0xAABBCCDDEEFF
}

// 整数をMACアドレス文字列に変換
auto const mac_str = macad_parser::format_mac_address(0xAABBCCDDEEFFull);
// mac_str == "AA:BB:CC:DD:EE:FF"

// コンパイル時パース
static constexpr auto mac_val = macad_parser::parse_mac_address("01:23:45:67:89:AB");
static_assert(mac_val == 0x0123456789AB);
```

### 主要API

#### `parse_mac_address`
```cpp
template <typename Options = macad_parser::parse_mac_options>
auto constexpr parse_mac_address(std::string_view const mac) noexcept -> std::optional<std::uint64_t>;
```
- 安全なMACアドレスパース。内部でバッファコピーを行い、SIMDによる高速パースを実行します。
- コンパイル時評価にも対応しています。

#### `parse_mac_address_to_bytes`
```cpp
template <typename Options = macad_parser::parse_mac_options>
auto constexpr parse_mac_address_to_bytes(std::string_view const mac) noexcept -> std::optional<std::array<std::uint8_t, 6>>;
```
- パース結果を直接 `std::array<std::uint8_t, 6>` として取得します。

#### `format_mac_address`
```cpp
template <typename Options = macad_parser::parse_mac_options>
auto constexpr format_mac_address(std::uint64_t const mac) -> std::string;
```
- 48bit整数をMACアドレス文字列に変換します。

#### `parse_eui64_address` / `format_eui64_address`
- EUI-64（64bit拡張識別子、23文字）のパースとフォーマットを行います。

#### `parse_mac_address_no_delimiter`
- `AABBCCDDEEFF` のようなデリミタなし形式をパースします。

## FREESTANDING 対応

コア機能（パース・バッファ出力）は動的確保を行わないため、FREESTANDING 環境（組み込み・カーネル・`wasm32-unknown-unknown` など）でも利用できます。

### 有効化方法

| 方法 | 手順 |
|---|---|
| コンパイラフラグ | `-DMACAD_PARSER_FREESTANDING` を付与（`g++ -DMACAD_PARSER_FREESTANDING -I . ...`） |
| CMake | `-DENABLE_FREESTANDING=ON`（テストに freestanding 検証が追加される） |

`wasm32-unknown-unknown`（`__wasm__ && !__wasi__ && !__EMSCRIPTEN__`）では自動で有効になります。

### 無効化される機能

| 機能 | hosted | FREESTANDING | 代替 |
|---|---|---|---|
| `format_mac_address` | `std::string` を返す | 無効（動的確保のため） | `format_mac_address_to_buffer` + 自前バッファ |
| `format_eui64_address` | `std::string` を返す | 無効（動的確保のため） | `format_eui64_to_buffer` + 自前バッファ |

FREESTANDING 検証は `test/freestanding_check.cpp` を `-ffreestanding -fno-exceptions -fno-rtti -nostdlib++`（libstdc++ リンクなし）でビルド・実行し、動的確保（`operator new` 等）やホスト専用ライブラリへの逆戻りを検出します。

※ 依存の SIMDe は未使用でも `<cmath>` を取り込むため、GCC 16 のように `<cmath>` を hosted 専用とする実装では、`test/freestanding_check.cpp` が行っている `HUGE_VAL` 先行定義と `SIMDE_NO_NATIVE` の定義が必要です。また `wasm32-unknown-unknown` では simde の `_Float16` 検出が誤動作するため `-DSIMDE_FLOAT16_API=SIMDE_FLOAT16_API_PORTABLE` の付与を推奨します。

## ライセンス

MIT License
