/*
 This file is part of Livecut
 Copyright 2004 by Remy Muller.
 
 Livecut can be redistributed and/or modified under the terms of the
 GNU General Public License, as published by the Free Software Foundation;
 either version 2 of the License, or (at your option) any later version.
 
 Livecut is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with Livecut; if not, visit www.gnu.org/licenses or write to the
 Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 Boston, MA 02111-1307 USA
 */

#ifndef BBCUTTER_H
#define BBCUTTER_H

#include <valarray>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <numeric>

#include "BlockKinds.h"
#include "GateEnvelope.h"

//-------------------------------------------------------------------------------
struct Math
{
	static inline long  randominteger(long min, long max)
	{
    return long(0.5000001+randomfloat(min,max));
  }
	
	static inline double randomfloat(double min , double max)
	{
    return min + (max-min)*double(rand())/double(RAND_MAX);
  }
  
  static inline void randomseed(long seed)
  {
    srand(seed);
  }
  
  template<class T>
  static inline T wchoose(T *values,double *weights,long size)
  {
    std::vector<double> cumsum(size);
    cumsum[0] = *weights;
    for(int i=1;i<cumsum.size();++i)
      cumsum[i] = cumsum[i-1] + weights[i];
    
    double v = randomfloat(0.0,cumsum.back());
    long index;
    for(index=0;index<size;++index)
      if(v<cumsum[index])
        break;
    return values[index];
  }
	
  static inline float clip(const float x,const float mn,const float mx)
  {
    return std::min(std::max(x,mn),mx);
  }
};

static const float pi_4 = atan(1.f);

struct CutInfo
{
	long size;
	long length; //<= size
	float pan;
	float amp;
	float cents;
	long offset; // where in the capture buffer this cut starts reading (samples), 0 = block start
	long step;   // trance gate: step of the bar this cut plays (-1 for the other algorithms)
	float envattack, envdecay; // trance gate step envelope, fractions of the cut size (0 = none)
	float envtension;          // curve tension of that envelope (-1..1, 0 = straight)
	bool tiedin, tiedout;      // trance gate ties: no fade at the start / end, the slice continues from / into its neighbour
	// bit crusher
	float bits;
	float sr;
	CutInfo();
};

enum CutId
{
  kCutProc11=0,
  kWarpCut,
  kSQPusher,
  kEuclid,
  kRatchet,
  kGateCut,
  kNumCutProcs
};

/**
 @brief base class for cut procedures
 */
class CutProc
{
public:
	CutProc();
  virtual ~CutProc();
  
	void SetMinAmp(float v);
	void SetMaxAmp(float v);
	void SetMinPan(float v);
	void SetMaxPan(float v);
	void SetMinDetune(float v);
	void SetMaxDetune(float v);
	void SetDutyCycle(float v);
	void SetFillDutyCycle(float v);
  void SetMinPhraseLength(long v);
  void SetMaxPhraseLength(long v);
  // decision record of the last ChooseCuts() call
  int lastkind;
  int lastinfo[3];
  
  virtual long ChoosePhraseLength();
  virtual void ChooseCuts(std::vector<CutInfo> &cuts,
                          long &unitsinblock,
                          long unitsdone,
                          long totalunits,
                          long subdiv,
                          double spu);
  
protected:
  float minamp,maxamp,minpan,maxpan,mindetune,maxdetune,dutycycle,filldutycycle;
  long maxphraselength,minphraselength;
};

//-------------------------------------------------------------------------------

//-------------------------------------------------------------------------------
class CutProc11 : public CutProc
{
public:
	CutProc11();
  
	void SetStutterChance(float v);
	void SetStutterArea(float v);
	void SetMinRepeats(long v);
	void SetMaxRepeats(long v);
  
	void ChooseCuts(std::vector<CutInfo> &cuts,
                  long &unitsinblock,
                  long unitsdone,
                  long totalunits,
                  long subdiv,
                  double spu);

private:
  long ChooseRepeats();
	long ChooseUnitsInCut(long subdiv);
  
private:
	float stutterchance,stutterarea;
	long minrepeats, maxrepeats;
};

//-------------------------------------------------------------------------------
class WarpCutProc : public CutProc
{
public:
	WarpCutProc();
  
	void SetStraightChance(float chance);
	void SetRegularChance(float chance);
	void SetRitardChance(float chance);
	void SetAccel(float v);
  
