#include "hanime/small_fixture.hpp"

#include <algorithm>

namespace hanime::fixture {
namespace {

LexemeSourceRecord approved(StableLexemeKey key, LexemeKind kind,
                            std::string identity, std::string provenance,
                            std::int32_t weight, std::string default_output,
                            std::vector<OutputRelationInput> relations = {}) {
  LexemeSourceRecord record;
  record.stable_key = key;
  record.status = RegistryStatus::Approved;
  record.kind = kind;
  record.identity_definition = std::move(identity);
  record.provenance = std::move(provenance);
  record.base_weight = weight;
  record.default_output_utf8 = std::move(default_output);
  record.output_relations = std::move(relations);
  return record;
}

}  // namespace

ResourceBuildInput make_small_input(bool reverse_approved_lexeme_order) {
  ResourceBuildInput input;
  input.registry_namespace = kRegistryNamespace;
  input.source_name = "hanime-explicit-small-identity-fixture";
  input.source_version = "2026-09-08.v1";
  input.relation_basis = "manually declared fixture relations";

  input.lexemes = {
      approved(kPrototype, LexemeKind::Word, "fixture lexeme: prototype",
               "fixture/prototype", 80, "原型机",
               {{"zh-Hant", "原型機", "", "fixture preferred form", true}}),
      approved(kDefaultOnly, LexemeKind::Word,
               "fixture lexeme: default-only", "fixture/default-only", 40,
               "单一输出"),
      approved(kFullPinyinOnly, LexemeKind::Word,
               "fixture lexeme: full-pinyin-only", "fixture/full-only", 30,
               "全拼独有"),
      approved(kSameTextA, LexemeKind::Word,
               "fixture lexeme: same-text identity A", "fixture/same-a", 25,
               "同文"),
      approved(kSameTextB, LexemeKind::Word,
               "fixture lexeme: same-text identity B", "fixture/same-b", 20,
               "同文"),
      approved(kSupplementaryCharacter, LexemeKind::Character,
               "fixture character: U+20000", "fixture/u20000", 15,
               "𠀀"),
      approved(kMultipleVariantForms, LexemeKind::Character,
               "fixture character: unresolved variant choice",
               "fixture/multi-variant", 10, "发",
               {{"zh-Variant", "發", "traditional context",
                 "curated relation A", false, ConditionKind::WordOrSense},
                {"zh-Variant", "髮", "hair sense",
                 "curated relation B", false, ConditionKind::WordOrSense}}),
  };

  if (reverse_approved_lexeme_order) {
    std::reverse(input.lexemes.begin(), input.lexemes.end());
  }

  LexemeSourceRecord pending;
  pending.status = RegistryStatus::PendingMapping;
  pending.kind = LexemeKind::Word;
  pending.identity_definition = "unapproved real-data placeholder";
  pending.provenance = "fixture/pending";
  pending.default_output_utf8 = "待映射";
  input.lexemes.push_back(std::move(pending));

  input.mappings = {
      {"full_pinyin", "yuanxingji", kPrototype, 20},
      {"full_pinyin", "yuanxingji", kPrototype, 10},
      {"full_pinyin", "danyishuchu", kDefaultOnly, 10},
      {"full_pinyin", "quanpinduyou", kFullPinyinOnly, 10},
      {"full_pinyin", "tongwen", kSameTextA, 20},
      {"full_pinyin", "tongwen", kSameTextB, 10},
      {"full_pinyin", "u20000", kSupplementaryCharacter, 10},
      {"xiaohe", "yrxkji", kPrototype, 20},
      {"xiaohe", "djiyiu", kDefaultOnly, 10},
      {"xiaohe", "tw", kSameTextA, 20},
      {"xiaohe", "tw", kSameTextB, 10},
      {"xiaohe", "u2", kSupplementaryCharacter, 10},
  };
  return input;
}

}  // namespace hanime::fixture
