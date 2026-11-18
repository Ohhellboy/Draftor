#pragma once
#include "hanime/sparse_corrections.hpp"
#include "hanime/base_order.hpp"
#include <memory>
#include "r2_memory.hpp"
#include <string>
namespace ime_device {class Resources;}
namespace r1 {
struct IndexCost {
 uint64_t reads=0,read_bytes=0,writes=0,write_bytes=0,controls=0,syncs=0;
 uint64_t directory_comparisons=0,personal_members=0,count_queries=0,facts=0,sort_steps=0;
 uint64_t new_identities=0,existing_updates=0,relations_added=0,merge_rows=0,services=0,max_step_us=0;
 uint64_t bitmap_words=0,positive_members=0,bit_updates=0,ref_bit_updates=0,coverage_waits=0,query_during_save=0,local_fallbacks=0;
 uint64_t checkpoints=0,journal_commits=0,snapshot_copy_bytes=0,anchor_steps=0,load_words=0;
 uint64_t certificate_rewrites=0;
 size_t refs=0,relations=0,memory=0,peak=0;
};
struct PersistenceView {
 bool loaded=false,coverage_complete=false,saving=false,checkpoint=false;
 unsigned phase=0;size_t dirty_words=0,journal_bytes=0;
 uint64_t saved_sequence=0,frozen_sequence=0;
};
// Counts remain exclusively in BoundedUserState / StateV3. This owner stores
// only resource-bound positions. No key/count table is constructed at open.
class PersonalIndex final:public hanime::SparseCorrections, public r2::ExternalObject {
 public:
 PersonalIndex(hanime::BoundedUserState&,ime_device::Resources&,std::string file,size_t limit=720896,size_t range_limit=512);
 ~PersonalIndex() override;
 void accepted(const hanime::CompositionLearningTicket&);
 void releaseClosed();
 bool service(); bool pending()const;
 bool healthy()const;const char* error()const;
 // Explicit offline fixture import; never invoked by runtime open/query.
 void start_import();void import_identity(hanime::RuntimeLexemeRef,const hanime::StableLexemeKey&);
 void finish_import();
 bool direct_enabled()const override{return true;}
 bool accepts(const hanime::CountSource&)const override;
 hanime::SparseStatistics statistics()const override;
 void begin_direct(const hanime::StateView&,std::string_view,hanime::BaseOrderIndex&,hanime::BaseOrderCursor&,
                   hanime::RankRule,hanime::RangeDirection,const hanime::StableLexemeKey&,int64_t,size_t) override;
 hanime::SparseStep direct_next(const hanime::WorkBudget&) override;
 uint32_t direct_start()const override;
 uint32_t direct_eligible()const override;
 bool current()const override;uint64_t revision()const override;
 void cancel()override;
 const char* failure_reason()const override{return error();}
 IndexCost cost()const;
 hanime::VerifiedResourceSnapshot relation_snapshot()const;
 hanime::VerifiedResourceMemory relation_memory()const;
 PersistenceView persistence()const;
 static size_t implementationBytes();
 static const uint32_t TargetAbi[10];
 private:struct Impl;std::unique_ptr<Impl> p;
};
}
