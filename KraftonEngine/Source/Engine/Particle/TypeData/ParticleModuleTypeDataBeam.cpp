#include "ParticleModuleTypeDataBeam.h"

#include "Particle/ParticleEmitterInstance.h"

FParticleEmitterInstance* UParticleModuleTypeDataBeam::CreateInstance(UParticleSystemComponent* /*InComponent*/)
{
	// PSC/emitter 경로가 Init()을 수행. 여기선 runtime instance type만 고르고
	// 외부(게임/EventGenerator)가 SetEndpoints 하기 전까지 쓸 기본 endpoint를 박는다.
	FParticleBeamEmitterInstance* Inst = new FParticleBeamEmitterInstance();
	Inst->SetEndpoints(DefaultSource, DefaultTarget);
	return Inst;
}
