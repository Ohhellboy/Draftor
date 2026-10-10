# bgIME Wubi86 Host Demo v0.1

A scripted command-line demo of the existing `Wubi86Decoder`,
`DirectQueryBinding` and Core `CandidateSession`. It types codes one key at a
time, reads Chinese candidates from resources and commits a selected candidate
to its own `TextCommitSink`.

[host_minimal](../host_minimal/README.md) demonstrates Core queries using complete
fixture codes. [host_composition](../host_composition/README.md) uses Extension
for Full Pinyin and Xiaohe composition. This example uses the Wubi module's
direct query plan; it does not build Extension or use a Pinyin syllable table or
SegmentPlanner. Core remains **0.13.1-rc.1**, public API **0.13.0**, from the
Draftor Alpha **v0.1.0-alpha.1** source baseline.

## Build and run

Use CMake 3.20 or later and a C++17 compiler. Run from the repository root
(`source/` in this review workspace), starting with a new build directory:

```sh
cmake -S ime/bgime/examples/host_wubi86 -B build/host_wubi86 -DCMAKE_BUILD_TYPE=Debug
cmake --build build/host_wubi86 --parallel 2
./build/host_wubi86/bgime_host_wubi86 ./build/host_wubi86/fixture
```

The example CMake defines a local library over the original
[wubi.cpp](../../../../integration/product/dependencies/wubi/src/wubi.cpp) and
links the existing Core and POSIX Host adapter. It does not use the product
CMake entry. The default build also runs the executable's `--build-fixture`
mode, which calls `ResourceBuilder` and writes `shared.hns`, `wubi86.hnm` and a
manifest to the build directory. Normal runs open these files read-only.
If building a named target, use `bgime_host_wubi86_fixture` to build both the
executable and its resources. No B2, fonts, ESP32 SDK or device is needed.

For ASan and UBSan, use a separate clean directory:

```sh
cmake -S ime/bgime/examples/host_wubi86 -B build/host_wubi86-asan -DCMAKE_BUILD_TYPE=Debug -DHANIME_ENABLE_SANITIZERS=ON
cmake --build build/host_wubi86-asan --parallel 2
ASAN_OPTIONS=halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ./build/host_wubi86-asan/bgime_host_wubi86 ./build/host_wubi86-asan/fixture
```

The executable exits with status 1 if any check fails. CMake also registers it
with CTest. The verified environment is macOS 15.5 on Apple Silicon, Apple
Clang 17 and CMake 4.3.2; other platforms have not been tested here.

## Artificial resources

Every mapping below is **fixture data**, not a claim about real Wubi86 codes:

| Fixture code | Text | Purpose |
|---|---|---|
| `a` | 一 | One-key state |
| `ab` | 二 | Two-key state and explicit selection |
| `abc` | 三 | Three-key state and Backspace recovery |
| `abcd` | 四 | Four-key state; two mapping rows share one stable identity |
| `bbbb` | 同文, 同文 | Two different stable identities with identical text |
| `abcc` | No match | An exact miss must not fall back to `abc` |

[fixture.cpp](fixture.cpp) builds six artificial identities and seven mapping
rows. Their full keys and isolated namespace `0x57423836484f5354` (`WB86HOST`)
are declared in [fixture.hpp](fixture.hpp). The `四` identity also has an
explicitly blocked output form for a negative test. No production Registry,
dictionary or existing small fixture is changed.

The Host runs Core's shared/scheme validation, exact namespace and bundle
pairing, and text-access admission before input. Wrong namespace/bundle pairs
are rejected. Queries, display and commits keep the original resource and
selection lifetime checks and bounded work budgets.

## Input, candidates and submission

All active edits go through `DirectQueryBinding`, which delegates to the real
`Wubi86Decoder` and connects its `DirectQueryPlan` to `CandidateSession`.
The reference profile accepts `a`–`y`, folds uppercase ASCII, and permits four
letters. The program checks one through four keys, a fifth key returning `Full`,
`z` and a digit returning `InvalidCode`, Backspace/retyping, and cancel/leave.
Rejected edits retain the current code and revision.

Core supplies the candidate page and UTF-8 text. The demo copies them only
inside `CandidateSession::present`, selects a real handle, and confirms through
the binding. Hardcoded Chinese strings are fixture inputs or test expectations;
they are never used as a substitute for a query or sink payload.

The one-slot `bbbb` page has `has_more=true`. Its next page has the same text
but a different key. Neither page causes an automatic request. A separate
two-slot case shows both identities together. Old pages, replaced queries,
invalid selections and an expired resource activation cannot authorize a new
commit. Cancelling or leaving writes no text.

`CommitReason::AutoUniqueFullCode` is a **request**, not a text commit. The
public binding emits it for the first current page only when the code is full,
the profile enables it, the page contains exactly one candidate with no further
candidates, the order is current and the output text is valid. Duplicate
mapping paths for one identity do not create a second candidate. A blocked
output form prevents the request, and Core also rejects explicit submission
of that form.

The demo observes `sink_calls=0` after the unique request, then lets its Host
confirm the supplied selection. Accepted commits clear the decoder and write
once; repeating confirmation does not write again. The successful explicit
documents contain `二` and `同文`; the Host-confirmed automatic request produces
`四`. Each document has exactly one accepted sink call.

Draftor additionally checks autoSafe data, candidate-window state, resource
identity, input/event context and the current editing target in its
[product auto-commit path](../../../../integration/product/src/wubi_product.cpp).
This Host does not reproduce those product checks or replace their conditions.

## Learning and limits

Learning is unfinished and disabled here. The public binding starts an Exact
query. After its initialization tick, the Host calls Core's existing
`restart_first(..., ScoringMode::BaseOnly)` on the same logical input, before
any count lookup. The example-local disabled count source supplies an isolated
owner/namespace, never a fabricated learned count. Core's post-acceptance
notification still runs and returns `PausedUnavailable`; nothing is learned
or persisted. The program checks that frequency lookups remain zero.

This demonstrates the public Wubi module with small valid resources. It does
not validate real dictionary coverage, production `open_verified` authentication,
the full B2 set, Draftor's complete auto-commit policy, sink refusal/retry or
unknown outcomes. It is not an installable system input method. No Windows,
other POSIX host or MCU compatibility is claimed.
