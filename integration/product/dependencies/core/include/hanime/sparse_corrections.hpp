#pragma once
#include "hanime/bounded_user_state.hpp"
#include "hanime/verified_resource.hpp"
namespace hanime {
class BaseOrderIndex;class BaseOrderCursor;
enum class SparseStatus { Pending, Ready, Item, End, Fallback, Stale };
struct SparseStep { SparseStatus status{SparseStatus::Pending}; WorkCost cost; RankItem item; };
struct SparseOpenStep { SparseStatus status{SparseStatus::Pending}; WorkCost cost; std::shared_ptr<VerifiedResourceReader> reader; };
// Lifetime owned by the same resource adapter as the authoritative scheme.
// Each open step activates at most one file owner (open + size metadata);
// these synchronous controls are inside the measured caller segment.
class SparseResourcePort {
 public: virtual ~SparseResourcePort()=default;
 virtual SparseOpenStep open_sparse(const WorkBudget&)=0;
 virtual std::uint64_t sparse_generation() const=0;
 virtual void close_sparse(const std::shared_ptr<const VerifiedResourceReader>&) {}
};
struct SparseConfig { std::size_t entries{32768}, code_buckets{4096}, member_buckets{16384};
 void* (*allocate)(std::size_t){nullptr}; void (*release)(void*){nullptr}; };
struct SparseStatistics {
 std::uint64_t builds{0}, updates{0}, state_records{0}, ram_records{0}, reverse_rows{0}, mapping_rows{0};
 std::uint64_t comparisons{0}, queries{0}, members{0}, corrections{0}, fallbacks{0}, rebuilds{0};
 std::uint64_t state_reads{0},state_bytes{0},static_reads{0},static_bytes{0},work{0};
 std::size_t entries{0},peak_entries{0},allocated_bytes{0};
};
// Derived cache only: confirmed snapshot + absolute tail + captured RAM. Every
// complete view is tagged with authority identity and revision. Hash chains are
// bounded, advanced one link per charged work item, never whole-state hot scans.
class SparseCorrections {
 public:
 SparseCorrections(BoundedUserState&,SparseResourcePort&,SparseConfig={});
 virtual ~SparseCorrections();
 SparseCorrections(const SparseCorrections&)=delete;
 virtual void begin(const StateView&,std::string_view code);
 virtual void begin_located(const StateView& v,std::string_view c,BaseOrderIndex&,const BaseOrderCursor&){begin(v,c);}
 virtual SparseStep prepare(const WorkBudget&);
 virtual SparseStep next(const WorkBudget&); // All D(q), once per range.
 virtual SparseStep member(const StableLexemeKey&,const WorkBudget&); // Item iff in D.
 // R1 complete range source. Default preserves the independent exact reference.
 virtual bool direct_enabled()const{return false;}
 virtual void begin_direct(const StateView&,std::string_view,BaseOrderIndex&,BaseOrderCursor&,RankRule,RangeDirection,const StableLexemeKey&,std::int64_t,std::size_t){}
 virtual SparseStep direct_next(const WorkBudget&){return {SparseStatus::Fallback,{},{}};}
 virtual std::uint32_t direct_start()const{return 0;}
 virtual std::uint32_t direct_eligible()const{return 0;}
 virtual bool current() const;
 virtual std::uint64_t revision()const;
 virtual void cancel();
 virtual SparseStatistics statistics() const;
 virtual const char* failure_reason() const;
 virtual std::uint32_t raw_paths()const{return 0;}
 virtual bool accepts(const CountSource&)const{return true;}
 protected: SparseCorrections();
 private: struct Impl;std::unique_ptr<Impl> p_;
};
}
