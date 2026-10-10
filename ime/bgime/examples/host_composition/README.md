# bgIME Host Composition Demo v0.1

A small command-line consumer of the existing Core and Extension. It types Full
Pinyin and Xiaohe input one key at a time, selects Chinese candidates and submits
the complete composition to its own `TextCommitSink`.

Unlike [host_minimal](../host_minimal/README.md), which sends fixture codes to
Core directly, this example uses `Composition::edit_raw` and `query_next` after
each key. Extension owns the spelling rules, planning and selected members;
Core owns resource access, ranking, text resolution and the commit transaction.
`Submission` connects the selected composition to that transaction.

The example uses Core **0.13.1-rc.1** (API **0.13.0**) and Extension
**0.8.4-rc.1**, from the Draftor Alpha **v0.1.0-alpha.1** source baseline.
It does not build or run the Draftor application, editor or firmware.

## Build and run

Run these commands from the repository root, starting with a new build directory.
You need CMake 3.20 or later and a C++17 compiler. The example has been run on
macOS 15.5, Apple Silicon, with Apple Clang 17 and CMake 4.3.2.

```sh
cmake -S ime/bgime/examples/host_composition -B build/host_composition -DCMAKE_BUILD_TYPE=Debug
cmake --build build/host_composition --parallel 2
./build/host_composition/bgime_host_composition ./build/host_composition/fixture
```

CMake builds `HanIME::Core`, `HanIME::Extension`, the existing Host adapter and
`hanime_build_fixture`. The fixture tool generates the paired resource files in
the build directory automatically. No B2 download, fonts, ESP32 SDK or manual
resource copying is needed. The executable's single argument is that fixture
directory. A failed check reports an error and exits with status 1.

For ASan and UBSan, use the existing Core sanitizer option in a separate directory:

```sh
cmake -S ime/bgime/examples/host_composition -B build/host_composition-asan -DCMAKE_BUILD_TYPE=Debug -DHANIME_ENABLE_SANITIZERS=ON
cmake --build build/host_composition-asan --parallel 2
ASAN_OPTIONS=halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ./build/host_composition-asan/bgime_host_composition ./build/host_composition-asan/fixture
```

Both builds register the same executable with CTest, if you prefer that runner.

## What happens

- **Full Pinyin:** `SchemeRules::FullPinyin` receives `yuanxingji` one character
  at a time. Its existing syllable table and planner produce a three-syllable
  path for the fixture word 「原型机」. The program also cancels an active query,
  backspaces to `yuanxingj`, retypes `i` and queries again.
- **Xiaohe:** `SchemeRules::R1Xiaohe` receives `yrxkji` one character at a time.
  The current rules use two-letter boundaries (`yr | xk | ji`) and query the
  paired `xiaohe` resource directly. The odd-length prefix `yrxkj` is reported
  as incomplete. The planner's `syllables=3` counts those three encoding units;
  no Full Pinyin converter is involved.
- **Two members:** each scheme then types its code twice, without inserting a
  separator. The planner reports six syllables/encoding units and two words.
  Selecting the first candidate leaves the second code as remaining raw input.
  Selecting the next candidate reaches `AllSelected`. The entire text
  「原型机原型机」is delivered in **one sink call**.

Candidate text is read through `Composition::read_text`, and selection uses the
handle returned by `Composition::selection`. Expected strings in the program
are assertions, not candidate data or a source of committed text.

The public planning report is checked for `direct_range=false`, the expected
syllable and word counts, and the chosen and covered ranges. This demonstrates
planning beyond a single exact-code lookup. A later query may report
`queries=0` with cache hits: Extension can reuse completed planning facts after
checking their current input and resource context.

Submission follows `prepare` → budgeted `advance` → `confirm`. The program
checks the prepared UTF-8 text and final sink contents, including these cases:

- editing while a query or submission is active returns `Busy`;
- after cancelling and editing, old input stamps/plans are invalid, and old
  operations/selections return `Stale`;
- a stale selection or partially selected composition cannot submit;
- confirming an accepted transaction again returns its historical `Accepted`
  result without writing a second time.

Each scenario has a fresh document. All four scenarios finish with exactly one
sink call each. For example, the actual two-member Full Pinyin run includes:

```text
plan: syllables=6 words=2 next=[0,10) covered_end=20 direct_range=false queries=3 cache_hits=18
selected: members=1 text="原型机" remaining_raw="yuanxingji" state=PartialSelected
partial submission: InvalidRequest, sink_calls=0
plan: syllables=3 words=1 next=[10,20) covered_end=20 direct_range=false queries=0 cache_hits=6
selected: members=2 text="原型机原型机" remaining_raw="" state=AllSelected
submission: Preparing -> Ready -> Accepted; UTF-8="原型机原型机" sink_calls=1
duplicate confirm: Accepted, sink_calls=1; input=Idle; learning=DisabledByConfiguration
```

Candidate and identity lines are omitted from this excerpt; the executable
prints them too. Every tick keeps the configured read and work budgets.

## Resources and limits

The unchanged [small fixture](../../../../integration/product/dependencies/core/src/small_fixture.cpp)
is artificial test data built by the existing `ResourceBuilder`. Its isolated
namespace is `0x48414e494d453242`. Both scheme mappings resolve to the same
`fixture::kPrototype` stable identity. Repeating that word is enough to exercise
two selected members, so this example needs no additional dictionary or fixture.

Resources pass the existing shared/scheme validation and bundle pairing before
use. This demonstrates valid small resources, not the production `open_verified`
authentication path or the full B2 dictionary. The fixture and these inputs do
not validate a complete Xiaohe spelling table, arbitrary multi-word sentences,
ambiguous segmentation or production dictionary coverage. The current Xiaohe
interface is not a general Shuangpin plugin API.

Learning remains unfinished. The example uses Extension's existing disabled
count source and disabled composition learning policy; count lookups stay at
zero. It does not create or test user state or persistence.

This is a scripted Host demo, not an installable macOS system input method.
It does not exercise Wubi, candidate pagination, alternate output forms,
resource replacement, sink refusal/retry or unknown outcomes. Other POSIX hosts,
Windows and MCUs have not been tested here.
