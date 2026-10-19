// SPDX-License-Identifier: Apache-2.0
#include "draftor_qr/qr.hpp"
#include "draftor_qr/render.hpp"
#include <array>
#include <iostream>
#include <stdexcept>
#include <vector>
namespace q=draftor::qr;
#define CHECK(x) do { if(!(x)) throw std::runtime_error("line " + std::to_string(__LINE__) + ": " #x); } while(0)
int main(){try{
 constexpr auto n=q::bufferBytes(9);
 static_assert(n==353 && q::bufferBytes(7)==255 && q::bufferBytes(40)==3918);
 std::array<std::uint8_t,n+2> a{},b{};a.front()=b.front()=0xa5;a.back()=b.back()=0x5a;
 const q::Buffer scratch{a.data()+1,n},matrix{b.data()+1,n};q::Matrix m{};
 std::vector<std::uint8_t> bytes(181,0);for(std::size_t i=0;i<bytes.size();++i)bytes[i]=std::uint8_t(i);
 q::Options o{9,9,q::Ecc::Medium};
 auto encode=[&](std::size_t length){return q::encode(bytes.data(),length,q::Mode::Bytes,o,scratch,matrix,m);};
 CHECK(encode(180)==q::Status::Ok && m.side==53 && m.valid());
 CHECK(encode(181)==q::Status::Capacity && !m.valid());
 CHECK(q::encode(nullptr,0,q::Mode::Bytes,o,scratch,matrix,m)==q::Status::Ok);
 CHECK(q::encode(nullptr,1,q::Mode::Bytes,o,scratch,matrix,m)==q::Status::InvalidArgument && !m.valid());
 CHECK(q::encode(bytes.data(),SIZE_MAX,q::Mode::Bytes,o,scratch,matrix,m)!=q::Status::Ok);
 for(const q::Options bad: {q::Options{0,9,q::Ecc::Medium},{10,9,q::Ecc::Medium},{1,41,q::Ecc::Low},{1,9,q::Ecc(4)},{1,9,q::Ecc::Medium,8}})
  CHECK(q::encode(bytes.data(),1,q::Mode::Bytes,bad,scratch,matrix,m)==q::Status::InvalidArgument && !m.valid());
 CHECK(q::encode(bytes.data(),1,q::Mode(2),o,scratch,matrix,m)==q::Status::InvalidArgument);
 CHECK(q::encode(bytes.data(),1,q::Mode::Bytes,o,{scratch.data,n-1},matrix,m)==q::Status::WorkspaceTooSmall);
 CHECK(q::encode(bytes.data(),1,q::Mode::Bytes,o,scratch,{nullptr,n},m)==q::Status::WorkspaceTooSmall);
 CHECK(q::encode(bytes.data(),1,q::Mode::Bytes,o,scratch,scratch,m)==q::Status::InvalidArgument);
 CHECK(q::encode(scratch.data,1,q::Mode::Bytes,o,scratch,matrix,m)==q::Status::InvalidArgument);
 CHECK(q::encode(matrix.data,1,q::Mode::Bytes,o,scratch,matrix,m)==q::Status::InvalidArgument);
 std::vector<std::uint8_t> alpha(263,'A');
 CHECK(q::encode(alpha.data(),262,q::Mode::Alphanumeric,o,scratch,matrix,m)==q::Status::Ok);
 CHECK(q::encode(alpha.data(),263,q::Mode::Alphanumeric,o,scratch,matrix,m)==q::Status::Capacity && !m.valid());
 for(auto c:{std::uint8_t(0),std::uint8_t('a'),std::uint8_t(255)}){
  alpha[0]=c;CHECK(q::encode(alpha.data(),1,q::Mode::Alphanumeric,o,scratch,matrix,m)==q::Status::InvalidCharacter);
 }
 CHECK(a.front()==0xa5 && a.back()==0x5a && b.front()==0xa5 && b.back()==0x5a);
 for(int v=1;v<=40;++v)for(int ecc=0;ecc<4;++ecc){
  const auto size=q::bufferBytes(v);std::vector<std::uint8_t> s(size),r(size);
  CHECK(q::encode(bytes.data(),1,q::Mode::Bytes,{v,v,q::Ecc(ecc)},{s.data(),s.size()},{r.data(),r.size()},m)==q::Status::Ok);
  CHECK(m.side==17+4*v && m.bytes==size && !m.module(-1,0) && !m.module(0,m.side));
  auto g=q::fit(m,{0,0,400,400});CHECK(g.scale>0 && g.pixels==(m.side+8)*g.scale);
 }
 CHECK(encode(10)==q::Status::Ok);
 unsigned calls=0;auto fill=[&](int x,int y,int w,int h,bool){++calls;CHECK(x>=8&&y>=66&&x+w<=292&&y+h<=306);};
 CHECK(q::paint(m,{8,66,284,240},fill) && calls>1);
 calls=0;CHECK(!q::paint(m,{0,0,20,20},fill)&&calls==0);
 CHECK(!q::fit(m,{0,0,300,400},3).scale && !q::fit(m,{INT_MAX,0,300,400}).scale);
 CHECK(!q::fit({}, {0,0,300,400}).scale);
 q::erase(a.data(),a.size());for(auto c:a)CHECK(c==0);
 std::cout<<"PASS bounds, binary NUL, exact capacity, invalid parameters, overlap, guards, all 40 versions/4 ECC, geometry\n";
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
