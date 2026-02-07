# macad-parser

MACアドレス文字列（例: `AA:BB:CC:DD:EE:FF`）を48bit整数（`std::uint64_t` の下位48bit）へ変換、および48bit整数をMACアドレス文字列へ変換するヘッダオンリーライブラリです。

- SIMDe（AVX2相当）でSIMD化（ARM等でもSIMDe経由で動作）
- オプションstructによるコンパイル時設定（`if constexpr`）
- デリミタ位置検証/16進検証はオプションが有効な場合のみ実行

## 依存関係

- CMake
- vcpkg（`vcpkg.json`）
  - `simde`
  - `catch2`（テスト用）

## ビルド

```sh
cmake -B build -DCMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake -DCMAKE_BUILD_TYPE=Release -S .
cmake --build build
```

ビルド成果物は `build/` 以下に生成されます。

## テスト

```sh
./build/test/all_test
```

## ベンチマーク

Catch2のベンチマーク機能を使用して、各オプション指定による処理速度の違いを計測できます。

```sh
# ベンチマークのみ実行
./build/test/all_test "[benchmark]"

# ベンチマーク設定のカスタマイズ
./build/test/all_test "[benchmark]" --benchmark-samples 100 --benchmark-resamples 10000
```

### ベンチマーク内容

- **parse_mac_address**: 各種オプション設定でのパース性能比較
  - デフォルトオプション（検証なし）
  - 厳密オプション（検証あり）
  - ナイーブ実装（SIMD不使用のベースライン）
- **format_mac_address**: 各種オプション設定でのフォーマット性能比較
  - 大文字/小文字
  - デリミタ（コロン/ハイフン）
  - ナイーブ実装（SIMD不使用のベースライン）
- **validate_delimiters impact**: デリミタ検証の有無による性能差
- **validate_hex impact**: 16進数検証の有無による性能差
- **round-trip**: パースとフォーマットの組み合わせ性能

### 性能の目安

一般的な環境での測定結果（参考値）:

- **パース性能**: SIMD実装は約16ns、ナイーブ実装は約18-22ns
- **フォーマット性能**: SIMD実装は約13-18ns、ナイーブ実装は約19-25ns
- **検証オーバーヘッド**: 
  - `validate_delimiters`: 約0.5ns
  - `validate_hex`: 約0.4ns
- **SIMD vs ナイーブ**: SIMD実装は約1.1-1.5倍高速

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
```

### API

#### `parse_mac_address`

```cpp
template <typename Options = macad_parser::parse_mac_options>
std::optional<std::uint64_t> parse_mac_address(std::string_view const mac) noexcept;
```

- 入力を32byteのローカルバッファへコピーしてから `parse_mac_address_unsafe` を呼ぶラッパー。
- 入力が短い場合のバッファオーバーリードを避けたい場合はこちらを推奨します。

#### `parse_mac_address_unsafe`

```cpp
template <typename Options = macad_parser::parse_mac_options>
std::optional<std::uint64_t> parse_mac_address_unsafe(std::string_view const mac) noexcept;
```

- 高速パース本体。
- **注意:** 内部で256bit(32byte)を読み込むため、呼び出し側が `std::string_view` の有効範囲外を読ませない保証が必要になります。
  - `std::string_view::data()` が有効なのは `[data(), data() + size())` の範囲であり、範囲外アクセスは未定義動作になり得ます。
- データ範囲を確定できない場合は利用しないでください。

#### `format_mac_address`

```cpp
template <typename Options = macad_parser::parse_mac_options>
std::string format_mac_address(std::uint64_t const mac);
```

- 48bit整数をMACアドレス文字列に変換します。
- SIMDEを利用してAVX2命令で高速に変換を行います。
- 上位16bitは無視され、下位48bitのみが使用されます。
- `Options` でデリミタと大文字・小文字をカスタマイズできます（`validate_delimiters` と `validate_hex` は無視されます）。

## オプション

内部動作を制御するにはオプションstructをテンプレート引数で指定します。

### `macad_parser::parse_mac_options`（デフォルト）

- `validate_delimiters = false`
- `validate_hex = false`
- `delimiter = ':'`
- `uppercase = true`

> パフォーマンス優先で、入力の妥当性チェックは行いません。16進数文字は大文字で出力されます。

### `macad_parser::parse_mac_options_strict`

- `validate_delimiters = true`
- `validate_hex = true`
- `delimiter = ':'`
- `uppercase = true`

> 厳密に `AA:BB:CC:DD:EE:FF` 形式かを検証します。16進数文字は大文字で出力されます。

### カスタムオプションの例

独自structを定義して独自のオプションを指定できます。

```cpp
struct opt_delimiter {
  static constexpr bool validate_delimiters = true;
  static constexpr bool validate_hex = false;
  static constexpr char delimiter = '-';
};

auto const v = macad_parser::parse_mac_address<opt_delimiter>("01-23-45-67-89-AB");

// 小文字の16進数文字を使用する例
struct opt_lowercase {
  static constexpr bool uppercase = false;
};

auto const mac_str = macad_parser::format_mac_address<opt_lowercase>(0xAABBCCDDEEFFull);
// mac_str == "aa:bb:cc:dd:ee:ff"
```

## 形式と返り値

- 入力: 先頭17文字が `XX?XX?XX?XX?XX?XX` 形式（`X`は16進、`?`はデリミタ）
- 返り値: `0xAABBCCDDEEFF` のようにビッグエンディアン表記の値を `std::uint64_t` に格納し、下位48bitを利用します
- パース失敗時: `std::nullopt`

## ライセンス

MIT License
