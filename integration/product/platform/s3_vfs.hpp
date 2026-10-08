#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
namespace s3fs {
inline constexpr const char* Read="r";
inline constexpr const char* Write="w";
inline constexpr const char* Append="a";
struct Faults {long write_bytes=-1;bool flush=false,close=false,rename=false,read=false;size_t max_read_bytes=0;bool directory=false;};
enum class PathKind { Missing, File, Directory, Error };
struct DirectoryEntry {std::string name;bool directory=false,regular=false;};
inline Faults faults; // Deterministic host injection; defaults disabled in firmware.
class File {
 struct Handle;
 std::shared_ptr<Handle> h_;
 enum class OpenKind { Any, Regular, Directory };
 int open_errno_=0;
 explicit File(const std::string&,const char* mode,OpenKind);
 friend class Fs;
 public:
#ifndef ARDUINO
 // Optional Host-only pathname observation: diagnostics, no file contents.
 inline static void(*openObserver)(const char*,const char*)=nullptr;
#endif
 File()=default;
 explicit operator bool()const;
 int openError()const{return open_errno_;}
 bool isDirectory()const;
 bool isFile()const;
 size_t size()const;
 int read(uint8_t*,size_t);
 size_t write(const uint8_t*,size_t);
 bool flush();
 bool close();
 const char* name()const;
 bool nextEntry(DirectoryEntry&);
 bool error()const;
};
// The only device pathname translation. SD.begin mounts the same FAT volume at /sd.
// POSIX fsync exposes errors that Arduino File::flush() discards.
class Fs {
 std::string root_;
 public:
 explicit Fs(std::string root):root_(std::move(root)){}
 File open(const char* path,const char* mode=Read)const;
 // Use known type at storage call sites to avoid preflight stat/path traversal.
 File openFile(const char* path,const char* mode=Read)const;
 File openDirectory(const char* path)const;
 bool exists(const char*)const;
 PathKind kind(const char*)const;
 bool mkdir(const char*)const;
 bool remove(const char*)const;
 bool rename(const char*,const char*)const;
};
}
