# korobok

A lightweight, header-only C++20 library for parsing and serializing simple key-value data files in the `.krb` format.

## Table of Contents

- [File Format](#file-format)
- [Integration](#integration)
- [Building](#building)
- [Usage](#usage)
  - [Parsing a file](#parsing-a-file)
  - [Reading values](#reading-values)
  - [Iterating over tokens](#iterating-over-tokens)
  - [Modifying and creating tokens](#modifying-and-creating-tokens)
  - [Serialization](#serialization)
  - [Logging](#logging)
---

## File Format

Each line is a single entry in the `name:value` format. Supported types:

```
# This is a comment

# String (must be quoted)
app_name:"My Application"

# Numbers
version:1.5
count:32
# Type prefix
count_i16:i16;42
# Boolean
debug_mode:true
is_fullscreen:false

# Array of numbers
window_size:[1920,1080]
weights:[0.1,0.5,0.4]

# Array of strings
tags:["release","stable","lts"]

# Group (section marker)
$:graphics_settings
```
---

## Integration

The library is a single header file. Either copy `krb.h` directly into your project, or add it via CMake:

```cmake
add_subdirectory(korobok)
target_link_libraries(my_app PRIVATE krb::krb)
```

Then include the header:

```cpp
#include <krb.h>
```

---

## Building

### Requirements

- CMake 3.20+
- A C++20-capable compiler (GCC 12+, Clang 14+, MSVC 19.29+)
- Ninja (recommended, used by the presets)

### Library only

```bash
cmake --preset debug-gcc
cmake --build --preset debug-gcc
```

### With tests

```bash
cmake --preset debug-gcc-test
cmake --build build/debug-gcc-test
```

## Usage

### Parsing
```cpp
krb::Krb data;
const auto& result = data.from(
    "app_name:\"Hello World\"\n"
    "version:2.0\n"
    "debug_mode:true\n"
);
if (result.has_value()) {
    const auto& value = result.value(); // List of tokens
    // success
} else {
    // fail
}
```

---

### Reading values

#### `operator[]` with implicit type conversion

```cpp
korobok::krb data;
data.from("score:99.5\nname:\"Player One\"\nactive:true\nmoney:i16;2300\nhp:100");

float       score  = data["score"];  // 99.5
std::int16_t money  = data["money"];  // 2300
std::int32_t money  = data["hp"];  // 100
std::string name   = data["name"];   // "Player One"
bool        active = data["active"]; // true
```

> Throws `std::bad_variant_access` if the requested type doesn't match.
> For different numbers types you need to write prefix in krb file: i64; i32; i16; i8; u64; u32; u16; u8; f; d; 
#### `token()` — safe lookup via `std::optional`

```cpp
auto ref = data.token("score");
if (ref.has_value()) {
    auto val = ref.value().get().value<float>();
    if (val.has_value()) {
        float score = val.value().get(); // 99.5
    }
}
```

#### `at()` — throws if the token doesn't exist

```cpp
try {
    const krb::Token& t = data.at("score");
    float score = t; // implicit conversion
} catch (const std::invalid_argument& e) {
    // token not found
}
```

#### Reading arrays

```cpp
korobok::krb data;
data.from("coords:[10.0,20.0,30.0]\ntags:[\"a\",\"b\",\"c\"]\n");

// Array of numbers
auto nums = data["coords"].value<std::vector<float>>(); // or std::vector<std::string>
if (nums.has_value()) {
    for (auto v : nums.value().get()) {
        // 10.0, 20.0, 30.0
    }
}
```
Or

```cpp
// or std::vector<std::string>
std::vector<float> nums = data["coords"];
for (auto v : nums) {
    // 10.0, 20.0, 30.0
}

```

---

### Modifying and creating tokens

`operator[]` automatically creates a token if it doesn't exist yet:

```cpp
korobok::krb data;
data.from("version:1.0\n");

// Modify an existing token
data["version"] = 2.0f;

// Create new tokens
data["app_name"]    = std::string("korobok");
data["max_retries"] = 5.0f;
data["verbose"]     = false;

// From a string literal (stored as const char*)
data["build_tag"] = "nightly";
```

---

### Serialization

```cpp
korobok::krb data;
data.from("name:\"test\"\nvalue:42.0\nflags:[1,0,1]\n");

data["name"]  = std::string("updated");
data["extra"] = true;

std::string out = data.dump();
// name:"updated"
// value:42
// flags:[1,0,1]
// extra:true

// Save to file
std::ofstream file("output.krb");
file << out;
```

---

### Logging

Logs are disabled by default.:

```cpp
// Define before including the header
#define KRB_ENABLE_LOGS
#include <krb.h>
```
To plug in your own logging functions:
Note: logs should be based/(support) on `std::format()`.

```cpp
#define KRB_ENABLE_LOGS
#define KRB_USE_EXTERNAL_LOG_FUNCS
#define KRB_EXTERNAL_DEBUG(fmt, ...) custom_log::debug(fmt, ##__VA_ARGS__)
#define KRB_EXTERNAL_WARN(fmt, ...)  custom_log::warn(fmt, ##__VA_ARGS__)
#define KRB_EXTERNAL_ERROR(fmt, ...) custom_log::error(fmt, ##__VA_ARGS__)
#include <krb.h>
```
