#pragma once
#include "KartPawn.h"
#include "KartPhysicsDrivingEnvelope.h"

namespace KartPhysicsEvaluation
{
// Evaluation controls only: no actor relocation, gate awards or training promotion.
inline void DriveRoute(AKartPawn& K,const TArray<FVector>& Route)
{
    if(Route.Num()<3)return;
    const FVector P=K.GetActorLocation();int Nearest=0;double Best=DBL_MAX;
    for(int I=0;I<Route.Num();++I){const double D=FVector::DistSquared2D(P,Route[I]);if(D<Best){Best=D;Nearest=I;}}
    const double Speed=FMath::Abs(K.GetSpeedKmh())/.036;
    const double Look=FMath::Clamp(250+Speed*.35,300.,750.);double Travel=0;int Target=Nearest;
    while(Travel<Look){const int Next=(Target+1)%Route.Num();Travel+=FVector::Dist2D(Route[Next],Route[Target]);Target=Next;}
    const FVector Local=K.GetActorQuat().UnrotateVector(Route[Target]-P);
    const double Curvature=2*Local.Y/FMath::Max(10000.,Local.SizeSquared2D());
    const double Steering=FMath::Atan(Curvature*100)*(1+FMath::Square(Speed/1300.))/.45;
    const double TargetSpeed=FMath::Min(KartPhysicsDriving::SpeedEnvelope(Route,Nearest,0,K.GetSpeedLimitKmh()/.036),FMath::Sqrt(350/FMath::Max(.0001,FMath::Abs(Curvature))));
    K.SetDriveInput(Speed<TargetSpeed?1:.1,FMath::Clamp(Steering,-1.,1.),Speed>TargetSpeed+120);
}
}
