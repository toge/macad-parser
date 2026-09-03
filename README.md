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

## WASI 環境対応

`wasm32-wasip1`（旧 `wasm32-wasi`）環境でも、コア機能はヘッダオンリーかつ `constexpr` 主体のため利用できます。本ライブラリは `<string>` 等の hosted ヘッダを必要としますが、wasi-sdk sysroot を用いた `wasm32-wasip1` ではそのままビルドできます（`wasm3` 等で実行可能）。

### 有効化方法

| 方法 | 手順 |
|---|---|
| コンパイラフラグ | `-DMACAD_PARSER_WASI_MINIMAL` を付与（`g++ -DMACAD_PARSER_WASI_MINIMAL -I . ...`） |
| CMake | `-DENABLE_WASI_MINIMAL=ON`（`macad-parser.hpp:18`、`CMakeLists.txt:54`） |

`wasm32-wasip1` / `wasm32-emscripten` は WASI/hosted とみなすため自動では有効にならず、WASI 上で最小構成を検証したい場合は明示的に `-DMACAD_PARSER_WASI_MINIMAL` を付与してください。`wasm32-unknown-unknown`（`__wasm__ && !__wasi__ && !__EMSCRIPTEN__`）では自動で有効になります（bare-metal 向けの互換）。

### WASI Minimal の挙動

`MACAD_PARSER_WASI_MINIMAL` 定義時、例外送出は `MACAD_PARSER_THROW` マクロ（`macad-parser.hpp`）経由で `std::abort()` に置き換わります。`<string>` は wasip1 では WASI 経由で利用可能なため無効化しません。`-fno-exceptions` でビルドできます。

## ライセンス

MIT License
