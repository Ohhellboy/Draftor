#pragma once
#include "check.hpp"
#include "src/hanime_document/storage_port.hpp"
#include <algorithm>
#include <map>
#include <set>
namespace hd = hanime::documents;

// Test medium only: no filesystem, TF, network or device calls. Real R2
// RecordStore/codec/cooperative writer perform all encoding and verification.
struct MemoryStorage final : hd::StoragePort {
  std::map<std::string,std::string> files;
  std::set<std::string> dirs{"/"};
  hd::MediumSession medium{true,1};
  enum class Fault { None, Write, ShortWrite, Flush, Read, Open };
  Fault fault = Fault::None;
  std::string fault_path;
  unsigned fault_hits = 0;
  std::string handle;
  hd::CooperativeOpenMode mode = hd::CooperativeOpenMode::Read;
  std::size_t at=0, io_calls=0, max_io=0, writes=0;
  hd::MediumSession mediumSession() const noexcept override { return medium; }
  bool hit(Fault f,std::string_view path) {
    if(fault!=f || path.find(fault_path)==std::string_view::npos)return false;
    ++fault_hits;return true;
  }
  static std::string pathKey(std::string_view p) {
    while(p.size()>1&&p.back()=='/')p.remove_suffix(1);
    return std::string(p);
  }
  void parents(std::string_view p) {
    for(std::size_t i=1;i<p.size();++i)if(p[i]=='/')dirs.insert(std::string(p.substr(0,i)));
  }
  hd::ReadResult read(std::string_view p,std::size_t max) override {
    if(!medium.available)return {hd::Status::MediaUnavailable,{}};
    if(hit(Fault::Read,p))return {hd::Status::IoError,{}};
    auto i=files.find(pathKey(p));if(i==files.end())return {hd::Status::NotFound,{}};
    if(i->second.size()>max)return {hd::Status::TooLarge,{}};
    return {hd::Status::Ok,draftor::memory::String(i->second.data(),i->second.size())};
  }
  hd::ReadResult readPrefix(std::string_view p,std::size_t max) override {
    if(!medium.available)return {hd::Status::MediaUnavailable,{}};
    if(hit(Fault::Read,p))return {hd::Status::IoError,{}};
    auto i=files.find(pathKey(p));if(i==files.end())return {hd::Status::NotFound,{}};
    return {hd::Status::Ok,draftor::memory::String(i->second.data(),std::min(max,i->second.size()))};
  }
  bool writesTruncate() const noexcept override { return true; }
  hd::Status write(std::string_view p,std::string_view bytes) override { return writeParts(p,&bytes,1); }
  hd::Status writeParts(std::string_view p,const std::string_view* parts,std::size_t count) override {
    if(!medium.available)return hd::Status::MediaUnavailable;
    if(hit(Fault::Write,p))return hd::Status::IoError;
    parents(p);auto& target=files[pathKey(p)];target.clear();
    for(std::size_t i=0;i<count;++i)target.append(parts[i]);
    ++writes;return hit(Fault::Flush,p)?hd::Status::IoError:hd::Status::Ok;
  }
  hd::Status verifyParts(std::string_view p,const std::string_view* parts,std::size_t count) override {
    auto i=files.find(pathKey(p));if(i==files.end())return hd::Status::NotFound;
    std::size_t offset=0;
    for(std::size_t n=0;n<count;++n){
      if(parts[n].size()>i->second.size()-offset ||
          std::string_view(i->second).substr(offset,parts[n].size())!=parts[n])return hd::Status::Corrupt;
      offset+=parts[n].size();
    }
    return offset==i->second.size()?hd::Status::Ok:hd::Status::Corrupt;
  }
  hd::Status remove(std::string_view p) override {
    if(!medium.available)return hd::Status::MediaUnavailable;
    return files.erase(pathKey(p))?hd::Status::Ok:hd::Status::NotFound;
  }
  hd::ListResult list(std::string_view prefix,std::string_view after,std::size_t max) override {
    hd::ListResult result;
    if(!medium.available){result.status=hd::Status::MediaUnavailable;return result;}
    for(const auto& pair:files)if(pair.first.compare(0,prefix.size(),prefix)==0&&pair.first>after){
      if(result.paths.size()==max){result.has_more=true;break;}
      result.paths.emplace_back(pair.first.data(),pair.first.size());
    }
    return result;
  }
  bool cooperativeSupported() const noexcept override { return true; }
  hd::CooperativeIoResult cooperativeExists(std::string_view p) override {
    ++io_calls;const auto key=pathKey(p);const bool dir=dirs.count(key);
    return {hd::Status::Ok,0,dir?1U:files.count(key)?2U:0U,dir||files.count(key)};
  }
  hd::CooperativeIoResult cooperativeMkdir(std::string_view p) override {
    ++io_calls;parents(p);dirs.insert(pathKey(p));return {hd::Status::Ok};
  }
  hd::CooperativeIoResult cooperativeRemove(std::string_view p) override { ++io_calls;return {remove(p)}; }
  hd::CooperativeIoResult cooperativeOpen(std::string_view p,hd::CooperativeOpenMode m) override {
    ++io_calls;CHECK(handle.empty());
    if(!medium.available)return {hd::Status::MediaUnavailable};
    if(hit(Fault::Open,p))return {hd::Status::IoError};
    const auto key=pathKey(p);
    if(m==hd::CooperativeOpenMode::Read&&!files.count(key)&&!dirs.count(key))return {hd::Status::NotFound};
    if(m==hd::CooperativeOpenMode::Write){parents(p);files[key].clear();}
    handle=key;mode=m;at=0;return {hd::Status::Ok};
  }
  hd::CooperativeIoResult cooperativeIsDirectory() override { ++io_calls;return {hd::Status::Ok,0,0,dirs.count(handle)!=0}; }
  hd::CooperativeIoResult cooperativeSize() override { ++io_calls;return {hd::Status::Ok,0,files.at(handle).size()}; }
  hd::CooperativeIoResult cooperativeRead(std::uint8_t* out,std::size_t n) override {
    ++io_calls;CHECK(!handle.empty()&&mode==hd::CooperativeOpenMode::Read);
    max_io=std::max(max_io,n);
    if(hit(Fault::Read,handle))return {hd::Status::IoError};
    auto& value=files.at(handle);const auto count=std::min(n,value.size()-at);
    if(count)std::memcpy(out,value.data()+at,count);at+=count;
    return {hd::Status::Ok,count};
  }
  hd::CooperativeIoResult cooperativeWrite(const std::uint8_t* in,std::size_t n) override {
    ++io_calls;CHECK(!handle.empty()&&mode==hd::CooperativeOpenMode::Write);
    max_io=std::max(max_io,n);++writes;
    if(hit(Fault::Write,handle))return {hd::Status::IoError};
    const auto count=hit(Fault::ShortWrite,handle)?n/2:n;
    files.at(handle).append(reinterpret_cast<const char*>(in),count);at+=count;
    return {hd::Status::Ok,count};
  }
  hd::CooperativeIoResult cooperativeFlush() override {
    ++io_calls;CHECK(!handle.empty());return {hit(Fault::Flush,handle)?hd::Status::IoError:hd::Status::Ok};
  }
  hd::CooperativeIoResult cooperativeClose() override { ++io_calls;handle.clear();at=0;return {hd::Status::Ok}; }
  ~MemoryStorage() override { CHECK(handle.empty()); }
};
