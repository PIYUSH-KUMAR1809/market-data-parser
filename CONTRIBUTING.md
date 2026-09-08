# Contributing

## Build

C++20 compiler (Clang or GCC) and CMake 3.20+.

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

NSE L2 + miniLZO is **off** by default (GPL-2+). Opt in only if you accept that license:

```bash
cmake -B build -S . -DENABLE_NSE_L2_LZO=ON
```

Sanitizer build (what CI runs):

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug -DENABLE_SANITIZERS=ON
cmake --build build -j --target unit_tests
ctest --test-dir build --output-on-failure
```

## Do not

- Do not commit NASDAQ ITCH, NSE FO/L2, or other exchange dumps (`data/`, `*.NASDAQ_ITCH50`, `*.bin`).
- Do not add exchange data to CI artifacts.
- Do not describe this as a live HFT engine or a price-level book unless you actually implement one.

## Tests

Unit tests are synthetic. They do not need multi-gigabyte dumps. Do not upload those files in a PR.
