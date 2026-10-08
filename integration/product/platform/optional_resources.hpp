#pragma once
#include <cstddef>
#include <cstdint>
#include <string_view>
namespace s3host {
// Host transport categories only: no new package format, version or identity standard.
enum class ResourceRole:uint8_t{CharacterData,Lexicon,SchemeIndex,Font};
enum class ResourceStatus:uint8_t{NotConfigured,Missing,Incompatible,IoError,Ready};
class RandomAccessSource {
 public:virtual ~RandomAccessSource()=default;
 virtual std::size_t size()const=0;
 virtual bool read(std::size_t offset,void* destination,std::size_t bytes)const=0;
};
// Locator resolution and TF access belong here, never in Document or input handling.
// A future bridge implements the independent core's ResourceReader using this port.
// Package identity/compatibility must be checked by the independent core before activation.
class ResourceAccess {
 public:virtual ~ResourceAccess()=default;
 virtual ResourceStatus open(ResourceRole,std::string_view locator,RandomAccessSource*& borrowed)=0;
 virtual void close(RandomAccessSource*)=0;
};
class NoOptionalResources final:public ResourceAccess {
 public:ResourceStatus open(ResourceRole,std::string_view,RandomAccessSource*& p)override{p=nullptr;return ResourceStatus::NotConfigured;}
 void close(RandomAccessSource*)override{}
};
}
