# bgIME Core minimal Host demo v0.1

A standalone command-line example for Core **0.13.1-rc.1**, public API **0.13.0**. It uses Core's real resource, candidate and commit APIs with the existing small fixture. Draftor's application, editor, display, Bluetooth stack and ESP32 SDK are not needed.

The two inputs are complete, manually declared fixture codes:

| Scheme | Fixture code | Candidate |
|---|---|---|
| `full_pinyin` | `yuanxingji` | 原型机 |
| `xiaohe` | `yrxkji` | 原型机 |

Both mappings must resolve to the same stable lexical identity. This demonstrates Core queries by scheme code, not Full Pinyin or Xiaohe keystroke parsing, segmentation or composition. See [host_composition](../host_composition/README.md) for the Core + Extension keystroke and composition demo.

## Build and run

Requirements: CMake 3.20 or later, a C++17 compiler and POSIX file APIs. The example is tested on macOS 15.5, Apple Silicon, with Apple Clang 17 and CMake 4.3.2. Other POSIX environments have not been verified; Windows and MCU targets are outside this example's scope.

From the Draftor source root (`source/` in the isolated review workspace), start with a build directory that does not yet exist:

```sh
cmake -S ime/bgime/examples/host_minimal -B build/host_minimal -DCMAKE_BUILD_TYPE=Debug
cmake --build build/host_minimal --target bgime_host_minimal --parallel 2
./build/host_minimal/bgime_host_minimal ./build/host_minimal/fixture
```

Configuration uses the existing Core CMake entry directly. It enables Host adapters, fixtures and tools, disables Core's regression suite, and builds only the dependencies of this example: `HanIME::Core`, `HanIME::Host`, the small-fixture library and `hanime_build_fixture`.

The build runs that existing fixture tool automatically. It uses `ResourceBuilder` and [small_fixture.cpp](../../../../integration/product/dependencies/core/src/small_fixture.cpp) to write `shared.hns`, `full_pinyin.hnm`, `xiaohe.hnm` and a manifest under the build directory. No B2 files, production Registry, private data or manual resource copies are needed. The executable opens these files read-only.

The executable exits with zero only after all its checks succeed. To run it through CTest:

```sh
ctest --test-dir build/host_minimal --output-on-failure
```

For a separate ASan/UBSan build, use Core's existing sanitizer option:

```sh
cmake -S ime/bgime/examples/host_minimal -B build/host_minimal-asan \
  -DCMAKE_BUILD_TYPE=Debug -DHANIME_ENABLE_SANITIZERS=ON
cmake --build build/host_minimal-asan --target bgime_host_minimal --parallel 2
ASAN_OPTIONS=halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  ctest --test-dir build/host_minimal-asan --output-on-failure
```

## What the example checks

`HostResourceFile` supplies the generated resources. `PagedSharedResource` and `PagedSchemeIndex` perform their normal complete validation and namespace/bundle pairing checks, followed by text-access admission. Work advances in bounded slices of at most one read, 17 read bytes and three work items; the example checks each returned cost.

`CandidateSession` queries and publishes an identity-only page, then obtains its UTF-8 text through `request_text` and `present`. The Host copies the borrowed display text within the callback. It selects slot 0 using a real `SelectionHandle`, calls `confirm`, and advances Core until its own `TextCommitSink` accepts the complete text. Display and document output come from Core; expected strings in the source are assertions only.

Each scheme query submits once, so the final document contains `原型机原型机`. Reconfirming each accepted selection must return the existing acceptance without another sink call. A separate `tongwen` fixture query checks First/After/Before pages with identical text but different stable identities. An expired page selection and an out-of-range slot must return `Stale` without changing the document. Mismatched shared-bundle IDs must also be rejected.

Learning is unfinished and is neither implemented nor enabled here. The example follows Core's existing BaseOnly and rejected-learning test strategies. Its local `NoLearning` adapter reports learning unavailable and has no user state, frequency counts or persistence. Core still calls its notification hook after accepting text; this does not record learning. The demo checks that ranking never reads a count source.

These are valid small test resources in the fixture's isolated namespace. The demo uses the public `open` / `advance_validation` path; it does not exercise `open_verified`, production release authentication or the complete B2 resource set. Sink rejection/retry, uncertain outcomes, state persistence and full input-scheme integration are not tested here.

## Example output

Captured from a successful run on the macOS environment above:

```text
fixture: shared + full_pinyin + xiaohe fully validated
registry_namespace=0x48414e494d453242; mismatched bundle pairs rejected
scheme=full_pinyin code=yuanxingji candidate[0]="原型机" ref=1 paths=2
key=SLK1:48414e494d453242000000000000000000000000000000000000000000000001
commit=Accepted text="原型机" sink_calls=1; duplicate confirm: no extra call
scheme=xiaohe code=yrxkji candidate[0]="原型机" ref=1 paths=1
key=SLK1:48414e494d453242000000000000000000000000000000000000000000000001
commit=Accepted text="原型机" sink_calls=2; duplicate confirm: no extra call
paging: tongwen First/After/Before, two distinct identities with text="同文"
expired snapshot + invalid slot: Stale, no sink calls
same stable identity across schemes: yes
learning: unavailable, count-source reads=0, no user state
final UTF-8 document="原型机原型机" sink_calls=2
demo checks: OK
```

See the [bgIME module entry](../../README.en.md) for the broader module boundaries and resource licensing notes.
