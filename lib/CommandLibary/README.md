# CmdLib

A lightweight, cross-platform command **parser & builder** for structured command strings used by microcontrollers and host software.

---

## Format

Valid command form:

```
!![HEADER1[:HEADER2[:...]]]:MSG_KIND:COMMAND{key=value,key2=value2}##
```

- `HEADER1..N` *(optional)* — contextual routing hops such as `MASTER`, `ARM#1`, etc. (all headers before the message kind).  
- `MSG_KIND` *(required)* — message category (`REQUEST`, `CONFIRM`, `ERROR`, ...).  
- `COMMAND` *(required)* — the command name (`MAKE_STAR`, `SEND_STAR`, ...).  
- `{key=value,...}` *(optional)* — named parameters. When no parameters are present the braces may be omitted, e.g. `!!CONFIRM:SEND_STAR##`.

**Important:** positional params (like `100,red,80`) are **no longer supported** — every parameter inside `{}` **must** be `key=value`. If any token has no `=`, the parser returns an error.

---

## Features

- Cross-platform: works on **Arduino** (uses `String`) and **standard C++** (uses `std::string`)
- Automatic Arduino mode detection when `ARDUINO` is defined (or define `CMDLIB_ARDUINO`)
- Small footprint: Arduino build uses fixed arrays (no dynamic allocation)
- Simple API: build commands programmatically and serialize with `toString()`; parse strings with `parse()`
- All parameters are **named** (`key=value`) — consistent and explicit

> Note: the library does **not** accept positional parameters. Any tokens that appear before `MSG_KIND` are treated as generic headers and preserved round-trip, but they are not interpreted beyond ordering.

---

## Quick migration notes

- **Old (positional / location):**  
  `!!MASTER:[ARM#]:REQUEST:MAKE_STAR{100,red,80,20}##` → **invalid now**

- **New (named only + headers kept):**  
  `!!MASTER:ARM#1:REQUEST:MAKE_STAR{speed=100,color=red,brightness=80,size=20}##`

---

## API & Usage

The library exposes a `Command` struct and a `parse()` function in the `cmdlib` namespace.

### C++ (STL) example

```cpp
#include <iostream>
#include "CmdLib.h"

int main() {
  std::string in = "!!REQUEST:MAKE_STAR{speed=100,color=red,brightness=80,size=20}##";
  cmdlib::Command cmd;
  std::string err;

  if (cmdlib::parse(in, cmd, err)) {
    std::cout << "msgKind: " << cmd.msgKind << "\n";    // "REQUEST"
    std::cout << "command: " << cmd.command << "\n";    // "MAKE_STAR"
    std::cout << "speed: " << cmd.getNamed("speed") << "\n"; // "100"
  } else {
    std::cerr << "Parse error: " << err << "\n";
  }

  // Build a command:
  cmdlib::Command out;
  out.msgKind = "CONFIRM";
  out.command = "SEND_STAR";
  out.setNamed("speed", "3");
  out.setNamed("color", "red");
  std::string s = out.toString();
  // s: !!CONFIRM:SEND_STAR{speed=3,color=red}##
}
```

### Arduino example

```cpp
#include "CmdLib.h"

void setup() {
  Serial.begin(115200);
  cmdlib::Command cmd;
  String err;
  String in = "!!REQUEST:MAKE_STAR{speed=100,color=red,brightness=80,size=20}##";
  if (cmdlib::parse(in, cmd, err)) {
    Serial.println(cmd.msgKind);         // "REQUEST"
    Serial.println(cmd.command);         // "MAKE_STAR"
    Serial.println(cmd.getNamed("color")); // "red"
  } else {
    Serial.print("Parse error: "); Serial.println(err);
  }

  cmdlib::Command out;
  out.msgKind = "CONFIRM";
  out.command = "SEND_STAR";
  out.setNamed("speed", "3");
  Serial.println(out.toString()); // "!!CONFIRM:SEND_STAR{speed=3}##"
}

void loop() { }
```

---

## Error handling & validation

The parser validates and returns an error message when:

- Missing prefix `!!` or missing suffix `##`
- Unbalanced or malformed braces `{}` (if present)
- Empty command name
- **Positional-style tokens** inside braces (a token without `=` causes an error: _"Positional params not supported; expected key=value"_)
- Empty parameter key (e.g. `=value`) is rejected

When `parse()` fails it returns `false` and fills the `error` string (or `String` on Arduino) with a short description. New error strings include "Malformed braces" (closing brace without opening) and "Incomplete header" (when a message kind and command are not both present).

---

## Configuration

### Arduino mode
- Automatically enabled when `ARDUINO` is defined.
- You may explicitly force Arduino mode by defining `CMDLIB_ARDUINO` prior to including the header.

### Max parameters (Arduino only)
- Default maximum: **12** parameters.
- Override by defining `CMDLIB_MAX_PARAMS` before including:

```cpp
#define CMDLIB_MAX_PARAMS 20
#include "CmdLib.h"
```

---

## Unit tests

A C++17 test harness exercises the standard-library build (multi-header support, round-trip serialization, flag parameters, error detection, and clear/overwrite semantics).

```bash
g++ -std=c++17 CmdLibTest.cpp -o CmdLibTest
./CmdLibTest
```

Expected output (current suite):

```
[PASS] build round-trip
[PASS] parse with flags
[PASS] error detection
[PASS] clear and overwrite
[PASS] multi-header parse

Summary: 5 passed, 0 failed
```

---

## Implementation updates (October 2025)

- **Header splitting:** `cmdlib::Command` now keeps leading routing segments in a `headers` container while promoting the final two tokens to `msgKind` and `command`. Both tokens are mandatory; the parser rejects inputs missing either part with an "Incomplete header" error.
- **Named parameters only:** All parsing paths enforce `key=value` pairs. A positional-style token returns the error _"Positional params not supported; expected key=value"_.
- **Stricter brace handling:** The parser now catches stray closing braces (`}##`) that lack an opening `{` before them and reports "Malformed braces".
- **Test coverage:** `CmdLibTest.cpp` was updated to align with the new API, verify multi-hop headers (`A:B:C:D:KIND:CMD`), and assert the new error conditions.


## API Reference (summary)

**Struct `cmdlib::Command`** (fields / methods)

- `std::string` / `String` fields:
  - `msgKind` — optional (e.g. `REQUEST`, `CONFIRM`)
  - `command` — required (e.g. `MAKE_STAR`)
- `void setNamed(key, value)` — set or update a named parameter
- `std::string / String getNamed(key, default="")` — read parameter value
- `std::string / String toString()` — serialize to command string
- `void clear()` — clear the command

**Function**

- `bool parse(input, Command &out, std::string &error)` (or `String &error` in Arduino)
  - Parses `input` into `out`
  - Returns `true` on success, `false` on error (error message set)

---

## Examples (common commands)

- Make star (request):
  ```
  !!REQUEST:MAKE_STAR{speed=100,color=red,brightness=80,size=20}##
  ```

- Send star:
  ```
  !!REQUEST:SEND_STAR##
  ```

- Confirm send:
  ```
  !!CONFIRM:SEND_STAR##
  ```

- Error message:
  ```
  !!REQUEST:ERROR{message=Timeout occurred}##
  ```

---

## License

**Timo's Cookie License** — usage cost: **1 cookie**.  

(You may include this README in your project and distribute as you like — just leave a cookie for Timo 🍪.)
