#pragma once
#include "draftor/vertical_ab.hpp"
namespace ime_device { class DeviceIme; }
namespace s3app {
class App;
// Paint operands only: no candidate identity, generation, selection ordinal,
// revision, number or focus style. A preview can never be submitted as a choice.
struct CandidateVisualText {
 std::array<char,65> text{};
 std::uint16_t bytes=0;
 draftor::vertical::Rect box{};
 std::uint16_t limit=0;
 draftor::vertical::TextFlow flow=draftor::vertical::TextFlow::VerticalBody;
};
struct CandidateVisualList {
 std::array<CandidateVisualText,18> items{};
 std::size_t count=0;
 bool overflow=false;
 void clear() noexcept { count=0;overflow=false; }
};
struct CandidateVisualContinuity {
 enum class State:std::uint8_t { Empty, Ready, Pending };
 CandidateVisualList text{};
 State state=State::Empty;
 bool valid=false,vertical=false;
 std::uint64_t surface=0;
 std::uint32_t owner=0;
 draftor::vertical::Rect binding{}; // Candidate surface only; never drives layout.
 void reset() noexcept { text.clear();state=State::Empty;valid=false; }
 bool pending() const noexcept { return state==State::Pending&&valid&&text.count; }
 void capture(const draftor::vertical::AbRun&) noexcept;
 void captureHorizontal(std::string_view,int x,int y,unsigned width) noexcept;
 void complete(bool accepted) noexcept {
  if(!accepted){reset();return;}
  if(state==State::Ready)valid=!text.overflow&&text.count;
 }
};
void prepareCandidateVisual(App&,ime_device::DeviceIme*) noexcept;
void paintPendingCandidateVisual(const CandidateVisualContinuity&,const draftor::vertical::AbPort&) noexcept;
} // namespace s3app
