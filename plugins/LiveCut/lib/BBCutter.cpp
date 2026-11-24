/*
 This file is part of Livecut
 Copyright 2003 by Remy Muller.
 
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

#include "BBCutter.h"

//-------------------------------------------------------------------------------
CutInfo::CutInfo()
: size(0)
, length(0)
, pan(0.f)
, amp(1.f)
, cents(0.f)
, offset(0)
, step(-1)
, envattack(0.f)
, envdecay(0.f)
, envtension(0.f)
, tiedin(false)
, tiedout(false)
, fxCut(-1)
{
}

CutProc::CutProc()
: minamp(1.f)
, maxamp(1.f)
, minpan(0.5f)
, maxpan(0.5f)
, mindetune(0.f)
, maxdetune(0.f)
, dutycycle(1.f)
, filldutycycle(1.f)
, maxphraselength(4)
, minphraselength(1)
, lastkind(0)
{
  lastinfo[0] = lastinfo[1] = lastinfo[2] = 0;
}

CutProc::~CutProc() {}

void CutProc::SetMinAmp(float v) { minamp = v;}
void CutProc::SetMaxAmp(float v) { maxamp = v;}
void CutProc::SetMinPan(float v) { minpan = v;}
void CutProc::SetMaxPan(float v) { maxpan = v;}
void CutProc::SetMinDetune(float v) { mindetune = v;}
void CutProc::SetMaxDetune(float v) { maxdetune = v;}
void CutProc::SetDutyCycle(float v) { dutycycle = v;}
void CutProc::SetFillDutyCycle(float v) { filldutycycle = v;}
void CutProc::SetMinPhraseLength(long v) { minphraselength = v;}
void CutProc::SetMaxPhraseLength(long v) { maxphraselength = v;}

long CutProc::ChoosePhraseLength()
{
  return Math::randominteger(minphraselength,maxphraselength);
}

void CutProc::ChooseCuts(std::vector<CutInfo> &cuts,
                         long &unitsinblock,
                         long unitsdone,
                         long totalunits,
                         long subdiv,
                         double spu)
{
}

//-------------------------------------------------------------------------------
long CutProc11::ChooseRepeats()
{
  return Math::randominteger(minrepeats,maxrepeats);
}

long CutProc11::ChooseUnitsInCut(long subdiv)
{	// units = 2*x+1 , x is natural // 1 <= 2*x + 1 <= subdiv/2+1
  return 2*long(Math::randomfloat(0.f, float(subdiv)/4.f)+0.5f) + 1;
}

CutProc11::CutProc11()
: stutterchance(1.f)
, stutterarea(0.5f)
, minrepeats(1)
, maxrepeats(2)
{
}

void CutProc11::SetStutterChance(float v) { stutterchance = v;}
void CutProc11::SetStutterArea(float v) { stutterarea = v;}
void CutProc11::SetMinRepeats(long v) { minrepeats = v+1;} //not repeats actually but occurences
void CutProc11::SetMaxRepeats(long v) { maxrepeats = v+1;}

void CutProc11::ChooseCuts(std::vector<CutInfo> &cuts,
                           long &unitsinblock,
                           long unitsdone,
                           long totalunits,
                           long subdiv,
                           double spu)
{
  const long unitsleft = totalunits-unitsdone;
  long unitsincut = 1;
  long repeats = 1;
  
  if( float(unitsleft)/float(subdiv) < stutterarea &&
     Math::randomfloat(0.0, 1.0) < stutterchance) //end of phrase stutter
  {
    long numbers[] = { 1, 2, 3, 4, 6, 8 };
    double probs[]  = { 0.4, 0.3, 0.1, 0.1, 0.05, 0.05 };
    long multiplier = Math::wchoose(numbers,probs,6);
    repeats=unitsleft*multiplier;
    double unitsinthiscut= 1.0/multiplier;
    unitsinblock = repeats*unitsinthiscut;
    lastkind = kBlockStutter;
    lastinfo[0] = multiplier;
    lastinfo[1] = repeats;
    lastinfo[2] = 0;
    cuts.resize(repeats);
    const float startpan    = Math::randomfloat(minpan,maxpan);
    const float endpan      = Math::randomfloat(minpan,maxpan);
    const float startamp    = Math::randomfloat(minamp,maxamp);
    const float endamp      = Math::randomfloat(minamp,maxamp);
    const float startdetune = 0.f;
    const float enddetune   = Math::randomfloat(mindetune,maxdetune);
    
    for(int i=0;i<repeats;i++)
    {
      const float phase = float(i)/float(repeats);
      const long cutlength = long(unitsinthiscut*spu);
      cuts[i].size = cutlength;
      cuts[i].length = long(float(cutlength)*filldutycycle);
      cuts[i].pan = startpan + (endpan-startpan)*phase;
      cuts[i].amp = startamp + (endamp-startamp)*phase;
      cuts[i].cents = startdetune + (enddetune-startdetune)*phase;
    }
  }
  else
  {
    unitsincut	= ChooseUnitsInCut(subdiv);
    repeats = ChooseRepeats();
    
    while(unitsincut>unitsleft) {unitsincut -= 2;}
    if(unitsincut<0)			 {unitsincut = 0;}
    
    while((repeats*unitsincut)>unitsleft)
    {
      if(repeats>1)
      {
        repeats--;
      }
      else if(unitsleft<=(subdiv/2+1))
      {
        unitsincut=unitsleft;
        repeats=1;
      }
      else
      {
        unitsincut=1; //shouldn't happen
        repeats=1;
      }
    }
    unitsinblock = repeats*unitsincut;
    lastkind = kBlockNormal;
    lastinfo[0] = unitsincut;
    lastinfo[1] = repeats;
    lastinfo[2] = 0;
    cuts.resize(repeats);
    for(int i=0;i<repeats;i++)
    {
      cuts[i].size = long(unitsincut*spu);
      //quantize cut dutycycle to match cuts to units
      //cuts[i].length = long(double(std::max(long(dutycycle*unitsincut),1L))*spu);
      cuts[i].length = long(dutycycle*unitsincut*spu);
      cuts[i].amp = Math::randomfloat(minamp,maxamp);
    }
  }
}

//-------------------------------------------------------------------------------
long WarpCutProc::ChooseRepeats(float beatsinblock)
{
  long repeatsarray[] = {4,8,16,32};
  
  if(beatsinblock<1.f)
    return repeatsarray[Math::randominteger(0,2)];
  else
    return repeatsarray[Math::randominteger(1,3)];
}

long WarpCutProc::ChooseBlockSize()
{
  long blockarray[] = {1,2,4};
  double blockprobs[]  = {0.5,0.4,0.1};
  return Math::wchoose(blockarray,blockprobs,3);
}

WarpCutProc::WarpCutProc()
: straightchance(0.5)
, regularchance(0.7)
, ritardchance(0.6)
, accel(0.9)
{
}

void WarpCutProc::SetStraightChance(float chance) { straightchance = chance;}
void WarpCutProc::SetRegularChance(float chance) { regularchance = chance;}
void WarpCutProc::SetRitardChance(float chance) { ritardchance = chance;}
void WarpCutProc::SetAccel(float v) { accel = v;}

void WarpCutProc::ChooseCuts(std::vector<CutInfo> &cuts,
                             long &unitsinblock,
                             long unitsdone,
                             long totalunits,
                             long subdiv,
                             double spu)
{
  const long unitsleft = totalunits-unitsdone;
  long repeats = 1;
  unitsinblock = ChooseBlockSize();
  if(unitsinblock>unitsleft)
    unitsinblock = unitsleft;
  
  if(Math::randomfloat(0.0, 1.0)< straightchance)
  {
    lastkind = kBlockStraight;
    lastinfo[0] = unitsinblock;
    lastinfo[1] = lastinfo[2] = 0;
    double temp = double(unitsinblock)/double(repeats);
    cuts.resize(repeats);
    for(int i=0;i<repeats;i++)
    {
      long l = long(spu*temp);
      cuts[i].size = l;
      //quantize cut dutycycle to match cuts to units
      cuts[i].length = long(double(std::max(long(dutycycle*temp),1L))*spu);
      cuts[i].amp = Math::randomfloat(minamp,maxamp);
    }
  }
  else
  {
    repeats = ChooseRepeats(float(unitsinblock)/float(subdiv));
    const float startpan    = Math::randomfloat(minpan,maxpan);
    const float endpan      = Math::randomfloat(minpan,maxpan);
    const float startamp    = Math::randomfloat(minamp,maxamp);
    const float endamp      = Math::randomfloat(minamp,maxamp);
    const float startdetune = 0.f; //Math::randomfloat(mindetune,maxdetune);
    const float enddetune   = Math::randomfloat(mindetune,maxdetune);
    
    if(Math::randomfloat(0.0, 1.0)< regularchance)
    {
      //long repeatsarray[] = {4,6,8,12,16,18,24,32};
      //repeats = repeatsarray[Math::randominteger(0,7)];
      lastkind = kBlockRoll;
      lastinfo[0] = repeats;
      lastinfo[1] = unitsinblock;
      lastinfo[2] = 0;
      double temp = double(unitsinblock)/double(repeats);
      cuts.resize(repeats);
      for(int i=0;i<repeats;i++)
      {
        const float phase = float(i)/float(repeats);
        long l = long(spu*temp+0.5);
        cuts[i].size = l;
        cuts[i].length = long(float(l)*filldutycycle);
        cuts[i].pan = startpan + (endpan-startpan)*phase;
        cuts[i].amp = startamp + (endamp-startamp)*phase;
        cuts[i].cents = startdetune + (enddetune-startdetune)*phase;
      }
    }
    else //accel
    {
      double temp = unitsinblock*(1.0-double(accel))/(1.0-pow(double(accel),double(repeats)));
      cuts.resize(repeats);
      for(int i=0;i<repeats;i++)
      {
        const float phase = float(i)/float(repeats);
        long l = long(spu*temp*(pow(double(accel),double(i))));
        cuts[i].size = l;
        cuts[i].length = long(float(l)*filldutycycle);
        cuts[i].pan = startpan + (endpan-startpan)*phase;
        cuts[i].amp = startamp + (endamp-startamp)*phase;
        cuts[i].cents = startdetune + (enddetune-startdetune)*phase;
      }
      lastkind = kBlockAccel;
      lastinfo[0] = repeats;
      lastinfo[1] = unitsinblock;
      lastinfo[2] = 0;
      if(Math::randomfloat(0.0, 1.0)< ritardchance)
      {
        std::reverse(cuts.begin(),cuts.end());
        lastkind = kBlockRitard;
      }
    }
  }
}

//-------------------------------------------------------------------------------
SQPusherCutProc::SQPusherCutProc()
: activity(0.1)
, fill(false)
, fillnumber(0)
, fillpos(0)
{
  sqpusherinit();
}

void SQPusherCutProc::SetActivity(float v)
{
  activity=v;
}

void SQPusherCutProc::sqpusherinit()
{
  //boring manual initialisation
  cs.resize(13);
  
  cs[0].resize(2);
  cs[0][0].push_back(0.75);cs[0][0].push_back(0.75);cs[0][0].push_back(0.75);cs[0][0].push_back(0.75);
  cs[0][1].push_back(1.0);
  
  cs[1].resize(3);
  cs[1][0].push_back(0.5);cs[1][0].push_back(1.0);
  cs[1][1].push_back(1.0);
  cs[1][2].push_back(1.0);cs[1][2].push_back(0.5);
  
  cs[2].resize(3);
  cs[2][0].push_back(0.5);
  cs[2][1].push_back(1.0); cs[2][1].push_back(1.0); cs[2][1].push_back(1.0);
  cs[2][2].push_back(0.5);
  
  cs[3].resize(5);
  cs[3][0].push_back(0.571429 );
  cs[3][1].push_back(0.571429 );  cs[3][1].push_back(0.571429 );
  cs[3][2].push_back(0.571429 );  cs[3][2].push_back(0.571429 );
  cs[3][3].push_back(0.571429 );
  cs[3][4].push_back(0.285714 );  cs[3][4].push_back(0.285716 );
  
  cs[4].resize(3);
  cs[4][0].push_back(1.0); cs[4][0].push_back(0.5);
  cs[4][1].push_back(1.0); cs[4][1].push_back(0.5);
  cs[4][2].push_back(0.5); cs[4][2].push_back(0.5);
  
  cs[5].resize(3);
  cs[5][0].push_back(0.5);cs[5][0].push_back(0.5);
  cs[5][1].push_back(0.66);cs[5][1].push_back(0.67); cs[5][1].push_back(0.67);
  cs[5][2].push_back(1.0);
  
  cs[6].resize(4);
  cs[6][0].push_back(0.34);
  cs[6][1].push_back(0.33);  cs[6][1].push_back(0.33);
  cs[6][2].push_back(0.34);  cs[6][2].push_back(0.33);
  cs[6][1].push_back(2.33);
  
  cs[7].resize(4);
  cs[7][0].push_back(1.4);
  cs[7][1].push_back(0.4);cs[7][1].push_back(0.4);
  cs[7][2].push_back(0.6);cs[7][2].push_back(0.2);
  cs[7][3].push_back(1.0);
  
  cs[8].resize(3);
  cs[8][0].push_back(0.167);cs[8][0].push_back(0.167);cs[8][0].push_back(0.166);
  cs[8][1].push_back(1.0);cs[8][0].push_back(1.0);cs[8][0].push_back(1.0);
  cs[8][0].push_back(0.5);
  
  cs[9].resize(2);
  cs[9][0].push_back(1.5);  cs[9][0].push_back(0.5);  cs[9][0].push_back(1.0);
  cs[9][1].push_back(0.25); cs[9][1].push_back(0.25); cs[9][1].push_back(0.25); cs[9][1].push_back(0.25);
  
  cs[10].resize(4);
  cs[10][0].push_back(0.2);cs[10][0].push_back(0.2);
  cs[10][1].push_back(0.4);cs[10][1].push_back(0.4);
  cs[10][2].push_back(0.4);cs[10][2].push_back(0.4);
  cs[10][3].push_back(2.0);
  
  cs[11].resize(2);
  cs[11][0].push_back(0.75);cs[11][0].push_back(0.75);cs[11][0].push_back(1.0);
  cs[11][1].push_back(0.25);cs[11][1].push_back(0.25);cs[11][1].push_back(0.25);cs[11][1].push_back(0.25);cs[11][1].push_back(0.25);cs[11][1].push_back(0.25);
  
  cs[12].resize(5);
  cs[12][0].push_back(0.5);cs[12][0].push_back(1.0);
  cs[12][1].push_back(0.5);
  cs[12][2].push_back(0.125);cs[12][2].push_back(0.125);cs[12][2].push_back(0.125);cs[12][2].push_back(0.125);
  cs[12][3].push_back(1.0);
  cs[12][4].push_back(0.167); cs[12][4].push_back(0.167); cs[12][4].push_back(0.166);
}

long SQPusherCutProc::ChoosePhraseLength()
{
  fill = false;
  return CutProc::ChoosePhraseLength();
}

void SQPusherCutProc::ChooseCuts(std::vector<CutInfo> &cuts,
                                 long &unitsinblock,
                                 long unitsdone,
                                 long totalunits,
                                 long subdiv,
                                 double spu)
{
  const long unitsleft = subdiv - (unitsdone%subdiv); //one bar at a time
  double done = double(unitsdone)/double(subdiv);
  long phrase  = long(done);                   //{0,1,2,3} bars
  long barpos  = long(done*4.0) % 4;           //{0,1,2,3} beats
  long beatpos = long(done*16.0)%4;           //position inside beat {0,1,2,3} semiquaver
  long quaver = long(done*8.0) % 8;	         //quaver inside bar
  //quaver = (long(done*8.0)*2) % 8;
  double barprop = double(barpos)/4.0;
  double sqweights[]= {0.0, 0.3, 0.0, 0.5, 0.7, 0.8, 0.9, 0.6};
  double sqchance = sqweights[quaver]*activity;
  double spb = spu*double(subdiv)/4.0;        //samplesperbeat
  
  if((totalunits-unitsdone) == subdiv)
  {
    fill = true;
    fillnumber = Math::randominteger(0,12);
    fillpos=0;
  }
  
  if(fill==true)
  {
    if(fillpos<cs[fillnumber].size())
    {
      lastkind = kBlockFill;
      lastinfo[0] = fillnumber;
      lastinfo[1] = fillpos;
      lastinfo[2] = long(cs[fillnumber].size());
      cuts.resize(cs[fillnumber][fillpos].size());
      double beatsdone = 0.0;
      for(int i=0;i<cuts.size();i++)
      {
        const float phase = float(i)/float(cuts.size());
        beatsdone += cs[fillnumber][fillpos][i];
        long l = long(spb * cs[fillnumber][fillpos][i]);
        cuts[i].size = l;
        cuts[i].length = long(spb * cs[fillnumber][fillpos][i] * filldutycycle);
        cuts[i].pan = Math::randomfloat(minpan,maxpan);
        cuts[i].amp = Math::randomfloat(minamp,maxamp);
        cuts[i].cents = Math::randomfloat(mindetune,maxdetune);
      }
      unitsdone = long(double(subdiv)*beatsdone/4.0);
      if(unitsinblock>unitsleft)
        unitsinblock=unitsleft; //will automatically interrupt cutsequence before its end
      fillpos++;
      return;
    }
    else
    {
      fill = false;
      fillpos=0;
    }
  }
  
  {
    long temp = 1 + 2*Math::randominteger(0,1);
    if(beatpos == 2)
      temp=1;
    
    unitsinblock = long(double(temp*8.0)/double(subdiv));
    if(unitsinblock>unitsleft)
      unitsinblock=unitsleft; //will automatically interrupt cutsequence before its end
    
    if(Math::randomfloat(0.0,1.0) < sqchance) // 2*temp semiquaver
    {
      lastkind = kBlockSemiquavers;
      lastinfo[0] = temp*2;
      lastinfo[1] = lastinfo[2] = 0;
      cuts.resize(temp*2);
      for(int i=0;i<cuts.size();i++)
      {
        long l = long(0.25*spb);
        cuts[i].size = l;
        cuts[i].pan = Math::randomfloat(minpan,maxpan);
        cuts[i].amp = Math::randomfloat(minamp,maxamp);
        cuts[i].cents = Math::randomfloat(mindetune,maxdetune);
        cuts[i].length = long(0.25*spb*dutycycle);
      }
    }
    else            // or temp quaver i.e same duration
    {
      lastkind = kBlockQuavers;
      lastinfo[0] = temp;
      lastinfo[1] = lastinfo[2] = 0;
      cuts.resize(temp);
      for(int i=0;i<cuts.size();i++)
      {
        long l = long(0.5*spb);
        cuts[i].size = l;
        cuts[i].amp = Math::randomfloat(minamp,maxamp);
        cuts[i].cents = Math::randomfloat(mindetune,maxdetune);
        cuts[i].length = long(0.5*spb*dutycycle);
      }
    }
  }
}

//-------------------------------------------------------------------------------
// Euclidean cutter
//-------------------------------------------------------------------------------
EuclidCutProc::EuclidCutProc()
: density(0.45f)
, chance(0.8f)
, slice(0)
{
}

void EuclidCutProc::SetSlice(long v) { slice = v; }

void EuclidCutProc::SetDensity(float v) { density = v; }
void EuclidCutProc::SetChance(float v) { chance = v; }

void EuclidCutProc::ChooseCuts(std::vector<CutInfo> &cuts,
                               long &unitsinblock,
                               long unitsdone,
                               long totalunits,
                               long subdiv,
                               double spu)
{
  const long unitsleft = totalunits - unitsdone;
  const long toBarEnd  = subdiv - (unitsdone % subdiv);

  // half-bar or whole-bar blocks, never crossing a bar line or the phrase end
  long n = subdiv;
  if(subdiv >= 12 && Math::randomfloat(0.0, 1.0) < 0.5)
    n = subdiv / 2;
  n = std::min(n, std::min(toBarEnd, unitsleft));
  if(n < 1) n = 1;
  unitsinblock = n;

  const bool fillzone = unitsleft <= subdiv; // last bar of the phrase

  if(n < 3 || Math::randomfloat(0.0, 1.0) > chance)
  {
    // play the block straight
    cuts.resize(1);
    cuts[0].size   = long(n * spu);
    cuts[0].length = long(n * spu * dutycycle);
    cuts[0].amp    = Math::randomfloat(minamp, maxamp);
    lastkind = kBlockStraight;
    lastinfo[0] = lastinfo[1] = lastinfo[2] = 0;
    return;
  }

  // number of onsets
  const float d = fillzone ? std::min(1.f, density + 0.3f) : density;
  long k = long(2.0 + double(d) * double(n - 2) + 0.5);
  k = std::max(2L, std::min(n, k));

  // Bresenham-style euclidean onsets, then gaps between them
  std::vector<long> gaps;
  gaps.reserve(k);
  long last = 0;
  for(long i = 1; i <= n; ++i)
  {
    const bool onset = (i == n) || (((i * k) % n) < k);
    if(onset)
    {
      gaps.push_back(i - last);
      last = i;
    }
  }

  // random rotation keeps it from always starting with the long gap
  const long rot = Math::randominteger(0, long(gaps.size()) - 1);
  std::rotate(gaps.begin(), gaps.begin() + (rot % long(gaps.size())), gaps.end());

  const float duty = fillzone ? filldutycycle : dutycycle;
  cuts.resize(gaps.size());
  long onset = 0; // position of this hit in the block, in units
  std::vector<long> onsets(gaps.size());
  for(size_t i = 0; i < gaps.size(); ++i)
  {
    onsets[i] = onset;
    onset += gaps[i];

    // the slice this hit replays: an earlier (already recorded) hit's position
    long src = (slice >= 4) ? Math::randominteger(0, long(i)) : std::min<long>(slice, long(i));
    src = std::max(0L, std::min<long>(src, long(i)));
    cuts[i].offset = long(onsets[src] * spu);

    cuts[i].size   = long(gaps[i] * spu);
    cuts[i].length = long(gaps[i] * spu * duty);
    cuts[i].amp    = (i == 0) ? maxamp : Math::randomfloat(minamp, maxamp); // accent the downbeat
    cuts[i].pan    = Math::randomfloat(minpan, maxpan);
    cuts[i].cents  = (i == 0) ? 0.f : Math::randomfloat(mindetune, maxdetune);
  }
  lastkind = kBlockEuclid;
  lastinfo[0] = k;
  lastinfo[1] = n;
  lastinfo[2] = rot % long(gaps.size());
}

//-------------------------------------------------------------------------------
// Ratchet builder
//-------------------------------------------------------------------------------
RatchetCutProc::RatchetCutProc()
: intensity(0.6f)
, curve(0.5f)
{
}

void RatchetCutProc::SetIntensity(float v) { intensity = v; }
void RatchetCutProc::SetCurve(float v) { curve = v; }

void RatchetCutProc::ChooseCuts(std::vector<CutInfo> &cuts,
                                long &unitsinblock,
                                long unitsdone,
                                long totalunits,
                                long subdiv,
                                double spu)
{
  static const long options[8] = { 1, 2, 3, 4, 6, 8, 12, 16 };

  const long unitsleft = totalunits - unitsdone;
  long n = std::max(1L, subdiv / 4); // one beat in 4/4
  n = std::min(n, std::max(1L, unitsleft));
  unitsinblock = n;

  // phrase progress shaped by curve: 0 = builds late, 1 = builds early
  const double progress = totalunits > 0 ? double(unitsdone) / double(totalunits) : 0.0;
  const double exponent = 3.0 + (0.35 - 3.0) * double(curve);
  const double shaped   = std::pow(std::max(0.0, std::min(1.0, progress)), exponent);
  const double level    = double(intensity) * shaped;

  long top = long(level * 7.0 + 0.5);
  long idx;
  if(unitsleft <= n) // final beat of the phrase: full roll
    idx = std::max(long(intensity * 7.0 + 0.5), intensity > 0.f ? 1L : 0L);
  else
    idx = Math::randominteger(std::max(0L, top - 2), top);
  idx = std::max(0L, std::min(7L, idx));
  const long reps = options[idx];

  const double cutunits = double(n) / double(reps);
  const float  riser    = mindetune + (maxdetune - mindetune) * float(shaped);
  const float  amp      = minamp + (maxamp - minamp) * float(0.5 + 0.5 * shaped);
  const float  duty     = reps > 1 ? filldutycycle : dutycycle;

  lastkind = kBlockRatchet;
  lastinfo[0] = reps;
  lastinfo[1] = idx;
  lastinfo[2] = 0;
  cuts.resize(reps);
  for(long i = 0; i < reps; ++i)
  {
    const long l = long(cutunits * spu + 0.5);
    cuts[i].size   = l;
    cuts[i].length = long(float(l) * duty);
    cuts[i].amp    = amp;
    cuts[i].pan    = (reps > 1) ? ((i % 2 == 0) ? minpan : maxpan) : 0.5f * (minpan + maxpan);
    cuts[i].cents  = riser;
  }
}

//-------------------------------------------------------------------------------
// Trance gate
//-------------------------------------------------------------------------------
GateCutProc::GateCutProc()
: mode(0)
, density(0.6f)
, variation(0.25f)
, pattern(0x55555555ul)
, mask(0)
, randmask(0)
{
  for(int i = 0; i < 16; ++i) env[i] = 0;
  for(int i = 0; i < 16; ++i) tensions[i] = 0;
  ties = 0;
}

// random state of one step: on-beat steps are more likely to be open, so sparse patterns still groove
bool GateCutProc::RollStep(long step, long subdiv)
{
  const bool onbeat = subdiv >= 4 && (step * 4) % subdiv == 0;
  const double p = onbeat ? density + (1.0 - density) * 0.4 : density * 0.85;
  return Math::randomfloat(0.0, 1.0) < p;
}

void GateCutProc::ChooseCuts(std::vector<CutInfo> &cuts,
                             long &unitsinblock,
                             long unitsdone,
                             long totalunits,
                             long subdiv,
                             double spu)
{
  const long unitsleft = totalunits - unitsdone;
  const long pos = unitsdone % subdiv;  // step at which this block starts
  long n = std::min(subdiv - pos, unitsleft); // up to the bar line
  if(n < 1) n = 1;
  unitsinblock = n;
  const long steps = std::min(subdiv, 32L);

  if(mode == 1)
  {
    mask = pattern;
  }
  else
  {
    if(unitsdone == 0) // new phrase: a fresh pattern
    {
      randmask = 0;
      for(long st = 0; st < steps; ++st)
        if(RollStep(st, subdiv))
          randmask |= 1ul << st;
    }
    else if(pos == 0 && variation > 0.f) // new bar: re-roll some steps
    {
      for(long st = 0; st < steps; ++st)
        if(Math::randomfloat(0.0, 1.0) < variation)
        {
          if(RollStep(st, subdiv)) randmask |= 1ul << st;
          else randmask &= ~(1ul << st);
        }
    }
    if(density > 0.f && (randmask & ((steps >= 32) ? 0xFFFFFFFFul : ((1ul << steps) - 1ul))) == 0)
      randmask |= 1ul; // never a fully silent bar unless density is zero
    mask = randmask;
  }

  cuts.resize(n);
  long open = 0;
  // ties (drawn pattern only): an open step tied to the next open step plays on into it as one slice
  auto isOn = [&](long st) { return st >= 0 && st < steps && ((mask >> st) & 1ul); };
  auto tiedOut = [&](long st) { return mode == 1 && st + 1 < steps && isOn(st) && isOn(st + 1) && ((ties >> st) & 1ul); };
  for(long i = 0; i < n; ++i)
  {
    const long st = pos + i;
    const long a = long(double(i) * spu);
    const long b = long(double(i + 1) * spu); // sizes add up to the block exactly
    const bool on = isOn(st);
    const bool tin = tiedOut(st - 1), tout = tiedOut(st);
    cuts[i].size   = b - a;
    cuts[i].offset = a;            // play this step's own audio: nothing is rearranged
    cuts[i].length = on ? (tout ? (b - a) : long(double(b - a) * dutycycle)) : 0; // the gate gap only ends a slice
    cuts[i].tiedin = tin;
    cuts[i].tiedout = tout;
    // a tied run keeps one level
    cuts[i].amp    = (tin && i > 0) ? cuts[i - 1].amp : Math::randomfloat(minamp, maxamp);
    cuts[i].pan    = 0.f;
    cuts[i].cents  = 0.f;          // a pitched step would need audio that has not arrived yet
    cuts[i].step   = st;
    cuts[i].envattack = Attack(st);
    cuts[i].envdecay  = Decay(st);
    cuts[i].envtension = Tension(st);
    open += on ? 1 : 0;
  }
  lastkind = kBlockGate;
  lastinfo[0] = open;
  lastinfo[1] = n;
  lastinfo[2] = mode;
}

//-------------------------------------------------------------------------------
// Merges cuts shorter than minsize into their neighbours. Total block length is
// preserved, so the rhythm stays on the grid, the repeats just get slower.
//-------------------------------------------------------------------------------
static void EnforceMinCut(std::vector<CutInfo> &cuts, long minsize)
{
  if(minsize <= 0 || cuts.size() < 2)
    return;

  const long threshold = long(minsize * 0.95);
  bool anyshort = false;
  for(size_t i = 0; i < cuts.size(); ++i)
    if(cuts[i].size < threshold) { anyshort = true; break; }
  if(!anyshort)
    return;

  std::vector<CutInfo> out;
  out.reserve(cuts.size());
  CutInfo acc;
  double accduty = 1.0;
  bool open = false;

  for(size_t i = 0; i < cuts.size(); ++i)
  {
    if(!open)
    {
      acc = cuts[i];
      accduty = acc.size > 0 ? double(acc.length) / double(acc.size) : 1.0;
      open = true;
    }
    else
    {
      acc.size += cuts[i].size;
    }
    if(acc.size >= threshold)
    {
      acc.length = long(double(acc.size) * accduty);
      out.push_back(acc);
      open = false;
    }
  }

  if(open)
  {
    if(out.empty())
    {
      acc.length = long(double(acc.size) * accduty);
      out.push_back(acc);
    }
    else
    {
      CutInfo &back = out.back();
      const double duty = back.size > 0 ? double(back.length) / double(back.size) : 1.0;
      back.size += acc.size;
      back.length = long(double(back.size) * duty);
    }
  }

  cuts.swap(out);
}

//-------------------------------------------------------------------------------
ListenerManager::ListenerManager()	{}

void ListenerManager::OnPhrase(long bar, long sd)
{
  for(int i=0;i<listeners.size();++i) {
    listeners[i]->OnPhrase(bar,sd);
  }
}

void ListenerManager::OnBlock(long bar, long sd)
{
  for(int i=0;i<listeners.size();++i)
  {
    listeners[i]->OnBlock(bar,sd);
  }
}

void ListenerManager::OnUnit(long bar, long sd)
{
  for(int i=0;i<listeners.size();++i)
  {
    listeners[i]->OnUnit(bar,sd);
  }
}

void ListenerManager::OnCut(long cut, long numcuts)
{
  for(int i=0;i<listeners.size();++i)
  {
    listeners[i]->OnCut(cut,numcuts);
  }
}

void ListenerManager::RegisterListener(BBCutListener *l)
{
  if(l)
    listeners.push_back(l);
}

//------------------------------------------------------------------------
LivePlayer::LivePlayer()
: currentcut(0)
, inputindex(0)
, readindex(0)
, ll(0.f)
, lr(0.f)
, rl(0.f)
, rr(0.f)
, fade(0)
, lastgain(0.f)
, curpitched(false)
, L(&inputbufferL)
, R(&inputbufferR)
, listenermanager(NULL)
{
}

void LivePlayer::SetListenerManager(ListenerManager *lm)
{
  listenermanager = lm;
}

void LivePlayer::SetFade(float v)
{
  if(v<1.f) v = 1.f ; //0.1f;
  fade = v;
}

void LivePlayer::PrimeCut(long i)
{
  if(i<0 || i>=long(cuts.size()))
    return;

  // rotation matrix [ll lr; rl rr]
  const float pan = cuts[i].pan;
  const float amp = cuts[i].amp;
  ll = amp * ((pan<0)? 1.f :  cos(pan*2*pi_4));
  lr = amp * ((pan<0)? 0.f :  sin(pan*2*pi_4));
  rl = amp * ((pan>0)? 0.f : -sin(pan*2*pi_4));
  rr = amp * ((pan>0)? 1.f :  cos(pan*2*pi_4));

  curpitched = fabs(cuts[i].cents) > 1e-10;
  if(curpitched)
  {
    // resample from the cut's slice offset (0 for every algorithm except Euclid slices)
    const long src0 = cuts[i].offset;
    const double ratio = pow(2.f, cuts[i].cents/1200.f);
    const long sz = std::min( long(pitchedbufferL.size()),
                              (long(double(inputbufferL.size() - src0)/ratio)-1));
    for(long k=0;k<sz;++k)
    {
      const long pos = src0 + long(float(k)*ratio);
      const float frac = float(k)*ratio - float(pos);
      const float a = inputbufferL[pos]*(1-frac)+inputbufferL[pos+1]*frac;
      const float b = inputbufferR[pos]*(1-frac)+inputbufferR[pos+1]*frac;
      pitchedbufferL[k] = a;
      pitchedbufferR[k] = b;
    }
    L = &pitchedbufferL;
    R = &pitchedbufferR;
  }
  else
  {
    L = &inputbufferL;
    R = &inputbufferR;
  }

  // tell cut-synchrone effects. During freeze the expanded list is replayed, so pass the original
  // cut index / count: every FX rate, filter, delay and send repeats exactly as on the first pass.
  if(listenermanager)
  {
    const long fxCut = cuts[i].fxCut >= 0 ? cuts[i].fxCut : i;
    const long fxNum = freezeFxNumCuts > 0 ? freezeFxNumCuts : long(cuts.size());
    listenermanager->OnCut(fxCut, fxNum);
  }
}

void LivePlayer::SetFreeze(bool v)
{
  freezeRequested = v;
  if(!v)
  {
    if(freezeActive && !savedCuts.empty())
    {
      // Restore the live capture and the cut list that was playing when freeze was pressed.
      // The next OnBlock replaces them again on the host grid, but until then the slicer must
      // not keep replaying the frozen pattern.
      inputbufferL = savedInputL;
      inputbufferR = savedInputR;
      pitchedbufferL = savedPitchedL;
      pitchedbufferR = savedPitchedR;
      cuts = savedCuts;
      currentcut = savedCurrentCut;
      readindex = savedReadIndex;
      inputindex = savedInputIndex;
      curpitched = savedCurPitched;
      L = curpitched ? &pitchedbufferL : &inputbufferL;
      R = curpitched ? &pitchedbufferR : &inputbufferR;
      PrimeCut(std::min(currentcut, long(cuts.size()) - 1));
      savedCuts.clear();
    }
    freezeActive = false;
    freezeFxNumCuts = -1;
    return;
  }
  if(cuts.empty())
    return;

  // pressed after the block already finished: restart it from the top and prime the first cut.
  // If the buffer just filled mid-cut, the current cut is already primed; don't retrigger it.
  const bool pastEnd = currentcut>=long(cuts.size());
  if(pastEnd)
  {
    currentcut = 0;
    readindex = 0;
  }
  if(inputindex>=long(inputbufferL.size()))
  {
    freezeActive = true;
    if(pastEnd)
      PrimeCut(currentcut);
  }
}

void LivePlayer::BuildFreezeCuts(long loopLen)
{
  freezeCuts.clear();
  if(cuts.empty() || loopLen <= 0)
    return;

  long patternLen = 0;
  for(const CutInfo& c : cuts)
    patternLen = std::max(patternLen, c.offset + c.size);
  if(patternLen <= 0)
    return;

  // Repeat the exact same chop pattern across the two-bar snapshot. Every repeat starts a new
  // cut list entry, so PrimeCut re-fires the per-cut filter / delay / crusher / reverb settings
  // on every pass instead of leaving them stuck at one value.
  for(long base = 0; base < loopLen; base += patternLen)
  {
    for(size_t cutIndex = 0; cutIndex < cuts.size(); ++cutIndex)
    {
      const CutInfo& src = cuts[cutIndex];
      const long start = src.offset + base;
      if(start >= loopLen)
        break;

      CutInfo c = src;
      c.offset = start;
      c.fxCut = static_cast<long>(cutIndex); // replay the original FX value, not an expanded-list one
      long maxSize = loopLen - start;
      if(std::fabs(c.cents) > 1e-10)
      {
        const double ratio = std::pow(2.0, double(c.cents) / 1200.0);
        maxSize = std::min(maxSize, std::min(loopLen, long(double(loopLen - start) / ratio) - 1));
      }
      c.size = std::min(c.size, maxSize);
      c.length = std::min(c.length, std::max(0L, c.size));
      if(c.size <= 0)
        continue;
      freezeCuts.push_back(c);
    }
  }
}

void LivePlayer::SetFreezeLoop(const float* l, const float* r, long size)
{
  if(size <= 0 || l == nullptr || r == nullptr)
  {
    SetFreeze(true); // nothing captured yet: hold the current block until it is full
    return;
  }

  // Keep the live state for a seamless release.
  savedInputL = inputbufferL;
  savedInputR = inputbufferR;
  savedPitchedL = pitchedbufferL;
  savedPitchedR = pitchedbufferR;
  savedCuts = cuts;
  // A block that finished just before freeze has currentcut == cuts.size(); restart it from 0
  // on release instead of restoring an out-of-range index that would play silence.
  savedCurrentCut = currentcut >= long(cuts.size()) ? 0 : currentcut;
  savedReadIndex = savedCurrentCut == 0 ? 0 : readindex;
  savedInputIndex = inputindex;
  savedCurPitched = curpitched;
  freezeFxNumCuts = static_cast<long>(cuts.size()); // FX automation follows the original pattern

  inputbufferL.resize(size);
  inputbufferR.resize(size);
  pitchedbufferL.resize(size);
  pitchedbufferR.resize(size);
  for(long i = 0; i < size; ++i)
  {
    inputbufferL[i] = l[i];
    inputbufferR[i] = r[i];
  }

  BuildFreezeCuts(size);
  if(!freezeCuts.empty())
    cuts = freezeCuts;

  freezeRequested = true;
  freezeActive = true;
  inputindex = size; // snapshot is complete: the recording branch must stay off
  currentcut = 0;
  readindex = 0;
  curpitched = false;
  L = &inputbufferL;
  R = &inputbufferR;
  freezeLoopCount = 0;
  lastgain = 1.f;
  gateenv = 1.f;
  PrimeCut(0); // cut-synchrone effects follow the first slice of the loop
}

void LivePlayer::OnBlock(std::vector<CutInfo> newcuts)
{
  if(!newcuts.empty())
  {
    cuts = newcuts;
    inputindex = readindex = 0;
    currentcut = 0;

    long maxcutlength=0;
    for(int i=0;i<cuts.size();i++)
      if(cuts[i].offset+cuts[i].size>maxcutlength)
        maxcutlength = cuts[i].offset+cuts[i].size;
    curpitched = false;

    inputbufferL.resize(maxcutlength,0.f);
    inputbufferR.resize(maxcutlength,0.f);
    pitchedbufferL.resize(maxcutlength,0.f);
    pitchedbufferR.resize(maxcutlength,0.f);
    L = &inputbufferL;
    R = &inputbufferR;

    // prime the first cut (also tells cut-synchrone effects to interpolate)
    PrimeCut(0);
  }
}

//------------------------------------------------------------------------------------------------
#include <algorithm>
#include <functional>
#include <cassert>
using std::for_each;
using std::mem_fun;
using std::bind2nd;

BBCutter::BBCutter(LivePlayer &player)
: player(player)
, tempo(180)
, sr(44100)
, subdiv(8)
, numerator(4)
, denominator(4)
, beatsPerBar(4.0*numerator/double(denominator))
//states
, unitsdone(0)
, totalunits(0)
, barsnow(0)
, unitsinblock(0)
, unitsinsideblock(0)
, barsinsample(1)
, slicestart(0)
, mincutquarters(0.0)
, phraselimit(0)
, strategy(&cutproc11)
{
  procs.reserve(kNumCutProcs);
  procs.push_back(&cutproc11);
  procs.push_back(&warpcutproc);
  procs.push_back(&sqpusher);
  procs.push_back(&euclid);
  procs.push_back(&ratchet);
  procs.push_back(&gate);
  
  player.SetListenerManager(&listenermanager);
}

 BBCutter::~BBCutter()
{
}

void BBCutter::RegisterListener(BBCutListener *l)
{
  listenermanager.RegisterListener(l);
}

void	BBCutter::SetCutProc(long i)
{
  assert(i>=0 && i<procs.size());
  if(i>=0 && i<procs.size())
  {
    strategy = procs[i];
  }
}

void	BBCutter::SetBarsInSample(long bars) { barsinsample = bars; UpdateRates(); }
void	BBCutter::SetTempo(double v) { tempo=v; UpdateRates(); }
// A new subdivision mid-phrase: the phrase keeps its bars, its unit counts move to the new grid (otherwise its
// length is no longer a whole number of bars and it never lines up with the bar again), and the block that was cut
// on the old grid is replaced at the next unit.
void	BBCutter::SetSubdiv(long v)
{
  if(v > 0 && subdiv > 0 && v != subdiv && totalunits > 0)
  {
    const double f = double(v) / double(subdiv);
    const long bars = (totalunits + subdiv - 1) / subdiv;
    totalunits = bars * v;
    unitsdone = std::max(0L, std::min(totalunits, long(std::floor(double(unitsdone) * f + 0.5))));
    unitsinsideblock = unitsinblock; // new block at the next unit
  }
  subdiv = v;
  UpdateRates();
}
void	BBCutter::SetStutterChance(float chance)  { cutproc11.SetStutterChance(chance);}
void	BBCutter::SetStutterArea(float area)      { cutproc11.SetStutterArea(area);}
void	BBCutter::SetMaxRepeats(long repeats)     { cutproc11.SetMaxRepeats(repeats);}
void	BBCutter::SetMinRepeats(long repeats)     { cutproc11.SetMinRepeats(repeats);}
void	BBCutter::SetStraightChance(float chance) { warpcutproc.SetStraightChance(chance);}
void	BBCutter::SetRegularChance(float chance)  { warpcutproc.SetRegularChance(chance);}
void	BBCutter::SetRitardChance(float chance)   { warpcutproc.SetRitardChance(chance);}
void	BBCutter::SetAccel(float v)               { warpcutproc.SetAccel(v);}
void	BBCutter::SetActivity(float v)            { sqpusher.SetActivity(v);}
void	BBCutter::SetEuclidDensity(float v)     { euclid.SetDensity(v);}
void	BBCutter::SetEuclidChance(float v)      { euclid.SetChance(v);}
void	BBCutter::SetEuclidSlice(long v)        { euclid.SetSlice(v);}
void	BBCutter::SetRatchetIntensity(float v)  { ratchet.SetIntensity(v);}
void	BBCutter::SetRatchetCurve(float v)      { ratchet.SetCurve(v);}
void	BBCutter::SetFade(float v)                { player.SetFade( ms2samples(v,sr) );}
void	BBCutter::SetMinPhraseLength(long v) { for_each(procs.begin(),procs.end(),bind2nd(mem_fun(&CutProc::SetMinPhraseLength),v));}
void	BBCutter::SetMaxPhraseLength(long v) { for_each(procs.begin(),procs.end(),bind2nd(mem_fun(&CutProc::SetMaxPhraseLength),v));}
void	BBCutter::SetMinAmp(float v)      { for_each(procs.begin(),procs.end(),bind2nd(mem_fun(&CutProc::SetMinAmp),v));}
void	BBCutter::SetMaxAmp(float v)      { for_each(procs.begin(),procs.end(),bind2nd(mem_fun(&CutProc::SetMaxAmp),v));}
void	BBCutter::SetMinPan(float v)      { for_each(procs.begin(),procs.end(),bind2nd(mem_fun(&CutProc::SetMinPan),v));}
void	BBCutter::SetMaxPan(float v)      { for_each(procs.begin(),procs.end(),bind2nd(mem_fun(&CutProc::SetMaxPan),v));}
void	BBCutter::SetDutyCycle(float v)   { for_each(procs.begin(),procs.end(),bind2nd(mem_fun(&CutProc::SetDutyCycle),v));}
void	BBCutter::SetFillDutyCycle(float v) { for_each(procs.begin(),procs.end(),bind2nd(mem_fun(&CutProc::SetFillDutyCycle),v));}
void	BBCutter::SetMinDetune(float v)   { for_each(procs.begin(),procs.end(),bind2nd(mem_fun(&CutProc::SetMinDetune),v));}
void	BBCutter::SetMaxDetune(float v)   { for_each(procs.begin(),procs.end(),bind2nd(mem_fun(&CutProc::SetMaxDetune),v));}
void	BBCutter::SetNumerator(double v)  { numerator   = v; beatsPerBar=4.0*numerator/denominator; UpdateRates();}
void	BBCutter::SetDenominator(double v) { denominator = v; beatsPerBar=4.0*numerator/denominator; UpdateRates();}

void	BBCutter::SetSampleRate(double v)
{
  sr = v;
  UpdateRates();
}

void	BBCutter::SetTimeInfos(double bpm,double num,double den,double srate)
{
  bool changed = false;
  if(tempo != bpm)
  {
    tempo = bpm;
    changed = true;
  }
  if(numerator != num)
  {
    numerator = num;
    changed = true;
  }
  if(denominator != den)
  {
    denominator = den;
    changed = true;
  }
  if(srate != sr)
  {
    sr = srate;
    changed = true;
  }
  if(changed)
    UpdateRates();
}

void	BBCutter::UpdateRates()
{
}

void	BBCutter::Phrase(long bar, long sd)
{
  barsnow = strategy->ChoosePhraseLength();
  if(phraselimit > 0 && barsnow > phraselimit)
    barsnow = phraselimit;
  totalunits = barsnow*subdiv;
  unitsdone = 0;
  
  listenermanager.OnPhrase(bar,sd);
}

void	BBCutter::Block(long bar,long sd)
{
  unitsinsideblock=0;
  std::vector<CutInfo> cuts;
  strategy->ChooseCuts(cuts,unitsinblock,
                       unitsdone,totalunits,subdiv,SamplesPerUnit());
  // (the gate's steps are its grid: merging them would move the pattern)
  if(mincutquarters > 0.0 && tempo > 0.0 && strategy != &gate)
    EnforceMinCut(cuts, long(mincutquarters * sr * 60.0 / tempo));
  player.OnBlock(cuts);
  
  listenermanager.OnBlock(bar,sd);
}

void	BBCutter::Unit(long bar, long sd)
{
  // safety net: a phrase whose remaining length is not whole bars can never end on a bar line; restart it on one
  if(sd == 0 && totalunits > 0 && unitsdone >= 0 && unitsdone < totalunits && subdiv > 0
     && ((totalunits - unitsdone) % subdiv) != 0)
  {
    totalunits = 0;
    unitsinsideblock = unitsinblock;
  }

  if( totalunits<=0 || unitsdone>=totalunits || unitsdone<0 ) //out of phrase bounds and start of a bar
  {
    if(sd == 0)
      Phrase(bar,sd);
    else
      return;
  }
  
  if( unitsinsideblock>=unitsinblock || unitsinsideblock<0) //out of block bounds
    Block(bar,sd);
  
  unitsinsideblock++;
  unitsdone++;
  
  listenermanager.OnUnit(bar,sd);
}

void	BBCutter::SetPosition(long bar, long sd)
{
  const long delta = sd - (unitsdone % long(UnitsPerBar(subdiv,numerator,denominator)));
  unitsinsideblock += delta;
  unitsdone += delta;
  Unit(bar,sd);
}

