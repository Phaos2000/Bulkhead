// Copyright (c) 2026 Phaos - phaos2000@pm.me
// Original Bulkhead portions are licensed under the Bulkhead Freeware and
// Source-Available License. See LICENSE and THIRD_PARTY_NOTICES.md in the project root.
#include "Engine.h"
#include <iostream>
#include <iomanip>
using namespace bulkhead;
int main(){
 NoDenormals guard;
 for(int mode=0;mode<2;++mode)for(int bits:{8,10,16})for(int kind=0;kind<3;++kind){
  auto v=defaults();v[Mix]=1.;v[Body]=0.;v[Truncate]=mode;v[Bits]=normalized(Bits,bits);Engine e;e.prepare(48000,v);
  double dry=0,wet=0,peak=0;unsigned rng=1234;int count=0;
  for(int n=0;n<48000*8;++n){
   rng=rng*1664525u+1013904223u;
   double x=kind==0?(double(int(rng>>8))-8388608)/8388608*.18:kind==1?.18*std::sin(2*pi*110*n/48000):((n%12000)<1200?.35*std::sin(2*pi*150*n/48000)*std::exp(-(n%12000)/190.):0.);
   double a,b;e.tick(x,x,a,b);
   if(n>48000*2){dry+=x*x;wet+=(a*a+b*b)*.5;peak=std::max({peak,std::abs(a),std::abs(b)});++count;}
  }
  std::cout<<"TRUNC "<<mode<<" / "<<bits<<" bit kind "<<kind<<" dry "<<std::sqrt(dry/count)<<" wet "<<std::sqrt(wet/count)<<" difference "<<10*std::log10(std::max(1e-30,wet/dry))<<" dB peak "<<peak<<"\n";
 }
}
