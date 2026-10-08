#pragma once
struct mutex_t { bool locked=false; };
inline bool mutex_try_enter(mutex_t* m,void*) { if(m->locked)return false;m->locked=true;return true; }
inline void mutex_exit(mutex_t* m){m->locked=false;}
