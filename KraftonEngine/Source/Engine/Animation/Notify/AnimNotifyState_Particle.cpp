#include "AnimNotifyState_Particle.h"

#include "Component/Primitive/SkeletalMeshComponent.h"
#include "Component/Particle/ParticleSystemComponent.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "GameFramework/GameMode/GameplayStatics.h"
#include "Mesh/Skeletal/SkeletalMesh.h"
#include "Mesh/Skeletal/SkeletalMeshAsset.h"

namespace
{
	int32 FindBoneIndex(USkeletalMeshComponent* MeshComp, const FString& BoneName)
	{
		if (!MeshComp || BoneName.empty()) return -1;

		USkeletalMesh* Mesh = MeshComp->GetSkeletalMesh();
		FSkeletalMesh* Asset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
		if (!Asset) return -1;

		for (int32 BoneIndex = 0; BoneIndex < static_cast<int32>(Asset->Bones.size()); ++BoneIndex)
		{
			if (Asset->Bones[BoneIndex].Name == BoneName)
			{
				return BoneIndex;
			}
		}
		return -1;
	}

	// 스폰/추적 위치 = (본이 있으면 본 월드위치, 없으면 actor 위치) + actor-로컬 오프셋.
	FVector ResolveSpawnLocation(USkeletalMeshComponent* MeshComp, AActor* Owner, const FString& BoneName, const FVector& Offset)
	{
		const FVector WorldOffset = Owner
			? (Owner->GetActorForward() * Offset.X + Owner->GetActorRight() * Offset.Y + FVector::UpVector * Offset.Z)
			: Offset;

		const int32 BoneIndex = FindBoneIndex(MeshComp, BoneName);
		if (BoneIndex >= 0)
		{
			return MeshComp->GetBoneLocationByIndex(BoneIndex) + WorldOffset;
		}
		return Owner ? Owner->GetActorLocation() + WorldOffset : WorldOffset;
	}
}

void UAnimNotifyState_Particle::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* /*Anim*/, float /*TotalDuration*/)
{
	if (!MeshComp) return;

	UWorld* World = MeshComp->GetWorld();
	if (!World) return;

	const FString& Path = TemplatePath;
	if (Path.empty() || Path == "None") return;

	AActor* Owner = MeshComp->GetOwner();
	const FVector SpawnLoc = ResolveSpawnLocation(MeshComp, Owner, BoneName, LocationOffset);

	// 이미 이 mesh 용 PSC 가 있으면 재활성화(반복 발동 시 actor 누적 방지).
	auto It = SpawnedByMesh.find(MeshComp);
	if (It != SpawnedByMesh.end() && It->second)
	{
		UParticleSystemComponent* PSC = It->second;
		if (AActor* PA = PSC->GetOwner()) PA->SetActorLocation(SpawnLoc);
		PSC->Activate(true);
		return;
	}

	UParticleSystemComponent* PSC = FGameplayStatics::SpawnEmitterAtLocation(World, Path, SpawnLoc);
	if (PSC)
	{
		SpawnedByMesh[MeshComp] = PSC;
	}
}

void UAnimNotifyState_Particle::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* /*Anim*/, float /*FrameDeltaTime*/)
{
	if (!bFollowBone || !MeshComp) return;

	auto It = SpawnedByMesh.find(MeshComp);
	if (It == SpawnedByMesh.end() || !It->second) return;

	AActor* Owner = MeshComp->GetOwner();
	const FVector Loc = ResolveSpawnLocation(MeshComp, Owner, BoneName, LocationOffset);
	if (AActor* PA = It->second->GetOwner())
	{
		PA->SetActorLocation(Loc);
	}
}

void UAnimNotifyState_Particle::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* /*Anim*/)
{
	auto It = SpawnedByMesh.find(MeshComp);
	if (It == SpawnedByMesh.end()) return;

	// Deactivate 만 — 잔여 입자 소멸 후 스스로 비활성. actor 는 재사용 위해 유지
	// (anim tick 중 즉시 DestroyActor 는 world actor 순회 무효화 위험이라 회피). 다음 Begin 이 재활성화.
	if (UParticleSystemComponent* PSC = It->second)
	{
		PSC->Deactivate();
	}
}
