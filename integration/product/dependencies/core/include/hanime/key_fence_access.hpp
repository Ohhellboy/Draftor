#pragma once
#include "hanime/bounded_user_state.hpp"
#include "hanime/base_order.hpp"
namespace hanime::detail {
// Private experimental acquisition ports. No RankTicket or Count authority.
struct KeyFenceCapture {
  StateView view; StateExportCursor cutoff;
  std::size_t slots{0};
};
struct KeyFenceAccess {
  static bool capture(BoundedUserState&, KeyFenceCapture&);
  static StateExportStep advance(BoundedUserState&, KeyFenceCapture&, const WorkBudget&);
  static bool ram(BoundedUserState&, const KeyFenceCapture&, std::size_t, StateEntry&);
  static bool seal(BoundedUserState&, const KeyFenceCapture&);
  static StateStep journal(BoundedUserState&,const WorkBudget&);
};
class KeyFenceMember {
 public:
  KeyFenceMember(); ~KeyFenceMember();
  KeyFenceMember(const KeyFenceMember&)=delete;
  KeyFenceMember& operator=(const KeyFenceMember&)=delete;
  bool begin(BaseOrderIndex&, const BaseOrderCursor&, RuntimeLexemeRef);
  BaseOrderStep advance(BaseOrderIndex&, const BaseOrderCursor&, const WorkBudget&);
  std::uint32_t position() const;
  void cancel();
  static std::size_t control_bytes();
  static bool borrow_located(BaseOrderIndex&,const BaseOrderCursor&,BaseOrderCursor&);
  static std::uint32_t raw_paths(const BaseOrderCursor&);
 private:
  struct Impl; std::shared_ptr<Impl> impl_;
};
}
