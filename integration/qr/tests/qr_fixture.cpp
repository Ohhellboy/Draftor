// SPDX-License-Identifier: Apache-2.0
// Host bridge: links only draftor_qr, never Wi-Fi, App, TF or ESP-IDF.
#include "draftor_qr/qr.hpp"
#include "draftor_qr/render.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <vector>
namespace q=draftor::qr;
int main(int argc,char** argv){try{
 if(argc!=8)throw std::runtime_error("input output min-version max-version ECC[0..3] bytes|alpha mask[-1..7]");
 const q::Options o{std::stoi(argv[3]),std::stoi(argv[4]),q::Ecc(std::stoi(argv[5])),std::stoi(argv[7])};
 std::ifstream input(argv[1],std::ios::binary);if(!input)throw std::runtime_error("input missing");
 std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(input)),{});
 const auto n=q::bufferBytes(o.max_version);std::vector<std::uint8_t> s(n),r(n);q::Matrix m;
 auto begin=std::chrono::steady_clock::now();
 const auto status=q::encode(bytes.data(),bytes.size(),std::string(argv[6])=="alpha"?q::Mode::Alphanumeric:q::Mode::Bytes,o,{s.data(),n},{r.data(),n},m);
 auto us=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-begin).count();
 if(status!=q::Status::Ok)throw std::runtime_error(q::statusText(status));
 const int width=(m.side+8)*4;std::vector<unsigned char> pixels(width*width,255);
 q::paint(m,{0,0,width,width},[&](int x,int y,int w,int h,bool ink){for(int dy=0;dy<h;++dy)for(int dx=0;dx<w;++dx)pixels[(y+dy)*width+x+dx]=ink?0:255;});
 std::ofstream output(argv[2],std::ios::binary);output<<"P5\n"<<width<<' '<<width<<"\n255\n";output.write(reinterpret_cast<const char*>(pixels.data()),pixels.size());
 if(!output)throw std::runtime_error("output failed");
 std::cout<<"{\"side\":"<<m.side<<",\"workspace_each\":"<<n<<",\"host_encode_us\":"<<us<<",\"bits\":\"";
 for(int y=0;y<m.side;++y)for(int x=0;x<m.side;++x)std::cout<<(m.module(x,y)?'1':'0');
 std::cout<<"\"}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
