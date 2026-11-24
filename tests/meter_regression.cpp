#include "DistrhoPluginInternal.hpp"
#include "DistrhoVST3TimePosition.hpp"
#include "BBCutter.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>
using namespace DISTRHO;
static int failures = 0;
static void check(bool ok, const char* text) { if(!ok) { std::fprintf(stderr,"FAIL: %s\n",text); ++failures; } }
static bool near(double a,double b) { return std::fabs(a-b)<1.e-7; }
static v3_process_context context(int n,int d,double q,double start) {
 v3_process_context c={}; c.state=V3_PROCESS_CTX_PROJECT_TIME_VALID|V3_PROCESS_CTX_TIME_SIG_VALID|V3_PROCESS_CTX_BAR_POSITION_VALID|V3_PROCESS_CTX_TEMPO_VALID|V3_PROCESS_CTX_PLAYING;
 c.time_sig_numerator=n;c.time_sig_denom=d;c.project_time_quarters=q;c.bar_position_quarters=start;c.bpm=190.;return c;
}
static void timingTests() {
 TimePosition::BarBeatTick b;
 fillVST3BarBeatTick(context(19,16,4.75,4.75),b);check(b.valid&&b.bar==2&&b.beat==1&&near(b.tick,0.),"19/16 bar boundary is 4.75 quarters");
 fillVST3BarBeatTick(context(19,16,4.875,4.75),b);check(b.beat==1&&near(b.tick,960.),"19/16 half of a sixteenth-note beat");
 fillVST3BarBeatTick(context(1,16,.25,.25),b);check(b.valid&&b.bar==2&&b.beat==1,"sub-quarter bar has no integer-division-by-zero");
 fillVST3BarBeatTick(context(17,32,2.125,2.125),b);check(b.valid&&b.bar==2&&b.beat==1,"17/32 fractional bar");
 fillVST3BarBeatTick(context(7,8,4.25,4.),b);check(b.beat==1&&near(b.tick,960.)&&near(b.barStartTick,15360.),"use host bar start following a meter change");
 fillVST3BarBeatTick(context(19,16,-.125,-4.75),b);check(b.valid&&b.bar==0&&b.beat==19&&near(b.tick,960.),"negative PPQ count-in");
 auto c=context(19,16,4.75,0.);c.state&=~V3_PROCESS_CTX_BAR_POSITION_VALID;fillVST3BarBeatTick(c,b);check(b.bar==2&&b.beat==1,"fallback without host bar-start field");
 c=context(19,0,0.,0.);fillVST3BarBeatTick(c,b);check(!b.valid,"reject invalid denominator");
 c=context(19,16,std::numeric_limits<double>::quiet_NaN(),0.);fillVST3BarBeatTick(c,b);check(!b.valid,"reject non-finite PPQ");
 c=context(4,4,0.,0.);c.state=0;fillVST3BarBeatTick(c,b);check(!b.valid,"reject missing timing flags");
 LivePlayer player;BBCutter cutter(player);cutter.SetTimeInfos(190.,19.,16.,48000.);check(near(cutter.SamplesPerBar(),48000.*60./190.*4.75),"capture duration follows 19/16");
 cutter.SetTimeInfos(190.,7.,8.,48000.);check(near(cutter.SamplesPerBar(),48000.*60./190.*3.5),"capture duration follows meter changes");
}
static void set(PluginExporter& p,const char* symbol,float value) {
 for(uint32_t i=0;i<p.getParameterCount();++i)if(p.getParameterSymbol(i)==symbol){p.setParameterValue(i,value);return;}
 check(false,"parameter symbol exists");
}
struct Meter { int n,d; };
static double audioTest(const std::vector<Meter>& meters,int subdiv,bool loop,double sr=8000.,int frames=128) {
 d_nextBufferSize=frames;d_nextSampleRate=sr;PluginExporter p(nullptr,nullptr,nullptr,nullptr);set(p,"livecut_cutproc",.5f);set(p,"livecut_subdiv",float(subdiv));
 const double bpm=190.;std::vector<double> starts(1,0.);double longestBar=0.;
 for(const auto& m:meters){double len=m.n*4./m.d;starts.push_back(starts.back()+len*8.);longestBar=std::max(longestBar,len*60./bpm);}
 const double duration=starts.back()*60./bpm;
 std::vector<float> a(frames),b(frames),l(frames),r(frames);const float* in[]={a.data(),b.data()};float* out[]={l.data(),r.data()};long zero=0,longest=0;bool finite=true;
 for(long f=0;f<long(duration*sr);f+=frames){
  double q=f/sr*bpm/60.;size_t seg=0;while(seg+1<meters.size()&&q>=starts[seg+1])++seg;
  const Meter m=meters[seg];const double barLength=m.n*4./m.d;double local=q-starts[seg];if(loop)local=std::fmod(local,barLength*4.);
  const double pos=starts[seg]+local;const double barStart=starts[seg]+std::floor(local/barLength)*barLength;
  auto c=context(m.n,m.d,pos,barStart);TimePosition t;t.clear();t.playing=true;t.frame=f;t.bbt.beatsPerMinute=bpm;fillVST3BarBeatTick(c,t.bbt);p.setTimePosition(t);
  for(int i=0;i<frames;++i)a[i]=b[i]=.2f*std::sin(float(f+i)*.11f);
  p.run(in,out,frames);
  for(int i=0;i<frames;++i){finite=finite&&std::isfinite(l[i])&&std::isfinite(r[i]);if(std::fabs(l[i])+std::fabs(r[i])<1.e-8)++zero;else zero=0;longest=std::max(longest,zero);}
 }
 check(finite,"audio has no NaN/Inf");double longestSilence=longest/sr;
 // A newly loaded slicer waits for a bar boundary; allow that startup plus two host buffers.
 if(longestSilence>longestBar+2.*frames/sr+.02){std::fprintf(stderr,"FAIL: audio %d/%d subdiv-index=%d loop=%d longest silence=%.5fs bar=%.5fs\n",meters[0].n,meters[0].d,subdiv,loop,longestSilence,longestBar);++failures;}
 return longestSilence;
}
int main(){timingTests();const Meter meters[]={{4,4},{3,4},{5,4},{6,8},{7,8},{5,8},{19,16},{1,16},{17,32}};int cases=0;
 for(const auto& m:meters)for(int div=0;div<7;++div)for(bool loop:{false,true}){audioTest({m},div,loop);++cases;}
 std::vector<Meter> mixed={{4,4},{19,16},{7,8},{5,16},{17,32},{3,4},{4,4}};for(int div=0;div<7;++div){audioTest(mixed,div,false);++cases;}
 // User-like sample rate and host buffer size, with repeated 19/16 bars.
 audioTest({{19,16}},1,true,48000.,256);++cases;
 std::printf("%s: timing checks + %d audio cases (EUCLID, 9 meters, 7 subdivisions, loops, changing meter).\n",failures?"FAILED":"PASS",cases);
 return failures?1:0;}
