// Select the ten relevant inherited groups without altering their assertions.
// The complete old port_risks suite has an unrelated obsolete Find height=306
// expectation; Alpha 2.1 editor_geometry fixes that height at 320.
#include "app_test_access.hpp"
#include <cstdlib>
#include <unistd.h>
#include <cstring>
inline char* adapter_mkdtemp(char* p){if(std::strncmp(p,"/tmp/hanime-s3-port-tests-",25)==0)std::memmove(p,p+5,std::strlen(p+5)+1);return ::mkdtemp(p);}
#define mkdtemp adapter_mkdtemp
#define main inheritedLegacyMain
#include "../../integration/product/dependencies/s3/tests/host_tests.cpp"
#undef main
int main(){try{
 hid();files();editing();saves();selectionGeometry();commandBarrier();longRun();openRegression();transitionInput();accessPaths();
 std::puts("PASS 10 unchanged inherited groups: HID, original VFS, editing, cooperative save, selection, command barrier, long document, open/restore integrity, focus epochs, direct file access");return 0;
}catch(const std::exception& e){std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}}
