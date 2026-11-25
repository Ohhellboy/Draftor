// SPDX-License-Identifier: Apache-2.0
// Execute the exact documentation snippets with the existing Host fixture owners.
// Build the selected Host examples first; this script never edits engine sources.
import { execFileSync } from 'node:child_process';
import { readFile, writeFile, mkdir, stat } from 'node:fs/promises';
import path from 'node:path';
import MarkdownIt from 'markdown-it';
import { root } from './build.mjs';

if (!process.argv[2]) throw Error('Usage: npm run check:flows -- /absolute/path/to/parent-of-host-builds [host_minimal|host_composition|host_wubi86]');
const repo = path.resolve(root, '../..'), builds = path.resolve(process.argv[2]);
const output = path.join(builds, 'docs-flows');
await mkdir(output, { recursive: true });
const cases = [
  { page: 'api-core.md', example: 'host_minimal', libraries: [], body: String.raw`
    Resources resources(argv[1]); NoLearning counts; DemoDocument document;
    CandidateSession session(resources.full, resources.shared, counts, document, {4,1,1,64,64,32});
    require(session.valid(), "session invalid"); admit_text(session);
    docs_core::FirstCandidate flow(session);
    const auto query_first = [&] {
      auto state = flow.start("yuanxingji", epoch);
      for (std::size_t n = 0; state == SessionStatus::Pending || state == SessionStatus::WaitingState; ++n) {
        require(n < max_steps, "docs Core query stalled"); state = flow.advance(budget);
      }
      require(state == SessionStatus::Ready && flow.frame.text == u8"原型机", "docs Core candidate");
    };
    query_first(); const auto old = flow.frame; query_first();
    require(flow.choose(old).status == CommitStatus::Stale && document.calls == 0, "old frame committed");
    const auto shown = flow.frame; auto result = flow.choose(shown);
    require(result.status == CommitStatus::Preparing, "commit did not prepare");
    for (std::size_t n = 0; result.status == CommitStatus::Preparing; ++n) {
      require(n < max_steps, "docs Core commit stalled"); result = flow.advance_commit(budget);
    }
    require(result.status == CommitStatus::Accepted && document.text() == u8"原型机" && document.calls == 1, "docs Core commit");
    require(flow.choose(shown).status == CommitStatus::Accepted && document.calls == 1, "duplicate Core commit");
    require(flow.start("yuanxingji", epoch + 1) == SessionStatus::Stale, "wrong mount accepted");
    std::cout << "Core: candidate, stale frame, Accepted once, duplicate confirm, wrong MountEpoch: PASS\n";
` },
  { page: 'api-composition.md', example: 'host_composition', libraries: ['extension/libhanime_extension.a'], body: String.raw`
    Resources resources(argv[1]);
    for (const auto rules : {ex::SchemeRules::FullPinyin, ex::SchemeRules::R1Xiaohe}) {
      for (const bool multiple : {false, true}) {
        DemoDocument document;
        auto& scheme = rules == ex::SchemeRules::FullPinyin ? resources.full : resources.xiaohe;
        ex::Composition input(scheme, resources.shared, rules);
        ex::Submission submission(input, resources.shared, document);
        require(input.valid() && submission.valid(), "invalid docs composition");
        const std::string word = rules == ex::SchemeRules::FullPinyin ? "yuanxingji" : "yrxkji";
        const auto code = multiple ? word + word : word;
        for (const char key : code) {
          const auto state = docs_composition::append_key(input, key);
          require(state == ex::Status::PageReady || state == ex::Status::NoPrefixMatch ||
                  state == ex::Status::NoMappingForPrefix || state == ex::Status::Unresolved, "docs append error");
        }
        docs_composition::Frame frame;
        require(docs_composition::read_first(input, frame) == ex::Status::Ready && frame.text == u8"原型机", "docs text");
        const auto old = frame;
        require(docs_composition::finish(input, input.query_next()) == ex::Status::PageReady, "requery failed");
        require(docs_composition::choose(input, old) == ex::Status::Stale, "old composition selection accepted");
        require(docs_composition::read_first(input, frame) == ex::Status::Ready, "refresh failed");
        require(docs_composition::choose(input, frame) == ex::Status::Ready, "docs choose failed");
        if (multiple) {
          require(input.snapshot().state == ex::CompositionState::PartialSelected, "missing partial state");
          require(docs_composition::prepare(submission).result.core.status == CompositionStatus::InvalidRequest, "partial submitted");
          require(docs_composition::finish(input, input.query_next()) == ex::Status::PageReady, "second query");
          require(docs_composition::read_first(input, frame) == ex::Status::Ready, "second text");
          require(docs_composition::choose(input, frame) == ex::Status::Ready, "second selection");
        }
        require(input.snapshot().state == ex::CompositionState::AllSelected, "missing all-selected state");
        const auto prepared = docs_composition::prepare(submission);
        const std::string expected = multiple ? u8"原型机原型机" : u8"原型机";
        require(prepared.result.core.status == CompositionStatus::Ready && prepared.text == expected && document.calls == 0, "prepare wrote text");
        require(docs_composition::confirm(submission, prepared).core.status == CompositionStatus::Accepted, "docs submit failed");
        require(document.text() == expected && document.calls == 1 && input.snapshot().raw.empty(), "docs commit result");
        require(docs_composition::confirm(submission, prepared).core.status == CompositionStatus::Accepted && document.calls == 1, "duplicate composition commit");
      }
    }
    std::cout << "Composition: both schemes, one/two members, stale selection, partial rejection, preview without writes, Accepted once: PASS\n";
` },
  { page: 'api-wubi.md', example: 'host_wubi86', libraries: ['libbgime_demo_wubi86.a'], body: String.raw`
    Resources resources(argv[1]); Host host(resources, 1); docs_wubi::Query query{};
    for (const char key : std::string("abcd")) {
      query = docs_wubi::key(host.binding, host.session, 1, key);
      require(query.edit == wb::EditStatus::Changed && query.step.shared.status == SessionStatus::Ready, "docs Wubi query");
      require(host.document.calls == 0, "Wubi request wrote text");
    }
    require(query.step.request == wb::CommitReason::AutoUniqueFullCode &&
            query.step.code_revision == host.decoder.plan().revision, "missing current request");
    require(docs_wubi::key(host.binding, host.session, 1, 'a').edit == wb::EditStatus::Full, "fifth key accepted");
    require(docs_wubi::key(host.binding, host.session, 1, 'z').edit == wb::EditStatus::InvalidCode, "z accepted");
    docs_wubi::Frame frame;
    require(docs_wubi::read_first(host.session, frame) == SessionStatus::Ready && frame.text == u8"四" && output_has_text(frame.output), "docs Wubi text");
    const auto result = docs_wubi::confirm(host.binding, host.session, query.step.selection, query.step.request);
    require(result.status == CommitStatus::Accepted && host.document.calls == 1 && host.document.text() == u8"四", "docs Wubi commit");
    require(host.binding.confirm(query.step.selection, query.step.request).status == CommitStatus::Accepted && host.document.calls == 1, "duplicate Wubi commit");
    Host stale(resources, 1);
    require(docs_wubi::key(stale.binding, stale.session, 1, 'a').step.shared.status == SessionStatus::Ready, "stale setup");
    require(docs_wubi::read_first(stale.session, frame) == SessionStatus::Ready, "stale frame setup");
    require(docs_wubi::key(stale.binding, stale.session, 1, 'b').step.shared.status == SessionStatus::Ready, "stale replacement");
    require(docs_wubi::confirm(stale.binding, stale.session, frame.choice, wb::CommitReason::Explicit).status == CommitStatus::Stale && stale.document.calls == 0, "old Wubi choice committed");
    require(docs_wubi::read_first(stale.session, frame) == SessionStatus::Ready && frame.text == u8"二", "explicit frame");
    require(docs_wubi::confirm(stale.binding, stale.session, frame.choice, wb::CommitReason::Explicit).status == CommitStatus::Accepted &&
            stale.document.calls == 1 && stale.document.text() == u8"二", "explicit selection did not commit once");
    Host blocked(resources, 1, {"blocked", OutputInvocation::Explicit});
    for (const char key : std::string("abcd")) {
      query = docs_wubi::key(blocked.binding, blocked.session, 1, key);
      require(query.edit == wb::EditStatus::Changed && query.step.shared.status == SessionStatus::Ready, "blocked query lost candidate page");
    }
    require(query.step.request == wb::CommitReason::None, "blocked output requested auto commit");
    // Reuse a populated frame: restricted output must clear its text and choice.
    require(!frame.text.empty(), "blocked test requires an earlier populated frame");
    require(docs_wubi::read_first(blocked.session, frame) == SessionStatus::Ready &&
            frame.output == OutputStatus::Blocked && !output_has_text(frame.output) && frame.text.empty(), "blocked output status/frame");
    require(docs_wubi::confirm(blocked.binding, blocked.session, frame.choice, wb::CommitReason::Explicit).status == CommitStatus::Stale,
            "restricted frame retained a confirmable choice");
    blocked.no_writes();
    std::cout << "Wubi: normal/explicit selection, request without writes, confirmation once, invalid/full key, stale selection: PASS\n"
              << "Wubi restricted output: page=Ready output=Blocked text empty; no confirmable selection; sink_calls=0: PASS\n";
` },
];
const selected = cases.filter(test => !process.argv[3] || test.example === process.argv[3]);
if (!selected.length) throw Error('Unknown Host example: ' + process.argv[3]);
const results = [];
for (const test of selected) {
  const markdown = await readFile(path.join(root, 'content', test.page), 'utf8');
  const blocks = new MarkdownIt().parse(markdown, {}).filter(t => t.type === 'fence' && t.info.trim().split(/\s+/u)[0] === 'cpp');
  if (blocks.length !== 1) throw Error(`${test.page}: expected one main C++ flow`);
  const source = path.join(output, test.example + '.cpp'), executable = path.join(output, test.example);
  const main = path.join(repo, 'ime/bgime/examples', test.example, 'main.cpp');
  await writeFile(source, `#define main original_host_main\n#include ${JSON.stringify(main)}\n#undef main\n${blocks[0].content}\nint main(int argc, char** argv) {\n  if (argc != 2) return 2;\n  try {${test.body}\n    return 0;\n  } catch (const std::exception& error) { std::cerr << error.what() << '\\n'; return 1; }\n}\n`);
  const directory = path.join(builds, test.example);
  const libraries = [...test.libraries, 'core/libhanime_host_adapters.a', 'core/libhanime_core.a'].map(file => path.join(directory, file));
  for (const file of libraries) if (!(await stat(file)).isFile()) throw Error('Build Host example first: ' + file);
  const args = ['-std=c++17', '-Wall', '-Wextra', '-Werror', ...['core', 'extension', 'wubi'].flatMap(module => ['-I', path.join(repo, 'integration/product/dependencies', module, 'include')]), source];
  if (test.example === 'host_wubi86') args.push(path.join(repo, 'ime/bgime/examples/host_wubi86/fixture.cpp'));
  execFileSync('c++', [...args, ...libraries, '-o', executable], { stdio: 'inherit' });
  const stdout = execFileSync(executable, [path.join(directory, 'fixture')], { encoding: 'utf8' });
  await writeFile(path.join(output, test.example + '.log'), stdout);
  console.log(stdout.trim()); results.push({ page: test.page, example: test.example, result: 'PASS' });
}
await writeFile(path.join(output, 'results.json'), JSON.stringify(results, null, 2) + '\n');