	void ChooseCuts(std::vector<CutInfo> &cuts,
                  long &unitsinblock,
                  long unitsdone,
                  long totalunits,
                  long subdiv,
                  double spu);
  
	long ChooseRepeats(float beatsinblock);
	long ChooseBlockSize();
  
private:
	float straightchance, regularchance, ritardchance, accel;
};

//-------------------------------------------------------------------------------
class SQPusherCutProc : public CutProc
{
public:
  SQPusherCutProc();
  void SetActivity(float v);
  
  
  long ChoosePhraseLength();
	void ChooseCuts(std::vector<CutInfo> &cuts,
                  long &unitsinblock,
                  long unitsdone,
                  long totalunits,
                  long subdiv,
                  double spu);

private:
  void sqpusherinit();

private:
  typedef std::vector< std::vector< std::vector<double> > > CutSequence;
  double activity;
  bool fill;
  long fillnumber,fillpos;
  CutSequence cs;
};

//-------------------------------------------------------------------------------
/**
 @brief Euclidean cutter (LiveCut 2).
 Each block (half or whole bar) is split into k cuts whose sizes follow a
 Euclidean rhythm E(k,n), e.g. 3-3-2. Every cut replays the block start, so the
 groove comes from re-triggering the captured audio on the euclidean onsets.
*/
class EuclidCutProc : public CutProc
{
public:
  EuclidCutProc();
  void SetDensity(float v);
  void SetChance(float v);
  // which hit's audio the pattern retriggers: 0..3 = 1st..4th hit, 4 = random earlier hit
  void SetSlice(long v);
  void ChooseCuts(std::vector<CutInfo> &cuts,
                  long &unitsinblock,
                  long unitsdone,
                  long totalunits,
                  long subdiv,
                  double spu);
private:
  float density, chance;
  long slice;
};

//-------------------------------------------------------------------------------
/**
 @brief Ratchet builder (LiveCut 2).
 One block per beat. The number of retriggers per beat grows over the phrase
 (1, 2, 3, 4, 6, 8, 12, 16) like a build-up snare roll, with the pitch range
 used as a riser and the pan range used as a ping-pong.
*/
class RatchetCutProc : public CutProc
{
public:
  RatchetCutProc();
  void SetIntensity(float v);
  void SetCurve(float v);
  void ChooseCuts(std::vector<CutInfo> &cuts,
                  long &unitsinblock,
                  long unitsdone,
                  long totalunits,
                  long subdiv,
                  double spu);
private:
  float intensity, curve;
};

//-------------------------------------------------------------------------------
/**
 @brief Trance gate.
 One block per bar, one cut per unit (step). Nothing is rearranged: every step
 plays the audio of its own moment (offset = its position in the block), muted
 steps are silent. The step pattern is either random (density, re-rolled per
 step at each bar by variation, fresh at each phrase) or the drawn pattern.
*/
class GateCutProc : public CutProc
{
public:
  GateCutProc();
  void SetMode(long v) { mode = v; }            // 0 = random, 1 = drawn pattern
  void SetDensity(float v) { density = v; }
  void SetVariation(float v) { variation = v; }
  void SetPattern(unsigned long v) { pattern = v; }
  // packed attack / decay levels of steps 2*pair and 2*pair+1 (see LVC_GATEENV1)
  void SetEnvelope(long pair, unsigned long packed) { if(pair >= 0 && pair < 16) env[pair] = packed & 0xFFFFul; }
  float Attack(long step) const { return step >= 0 && step < 32 ? GateEnvFraction(int((env[step / 2] >> ((step % 2) * 8)) & 15ul)) : 0.f; }
  float Decay(long step) const { return step >= 0 && step < 32 ? GateEnvFraction(int((env[step / 2] >> ((step % 2) * 8 + 4)) & 15ul)) : 0.f; }
  // packed curve tensions of steps 2*pair and 2*pair+1 (signed 8 bits each, see LVC_GATETENSION1)
  void SetTensions(long pair, unsigned long packed) { if(pair >= 0 && pair < 16) tensions[pair] = packed & 0xFFFFul; }
  float Tension(long step) const
  {
    if(step < 0 || step >= 32) return 0.f;
    const int raw = int((tensions[step / 2] >> ((step % 2) * 8)) & 255ul);
    const int v = raw >= 128 ? raw - 256 : raw;
    return float(std::max(-127, std::min(127, v))) / 127.f;
  }
  void SetTies(unsigned long v) { ties = v; } // bit i: step i runs into step i + 1 (drawn pattern only)
  unsigned long GetMask() const { return mask; } // steps of the bar chosen last (bit 0 = step 1)
  void ChooseCuts(std::vector<CutInfo> &cuts,
                  long &unitsinblock,
                  long unitsdone,
                  long totalunits,
                  long subdiv,
                  double spu);
private:
  bool RollStep(long step, long subdiv);
  long mode;
  float density, variation;
  unsigned long pattern, mask, randmask;
  unsigned long env[16];
  unsigned long tensions[16];
  unsigned long ties;
};

