﻿// Fill out your copyright notice in the Description page of Project Settings.


#include "Components/Component_ActorIdentity.h"

#include "Misc/OmegaUtils_Enums.h"

bool UActorIdentityComponent::Local_IsSourceAssetValid() const
{
	if(IdentitySource && IdentitySource->GetClass()->ImplementsInterface(UDataInterface_ActorIdentitySource::StaticClass()))
	{
		return true;
	}
	return false;
}

TArray<UActorIdentityScript*> UActorIdentityComponent::Local_GetScripts() const
{
	TArray<UActorIdentityScript*> out;
	if(Local_IsSourceAssetValid())
	{
		return IDataInterface_ActorIdentitySource::Execute_GetIdentityScripts(IdentitySource);
	}
	return out;
}

void UActorIdentityComponent::SetIdentitySourceAsset(UPrimaryDataAsset* SourceAsset)
{
	if(SourceAsset!=IdentitySource)
	{
		if(SourceAsset)
		{
			IdentitySource=SourceAsset;
		}
		else
		{
			IdentitySource=nullptr;
		}
		OnActorIdentityChanged.Broadcast(IdentitySource,this);
	}
}

#if WITH_EDITOR
void UActorIdentityComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	SetIdentitySourceAsset(IdentitySource);
	if(Local_IsSourceAssetValid())
	{
		IDataInterface_ActorIdentitySource::Execute_OnActorConstruction(IdentitySource,GetOwner());
		for(auto* i : Local_GetScripts())
		{
			if(i) { i->OnActorConstruction(GetOwner()); }
		}
	}
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

void UActorIdentityComponent::BeginPlay()
{
	GetWorld()->GetSubsystem<UOmegaActorSubsystem>()->local_RegisterActorIdComp(this,true);
	if(Local_IsSourceAssetValid())
	{
		IDataInterface_ActorIdentitySource::Execute_OnActorBeginPlay(IdentitySource,GetOwner());
		for(auto* i : Local_GetScripts())
		{
			if(i) { i->OnActorBeginPlay(GetOwner()); }
		}
	}
	Super::BeginPlay();
}

void UActorIdentityComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if(EndPlayReason==EEndPlayReason::Destroyed)
	{
		GetWorld()->GetSubsystem<UOmegaActorSubsystem>()->local_RegisterActorIdComp(this,false);
	}
	Super::EndPlay(EndPlayReason);
}

void UActorIdentityComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                            FActorComponentTickFunction* ThisTickFunction)
{
	if(Local_IsSourceAssetValid())
	{
		IDataInterface_ActorIdentitySource::Execute_OnActorTick(IdentitySource,GetOwner(),DeltaTime);
		for(auto* i : Local_GetScripts())
		{
			if(i) { i->OnActorTick(GetOwner(),DeltaTime); }
		}
	}
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

TArray<UActorIdentityScript*> UActorIdentityPreset::GetIdentityScripts_Implementation()
{
	TArray<UActorIdentityScript*> out;
	for(auto* i : PreSetup_Scripts)
	{
		if(i) { out.Append(i->GetIdentityScripts_Implementation());}
	}
	for(auto* i : Local_Scripts)
	{
		if(i) { out.Add(i);}
	}
	for(auto* i : PostSetup_Scripts)
	{
		if(i) { out.Append(i->GetIdentityScripts_Implementation());}
	}
	return out;
}

void UOmegaActorSubsystem::local_RegisterActorIdComp(UActorIdentityComponent* Component, bool bIsRegister)
{
	if(Component)
	{
		if(bIsRegister && !REF_ActorIdComps.Contains(Component))
		{
			REF_ActorIdComps.Add(Component);
		}
		if(!bIsRegister && REF_ActorIdComps.Contains(Component))
		{
			REF_ActorIdComps.Remove(Component);
		}
	}
}

TArray<UActorIdentityComponent*> UOmegaActorSubsystem::GetAllActorIdentityComponents()
{
	TArray<UActorIdentityComponent*>out;
	for(auto* i : REF_ActorIdComps)
	{
		if(i) {out.Add(i);}
	}
	return out;
}

TArray<AActor*> UOmegaActorIdentityFunctions::GetAllActorsWithIdentity(UObject* WorldContextObject,
                                                                       UPrimaryDataAsset* Asset, TSubclassOf<AActor> FilterClass)
{
	TArray<AActor*> out;
	if (WorldContextObject && Asset)
	{
		for (auto* c : WorldContextObject->GetWorld()->GetSubsystem<UOmegaActorSubsystem>()->GetAllActorIdentityComponents())
		{
			if(c && c->IdentitySource==Asset && (!FilterClass || c->GetOwner()->GetClass()->IsChildOf(FilterClass)))
			{
				out.Add(c->GetOwner());
			}
		}
	}
	return out;
}

AActor* UOmegaActorIdentityFunctions::GetFirstActorWithIdentity(UObject* WorldContextObject, UPrimaryDataAsset* Asset,
	TSubclassOf<AActor> FilterClass, TEnumAsByte<EOmegaFunctionResult>& Outcome)
{
	TArray<AActor*> list=GetAllActorsWithIdentity(WorldContextObject,Asset,FilterClass);
	if(list.IsValidIndex(0))
	{
		Outcome=EOmegaFunctionResult::Success;
		return list[0];
	}
	Outcome=EOmegaFunctionResult::Fail;
	return nullptr;
}

