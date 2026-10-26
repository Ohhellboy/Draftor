#pragma once
#include "horizontal_presentation.hpp"
#include "vertical_presentation.hpp"
#include "draftor/vertical_ab.hpp"
#include "ime_presentation_policy.hpp"

namespace s3app {
enum class LayoutMode : std::uint8_t { Horizontal, Vertical };
enum class VerticalImeGeometryState : std::uint8_t { Idle, ActiveBase, Expanded };
enum class VerticalPresentationState : std::uint8_t { NonChinese, ChineseBase, ChineseExpanded };
struct PresentationFrame {
  LayoutMode mode=LayoutMode::Horizontal;
  std::uint64_t revision=0;
};
// One boot-owned route for Find/storage/navigation. The two bounded geometry
// objects outlive this controller and never own or copy the shared editor.
class PresentationController final : public editor::presentation::PresentationPort {
 public:
  PresentationController(editor::SharedEditorView shared,HorizontalPresentation& h,
      VerticalPresentation& v,editor::presentation::PresentationPort* explicit_vertical=nullptr) noexcept
      : shared_(shared),horizontal_(h),vertical_(v),vertical_port_(explicit_vertical?*explicit_vertical:v),
        mode_(explicit_vertical?LayoutMode::Vertical:LayoutMode::Horizontal),custom_vertical_(explicit_vertical!=nullptr) {}
  LayoutMode mode() const noexcept { return mode_; }
  bool horizontalActive() const noexcept { return mode_==LayoutMode::Horizontal; }
  PresentationFrame frame() const noexcept { return {mode_,revision_}; }
  bool current(PresentationFrame f) const noexcept { return f.mode==mode_&&f.revision==revision_; }
  bool framePending() const noexcept { return presented_!=revision_; }
  bool framePresented(PresentationFrame f) noexcept {
    if(!current(f))return false;
    presented_=revision_;return true;
  }
  unsigned compositionWidth()const noexcept{return composition_peak_;}
  // Latched by an accepted semantic edit, including resource-loading input.
  // Query/selection/presentation microstates cannot end this reservation.
  void beginComposition(bool full_pinyin)noexcept{compositionScheme(full_pinyin);composition_active_=true;chinese_context_=true;}
  // Composition idle is semantic only. The visible CN surface owns geometry.
  void chineseContext(bool visible)noexcept{
    chinese_context_=visible;
    if(!visible)vertical_anchor_context_.valid=false;
  }
  VerticalPresentationState verticalPresentationState()const noexcept{
    return horizontalActive()||!chinese_context_?VerticalPresentationState::NonChinese:
      candidate_columns_==1?VerticalPresentationState::ChineseBase:VerticalPresentationState::ChineseExpanded;
  }
  VerticalImeGeometryState verticalImeGeometryState()const noexcept{
    return !composition_active_?VerticalImeGeometryState::Idle:
      candidate_columns_==1?VerticalImeGeometryState::ActiveBase:VerticalImeGeometryState::Expanded;
  }
  void compositionScheme(bool full_pinyin)noexcept{
    if(full_pinyin_==full_pinyin)return;
    full_pinyin_=full_pinyin;endComposition();
  }
  bool selectedAppeared()const noexcept{return selected_appeared_;}
  void observeComposition(bool full_pinyin,unsigned width,bool selected)noexcept{
    compositionScheme(full_pinyin);
    observeCompositionWidth(width);
    selected_appeared_=selected_appeared_||selected;
  }
  void observeCompositionWidth(unsigned width)noexcept{
    if(width&&width<=UINT16_MAX)composition_peak_=static_cast<std::uint16_t>(std::max(unsigned(composition_peak_),width));
  }
  void endComposition()noexcept{composition_active_=false;composition_peak_=ImeCompositionFontMetrics::minimum_width;selected_appeared_=false;candidate_columns_=1;}
  draftor::vertical::AbGeometry imeGeometry()const noexcept{
    draftor::vertical::AbGeometry g;g.area=draftor::vertical::editor_area;
    const bool active_vertical=!horizontalActive()&&chinese_context_;
    // 54 is the total Space02 A box, including its existing 2 px pads.
    g.composition_width=active_vertical?54:compositionWidth();g.candidate_columns=candidate_columns_;
    g.selected_visible=active_vertical||selected_appeared_;return g;
  }
  void candidateColumns(unsigned count)noexcept{if(count>=1&&count<=3)candidate_columns_=count;}
  // Toggle records intent only. The next prepare combines final geometry and
  // anchor before any pixels; coalesced F4 pairs do not quantize an interim view.
  void toggle() noexcept {
    if(!switch_pending_)source_mode_=mode_;
    switch_pending_=true;
    mode_=horizontalActive()?LayoutMode::Vertical:LayoutMode::Horizontal;
    ++revision_;prepared_=false;
  }
  bool prepare(bool incremental=false) noexcept {
    const auto now=context();
    const bool switching=switch_pending_&&source_mode_!=mode_;
    if(switching&&!custom_vertical_){
      // Save the departing presentation for its own later return only.
      // The destination never reads this source bookmark or its capacity.
      const auto source=source_mode_==LayoutMode::Horizontal?0:1;
      target_anchors_[source].observe(slots(source_mode_).first);
      saved_[source]=now;
    }
    syncFindSurface();
    bool preserve_vertical_anchor=false;
    if(!horizontalActive()&&!custom_vertical_){
      if(!same(vertical_anchor_context_,now))vertical_anchor_context_.valid=false;
      const auto g=imeGeometry();
      auto regions=draftor::vertical::reservedAbRegions(g);
      if(find_open_)regions.body={124,22,168,356};
      if(regions.status!=draftor::vertical::AbStatus::Ready)return false;
      const auto before=vertical_.region();
      const bool width_changed=before.x!=regions.body.x||before.width!=regions.body.width;
      if(chinese_context_&&!switching&&width_changed&&!vertical_anchor_context_.valid){
        const auto old=slots(mode_);
        if(old.capacity){vertical_anchor_slot_=old.slot;vertical_anchor_context_=now;}
      }
      preserve_vertical_anchor=chinese_context_&&vertical_anchor_context_.valid;
      if(vertical_.setRegion(regions.body,false,incremental?2048:0)!=draftor::vertical::Status::Ready)return false;
    }
    const auto& saved=saved_[horizontalActive()?0:1];
    if(!prepared_&&!same(saved,now))active().followCaret(shared_.editor.caretAffinity());
    if(!custom_vertical_){
      auto actual=slots(mode_);
      if(switching){
        const auto& anchor=target_anchors_[horizontalActive()?0:1];
        if(actual.capacity){
          const auto target=anchor.entrySlot(actual.first+actual.slot,actual.capacity,same(saved,now));
          if(horizontalActive())(void)horizontal_.navigator.placeCaretSlot(target);
          else (void)vertical_.placeCaretSlot(target);
        }
        actual=slots(mode_);
      }
      if(preserve_vertical_anchor&&actual.capacity){
        // Vertical-RL has a fixed right edge and pitch: preserving a column
        // slot preserves screen x/y. Clamp only to the feasible target range;
        // retain an infeasible request until expansion space is returned.
        const auto maximum=std::min<std::uint32_t>(actual.capacity-1,actual.first+actual.slot);
        const auto target=static_cast<std::uint16_t>(std::min<std::uint32_t>(vertical_anchor_slot_,maximum));
        if(!vertical_.placeCaretSlot(target))return false;
        actual=slots(mode_);
        if(actual.slot==vertical_anchor_slot_)vertical_anchor_context_.valid=false;
      }
      target_anchors_[horizontalActive()?0:1].observe(actual.first);
    }
    saved_[horizontalActive()?0:1]=now;
    prepared_=true;switch_pending_=false;return true;
  }
  void followCaret(hanime::renderer::CaretAffinity affinity,bool reset=true) noexcept override {
    if(!prepared_){prepare(!reset);}
    active().followCaret(affinity,reset);
  }
  bool captureViewport(editor::presentation::ViewportState& out) noexcept override {
    if(!prepared_){prepare();}
    return active().captureViewport(out);
  }
  void restoreViewport(const editor::presentation::ViewportState& state) noexcept override {
    if(!prepared_){prepare();}
    active().restoreViewport(state);
  }
  editor::presentation::SpatialResult spatialTarget(editor::presentation::SpatialDirection direction,bool extend,
      editor::presentation::ByteTarget& target,editor::presentation::NavigationContext context={}) noexcept override {
    if(!prepared_){prepare(true);}
    context.presentation_revision=revision_;
    return active().spatialTarget(direction,extend,target,context);
  }
  editor::presentation::SpatialResult serviceNavigation(editor::presentation::NavigationContext context,
      std::size_t budget,editor::presentation::ByteTarget& target,bool& extend) noexcept override {
    context.presentation_revision=revision_;
    return vertical_port_.serviceNavigation(context,budget,target,extend);
  }
  void cancelNavigation() noexcept override { vertical_port_.cancelNavigation(); }
  bool navigationPending() const noexcept override { return vertical_port_.navigationPending(); }
  void findSurface(bool open) noexcept override {
    find_open_=open;prepare();
    syncFindSurface();
  }
  VerticalPresentation& vertical() noexcept { return vertical_; }
 private:
  friend struct AppTestAccess; // Host-only inspection of the active delegate.
  struct Context {
    std::uint32_t revision=0,selection=0;
    std::size_t cursor=0;
    hanime::renderer::CaretAffinity affinity=hanime::renderer::CaretAffinity::Downstream;
    bool valid=false;
  };
  Context context() const noexcept {
    return {shared_.document.revision(),shared_.editor.selectionVersion(),
      shared_.document.cursorByte(),shared_.editor.caretAffinity(),true};
  }
  static bool same(const Context& a,const Context& b) noexcept {
    return a.valid&&a.revision==b.revision&&a.selection==b.selection&&
      a.cursor==b.cursor&&a.affinity==b.affinity;
  }
  struct Slots {std::uint32_t first=0;std::uint16_t slot=0,capacity=0;};
  Slots slots(LayoutMode mode)noexcept{
    if(mode==LayoutMode::Horizontal){
      const auto h=horizontal_.navigator.sync();
      return {h.first_visible_line,static_cast<std::uint16_t>(h.visual_line-h.first_visible_line),h.visible_lines};
    }
    if(vertical_.settle()!=draftor::vertical::Status::Ready)return {};
    const auto& view=vertical_.viewport();const auto c=vertical_.caret();
    return {view.firstColumn(),static_cast<std::uint16_t>(c.caret.column-view.firstColumn()),view.visibleColumns()};
  }
  editor::presentation::PresentationPort& active() noexcept {
    return horizontalActive()?static_cast<editor::presentation::PresentationPort&>(horizontal_.position):vertical_port_;
  }
  void syncFindSurface() noexcept {
    // Horizontal height also belongs to the IME surface. Do not reset that
    // geometry merely because an unchanged Find state is revisited after F4.
    if(horizontalActive()){
      if(horizontal_find_open_!=find_open_){horizontal_.setFindOpen(find_open_);horizontal_find_open_=find_open_;}
    }else vertical_port_.findSurface(find_open_);
  }
  editor::SharedEditorView shared_;
  HorizontalPresentation& horizontal_;
  VerticalPresentation& vertical_;
  editor::presentation::PresentationPort& vertical_port_;
  Context saved_[2]{};
  TargetViewportAnchor target_anchors_[2]{};
  Context vertical_anchor_context_{};
  std::uint16_t vertical_anchor_slot_=0;
  LayoutMode source_mode_=LayoutMode::Horizontal;
  std::uint16_t composition_peak_=ImeCompositionFontMetrics::minimum_width;
  std::uint8_t candidate_columns_=1;
  bool full_pinyin_=false,selected_appeared_=false,composition_active_=false;
  bool chinese_context_=false;
  LayoutMode mode_=LayoutMode::Horizontal; // sole runtime layout owner
  std::uint64_t revision_=1,presented_=0;
  bool prepared_=true,find_open_=false,horizontal_find_open_=false;
  bool custom_vertical_=false,switch_pending_=false;
};
} // namespace s3app