//-------------------------------------------------------------------------------
/*
 the BBCutter has to notify about phrase, blocks and units
 the player has to notify about cuts
 */
class BBCutListener
{
public:
	BBCutListener() { }
	virtual ~BBCutListener() { }
	virtual void OnPhrase(long bar, long sd) { }
	virtual void OnBlock(long bar, long sd) { }
	virtual void OnUnit(long bar, long sd) { }
	virtual void OnCut(long cut, long numcuts) { }
};

class ListenerManager
{
	std::vector<BBCutListener*> listeners;
public:
	ListenerManager();
	
  void OnPhrase(long bar, long sd);
	void OnBlock(long bar, long sd);
  void OnUnit(long bar, long sd);
  void OnCut(long cut, long numcuts);
	
  void RegisterListener(BBCutListener *l);
};

static float trapeze(long i,long fade, long size)
{
  const long fadein  = (i<fade);
  const long fadeout = (i>=(size-fade));
  const float env = fadein                * (i)
  + (1-fadein)*(1-fadeout)* (fade);
  +  fadeout              * (fade-i);
  return float(env)/float(fade);
}

static float expenv(float i,float fade,float size)
{
  return (1-exp(-5*i/fade)) * (1-exp(5*(i-size)/fade));
}

class LivePlayer
{
public:
	LivePlayer();
	void SetListenerManager(ListenerManager *lm);
  void SetFade(float v);
  void OnBlock(std::vector<CutInfo> newcuts);

  // read-only accessors (used for visualisation)
  inline long GetCurrentCut() const { return currentcut; }
  inline long GetNumCuts() const { return long(cuts.size()); }
  inline long GetReadIndex() const { return readindex; }
  inline long GetInputIndex() const { return inputindex; }
  inline long GetBufferSize() const { return long(inputbufferL.size()); }
  inline const std::vector<CutInfo>& GetCuts() const { return cuts; }
  inline float GetLastGain() const { return lastgain; } // envelope x amp of the last output sample
  // trance gate: envelope of the last sample (0..1, gate shut .. fully open) and how deeply it shapes the level
  inline float GetGateEnv() const { return gateenv; }
  inline void SetGateDepth(float v) { gatedepth = std::max(0.f, std::min(1.f, v)); }
  // change the audible length of a cut that has not started yet (trance gate edits while the bar plays)
  inline void SetCutLength(long i, long len)
  {
    if(i > currentcut && i < long(cuts.size()))
      cuts[i].length = std::max(0L, std::min(len, cuts[i].size));
  }
  inline void SetCutEnvelope(long i, float attack, float decay, float tension)
  {
    if(i > currentcut && i < long(cuts.size()))
    {
      cuts[i].envattack = attack;
      cuts[i].envdecay = decay;
      cuts[i].envtension = tension;
    }
  }
  inline void SetCutTies(long i, bool in, bool out)
  {
    if(i > currentcut && i < long(cuts.size()))
    {
      cuts[i].tiedin = in;
      cuts[i].tiedout = out;
    }
  }
  inline long GetFade() const { return fade; }

