#pragma once
#include "CoreMinimal.h"
/** Bounded120Hz accumulator. At most8 steps/frame; discard excess whole steps and count lost time.
 * Presentation alpha is residual/fixed dt. Failure resets backlog and stops the batch. */
struct FKartFixedStepClock
{
    static constexpr double StepSeconds=1./120;
    double Accumulator=0,DroppedSeconds=0;
    uint64 Steps=0;
    bool Advance(double FrameSeconds,TFunctionRef<bool(double)> Step)
    {
        if(!FMath::IsFinite(FrameSeconds) || FrameSeconds<0)return false;
        Accumulator+=FMath::Min(FrameSeconds,.25);DroppedSeconds+=FMath::Max(0.,FrameSeconds-.25);
        int Count=0;
        while(Accumulator+1.e-12>=StepSeconds && Count<8)
        {if(!Step(StepSeconds)){Accumulator=0;return false;}Accumulator=FMath::Max(0.,Accumulator-StepSeconds);++Steps;++Count;}
        if(Accumulator>=StepSeconds){const double Lost=FMath::FloorToDouble(Accumulator/StepSeconds)*StepSeconds;DroppedSeconds+=Lost;Accumulator-=Lost;}
        return true;
    }
    double Alpha() const {return FMath::Clamp(Accumulator/StepSeconds,0.,1.);}
    void Reset(){*this=FKartFixedStepClock();}
};
