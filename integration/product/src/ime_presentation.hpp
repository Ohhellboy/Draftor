#pragma once
#include "device_ime.hpp"
namespace ime_device {
// One predicate for layout, draw and the foreground scheduling exception.
// A hidden candidate generation must remain unpublished, including generation
// zero after cancel. A visible empty generation still needs presentation.
inline bool candidateSurfaceVisible(const DeviceIme* ime,const s3app::App* app){
 return app&&ime&&ime->chinese()&&(!app->filesOpen()||app->renameOpen())&&!app->statsOpen();
}
inline bool candidatePublicationPending(const DeviceIme* ime,const s3app::App* app){
 return candidateSurfaceVisible(ime,app)&&ime->window()&&
        ime->view()->generation!=ime->window()->onScreenGeneration();
}
// A complete nonempty view is already usable. Yield before starting another
// synchronous resource slice for a pending, not-yet-visible navigation target.
inline bool candidateWorkShouldYield(const DeviceIme* ime,const s3app::App* app){
 return candidatePublicationPending(ime,app)&&ime->view()->count!=0;
}
// Only call after successful LCD transfer or exact already-visible pixels.
inline uint32_t publishVisibleCandidate(DeviceIme* ime,const s3app::App* app){
 return candidateSurfaceVisible(ime,app)&&!app->findOpen()?ime->framePresented():0;
}
}