  inline bool tick(float &outL, float &outR,const float inL,const float inR)
  {
    if(currentcut<cuts.size())
    {
      if(inputindex<inputbufferL.size()) //store input
      {
        inputbufferL[inputindex] = inL;
        inputbufferR[inputindex] = inR;
        inputindex++;
      }
      
      const CutInfo &cc = cuts[currentcut];
      const bool gatecut = cc.step >= 0;
      const bool dutyon = readindex < cc.length;
      // trance gate steps below full volume depth keep playing (quieter) where the gate is shut
      if(dutyon || (gatecut && gatedepth < 1.f))
      {
        float l, r;
        const long off = cuts[currentcut].offset;
        if(off > 0 && !curpitched)
        {
          // slice taken from later in the capture buffer (already recorded: off <= cut start)
          l = inputbufferL[off + readindex];
          r = inputbufferR[off + readindex];
        }
        else
        {
          l = (*L)[readindex];
          r = (*R)[readindex];
        }
        //rotation matrix
        float env = 0.f;
        if(dutyon)
        {
          if(!cc.tiedin && !cc.tiedout)
            env = expenv(readindex,fade,cc.length);
          else // tied gate steps: no fade where the slice continues into its neighbour
            env = (cc.tiedin  ? 1.f : float(1-exp(-5.0*readindex/fade)))
                * (cc.tiedout ? 1.f : float(1-exp(5.0*(readindex-cc.length)/fade)));
          if(cc.envattack > 0.f || cc.envdecay > 0.f) // trance gate step envelope
            env *= GateEnvelope(float(readindex) / float(std::max(1L, cc.size)), cc.envattack, cc.envdecay, cc.envtension);
        }
        float g = env;
        if(gatecut)
        {
          gateenv = env;                          // drives the gate filter too
          g = 1.f - gatedepth * (1.f - env);
        }
        lastgain = g * cc.amp;
        
        outL = g*(ll*l + rl*r);
        outR = g*(lr*l + rr*r);
        readindex++;
      }
      else                             //dutycycle off
      {
        readindex++;
        outL = 0.f;
        outR = 0.f;
        lastgain = 0.f;
        if(gatecut)
          gateenv = 0.f;
      }
      
      if(readindex>=cuts[currentcut].size) //end of cut
      {
        currentcut++;
        readindex = 0;
        if(currentcut>=cuts.size())
        {
          outL = 0.f;
          outR = 0.f;
          return false;
        }
				const float amp = cuts[currentcut].amp;
				const float pan = cuts[currentcut].pan;
        ll = amp * ((pan<0)? 1.f :  cos(pan*2*pi_4));
        lr = amp * ((pan<0)? 0.f :  sin(pan*2*pi_4));
        rl = amp * ((pan>0)? 0.f : -sin(pan*2*pi_4));
        rr = amp * ((pan>0)? 1.f :  cos(pan*2*pi_4));
        
        curpitched = fabs(cuts[currentcut].cents) > 1e-10;
        if(curpitched)
        {
          //resample (from the cut's slice offset; 0 for every algorithm except Euclid slices)
          const long src0 = cuts[currentcut].offset;
          const double ratio = pow(2.f,cuts[currentcut].cents/1200.f);
          const long sz = std::min(   long(pitchedbufferL.size()),
                                   (long(double(inputbufferL.size() - src0)/ratio)-1));
          for(int i=0;i<sz;++i)
          {
            const long pos = src0 + long(float(i)*ratio);
            const float frac = float(i)*ratio-float(pos);
            const float a = inputbufferL[pos]*(1-frac)+inputbufferL[pos+1]*frac;
            const float b = inputbufferR[pos]*(1-frac)+inputbufferR[pos+1]*frac;
            pitchedbufferL[i] = a;
            pitchedbufferR[i] = b;
          }
          L = &pitchedbufferL;
          R = &pitchedbufferR;
        }
        // tell cut-synchrone effects
        if(listenermanager)
          listenermanager->OnCut(currentcut,cuts.size()); // allow interpolation...
        
        return true;
      }
    }
    else
    {
      outL = 0.f;
      outR = 0.f;
      lastgain = 0.f;
    }
    
		return false;
  }
  
private:
	std::valarray<float> inputbufferL;
	std::valarray<float> inputbufferR;
	std::valarray<float> pitchedbufferL;
	std::valarray<float> pitchedbufferR;
	std::valarray<float> *L;
	std::valarray<float> *R;
  long currentcut;
  long inputindex,readindex;
  float ll,lr,rl,rr;
  long fade;
  float lastgain;
  float gateenv = 1.f, gatedepth = 1.f;
  bool curpitched;
  std::vector<CutInfo> cuts;
	ListenerManager *listenermanager;
};

//------------------------------------------------------------------------------------------------
inline float ms2samples(float t,float sr)
{
  return (t*0.001f)*sr;
}

class BBCutter
{
public:
	BBCutter(LivePlayer &player);
  ~BBCutter();
  
	void RegisterListener(BBCutListener *l);
	void	SetCutProc(long i);
	
