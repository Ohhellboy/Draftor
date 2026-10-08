#define main inherited_main
#include "host_tests.cpp"
#undef main
void menuCase(unsigned mode){
 Fixture f;f.start();std::vector<ds::DocumentId> trash;
 trash.push_back(f.app.session.currentId());CHECK(f.app.session.create("trash two",++f.now));trash.push_back(f.app.session.currentId());
 for(int i=0;i<18;++i)CHECK(f.app.session.create({},++f.now));
 for(auto id:trash)CHECK(f.app.session.moveToTrash(id,++f.now));
 auto frame=[&]{if(mode!=1&&s3profile::active){profileFrame(f);auto r=s3profile::finish();CHECK(r.total_us>=r.us[s3profile::Draw]);}};
 if(mode==0)s3profile::active=false;
 if(mode==1)s3profile::begin("already_active");
 if(mode==2){s3profile::begin("previous");(void)s3profile::finish();}
 auto key=[&](uint8_t usage){auto serial=f.app.operationSerial;f.key(usage);frame();return f.app.operationSerial-serial;};
 CHECK(key(59)==1);CHECK(key(79)==1);auto& state=f.app.manager.state();CHECK(state.view==ds::FileManagerView::Normal&&state.page.offset==16&&state.page.items.size()==2);
 auto target=state.page.items[0].id;for(auto id:trash)CHECK(id!=target);
 auto selected=state.selected;CHECK(key(79)==1&&f.app.lastOperation.status==ds::Status::LastPage);CHECK(state.selected==selected&&state.page.offset==16);
 CHECK(key(23)==1&&state.view==ds::FileManagerView::Trash&&state.page.items.size()==2);CHECK(key(23)==1&&state.view==ds::FileManagerView::Normal);
 CHECK(key(80)==1&&f.app.lastOperation.status==ds::Status::FirstPage&&state.page.offset==0);
 key(79);target=state.page.items[0].id;key(21);CHECK(f.app.renameOpen());f.key(41);frame();CHECK(metadata(f,target).area==ds::DocumentArea::Normal);
 CHECK(key(7)==1&&state.pending==ds::PendingAction::TrashSelected&&state.trash_id==target);key(41);CHECK(metadata(f,target).area==ds::DocumentArea::Normal);
 CHECK(key(40)==1&&f.app.session.currentId()==target);key(59);key(79);CHECK(key(17)==0&&f.app.commandBarrier());f.settle();frame();CHECK(!f.app.filesOpen());
 key(59);key(79);target=state.page.items.back().id;selectId(f,target);key(7);key(40);CHECK(metadata(f,target).area==ds::DocumentArea::Trash);key(80);CHECK(state.view==ds::FileManagerView::Normal&&state.page.offset==0);
 if(s3profile::active)(void)s3profile::finish();
}
int main(){try{
 for(unsigned mode=0;mode<3;++mode)menuCase(mode);
 for(unsigned count:{0U,1U}){Fixture f;f.start();if(!count)CHECK(f.app.session.moveToTrash(f.app.session.currentId(),++f.now));(void)s3profile::finish();f.key(59);(void)s3profile::finish();f.key(79);CHECK(f.app.lastOperation.status==ds::Status::LastPage&&f.app.manager.state().page.offset==0);f.key(80);CHECK(f.app.lastOperation.status==ds::Status::FirstPage);}
 std::puts("PASS menu lifecycle: inactive/active/finished; 0/1/18 normal + 2 trash; exact-once T/page/trash/new/open; stable boundaries");return 0;
}catch(const std::exception&e){std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}}
