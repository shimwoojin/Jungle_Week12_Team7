#include "ParticleStats.h"

#if STATS
uint32 FParticleStats::SpriteParticles = 0;
uint32 FParticleStats::MeshParticles = 0;
uint32 FParticleStats::TotalParticles = 0;
uint32 FParticleStats::PeakTotalParticles = 0;
uint32 FParticleStats::EmitterCount = 0;
uint32 FParticleStats::ComponentCount = 0;
uint32 FParticleStats::ParticlesSpawned = 0;
uint32 FParticleStats::ParticlesKilled = 0;
uint32 FParticleStats::DrawCalls = 0;
uint64 FParticleStats::GTMemoryBytes = 0;
uint64 FParticleStats::ActiveDataBytes = 0;
uint64 FParticleStats::ReservedDataBytes = 0;
#endif