  void	SetBarsInSample(long bars) ;
	void	SetTempo(double v) ;
	void	SetSubdiv(long v) ;
	void	SetStutterChance(float chance);
	void	SetStutterArea(float area);
  void	SetMaxRepeats(long repeats);
	void	SetMinRepeats(long repeats);
	void	SetStraightChance(float chance);
	void	SetRegularChance(float chance);
	void	SetRitardChance(float chance);
	void	SetAccel(float v);
  void	SetActivity(float v);
	void	SetEuclidDensity(float v);
	void	SetEuclidChance(float v);
	void	SetEuclidSlice(long v);
	// decision record of the active cut procedure for the last block
	inline int GetLastKind() const { return strategy->lastkind; }
	inline const int* GetLastInfo() const { return strategy->lastinfo; }
	void	SetRatchetIntensity(float v);
	void	SetRatchetCurve(float v);
	void	SetGateMode(long v)          { gate.SetMode(v); }
	void	SetGateDensity(float v)      { gate.SetDensity(v); }
	void	SetGateVariation(float v)    { gate.SetVariation(v); }
	void	SetGatePattern(unsigned long v) { gate.SetPattern(v); }
	void	SetGateEnvelope(long pair, unsigned long packed) { gate.SetEnvelope(pair, packed); }
	void	SetGateTensions(long pair, unsigned long packed) { gate.SetTensions(pair, packed); }
	void	SetGateTies(unsigned long v) { gate.SetTies(v); }
	inline unsigned long GetGateMask() const { return gate.GetMask(); }
	// forget the current phrase: a fresh phrase starts on the next bar line
	void	ResetPhrase() { totalunits = 0; unitsdone = 0; unitsinblock = 0; unitsinsideblock = 0; }
	// cap new phrases to this many bars (0 = no cap), used by fill mode
	void	SetPhraseLimit(long bars) { phraselimit = bars; }
	// minimum cut length in quarter notes (0 = no limit). Shorter cuts are merged.
	void	SetMinCutQuarters(double q) { mincutquarters = q; }
  void	SetFade(float v);
  void	SetMinPhraseLength(long v);
	void	SetMaxPhraseLength(long v);
	void	SetMinAmp(float v);
	void	SetMaxAmp(float v);
	void	SetMinPan(float v);
	void	SetMaxPan(float v);
	void	SetDutyCycle(float v);
	void	SetFillDutyCycle(float v);
	void	SetMinDetune(float v);
	void	SetMaxDetune(float v);
	void	SetNumerator(double v);
	void	SetDenominator(double v);
	
  void	SetSampleRate(double v);
	void	SetTimeInfos(double bpm,double num,double den,double srate);
	void	UpdateRates();
  
	inline long	  GetUnitPosition() { return slicestart+unitsinsideblock;}
	// read-only accessors (used for visualisation)
	inline long GetUnitsDone() const { return unitsdone; }
	inline long GetTotalUnits() const { return totalunits; }
	inline long GetUnitsInBlock() const { return unitsinblock; }
	inline long GetUnitsInsideBlock() const { return unitsinsideblock; }
	inline long GetSubdiv() const { return subdiv; }
	inline double UnitsPerBar(double subdiv,double numerator,double denominator) { return subdiv*numerator/denominator;}
	inline double BeatsPerSecond(double tempo) { return tempo/60.0;}
	inline double SamplesPerBeat(double sr,double tempo) { return sr/BeatsPerSecond(tempo);}
	inline double SamplesPerBar() { return SamplesPerBeat(sr,tempo)*beatsPerBar;}
	inline double SamplesPerUnit() { return SamplesPerBar()/double(subdiv);}
  
	void	Phrase(long bar, long sd);
  void	Block(long bar,long sd);
  void	Unit(long bar, long sd);
	void	SetPosition(long bar, long sd);
  
private:
	// params
	double	tempo, sr;
	long	subdiv;
	double	numerator,denominator,beatsPerBar;
  
	// states
	long	unitsdone, totalunits, barsnow,
  unitsinblock, unitsinsideblock,	barsinsample,slicestart;
  
	CutProc11 cutproc11;
	WarpCutProc warpcutproc;
	SQPusherCutProc sqpusher;
	EuclidCutProc euclid;
	RatchetCutProc ratchet;
	GateCutProc gate;
	double mincutquarters;
	long phraselimit;
	CutProc	*strategy;
  std::vector<CutProc *> procs;
	ListenerManager listenermanager;
	LivePlayer	&player;
};

#endif

