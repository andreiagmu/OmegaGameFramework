// Copyright Studio Syndicat 2021. All Rights Reserved.


#include "Zone/OmegaZoneSubsystem.h"
#include "Zone/OmegaZoneGameInstanceSubsystem.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "OmegaSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "MovieSceneSequencePlayer.h"
#include "OmegaGameFrameworkBPLibrary.h"
#include "OmegaGameManager.h"
#include "TimerManager.h"
#include "Engine/LevelStreaming.h"
#include "OmegaGameplaySystem.h"
#include "Save/OmegaSaveSubsystem.h"
#include "Zone/OmegaZoneGameInstanceSubsystem.h"

void UOmegaZoneSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	FallbackZone = NewObject<UOmegaZoneData>(this,UOmegaZoneData::StaticClass());
}

void UOmegaZoneSubsystem::Tick(float DeltaTime)
{
	/*
	if(GamInstSubsys && GamInstSubsys->IsInlevelTransit && !bIsInLevelTransit)
	{
		if(UGameplayStatics::GetPlayerCharacter(this,0))
		{
			GamInstSubsys->IsInlevelTransit = false;
			FTimerHandle LocalTimer;
			GetWorld()->GetTimerManager().SetTimer(LocalTimer, this, &UOmegaZoneSubsystem::OnLoadFromLevelComplete, 0.1f, false);
		}
	}
	*/
}

UOmegaLevelData* UOmegaZoneSubsystem::GetLevelData(TSoftObjectPtr<UWorld> SoftLevelReference)
{
	// Check if the Soft Level Reference is valid
	/*if (!SoftLevelReference.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("Soft Level Reference is not valid! Cannot load World Data Asset."));
		return nullptr;
	}
*/
	// Get the current level path
	FString CurrentLevelPath = SoftLevelReference.GetLongPackageName();
	UE_LOG(LogTemp, Warning, TEXT("LEVEL DATA CHECK --> CurrentLevelPath: %s"), *CurrentLevelPath);
	// Remove ":PersistentLevel" from the level path
	FString LevelPathWithoutPersistentLevel = CurrentLevelPath.Replace(TEXT(":PersistentLevel"), TEXT(""));
	LevelPathWithoutPersistentLevel.ReplaceInline(TEXT("UEDPIE_0_"),TEXT(""));
	
	UE_LOG(LogTemp, Warning, TEXT("LEVEL DATA CHECK --> LevelPathWithoutPersistentLevel: %s"), *LevelPathWithoutPersistentLevel);
	FString MainNameLevel;
	FString MainNamePath;
	
	// Find the position of the first occurrence of "."
	int32 DotIndex = -1;
	if (LevelPathWithoutPersistentLevel.FindChar('/', DotIndex))
	{
		// Extract the level name from the path
		LevelPathWithoutPersistentLevel.Split("/",&MainNamePath,&MainNameLevel,ESearchCase::IgnoreCase,ESearchDir::FromEnd);
		
		UE_LOG(LogTemp, Warning, TEXT("LEVEL DATA CHECK --> Final Level: %s"), *MainNameLevel);
		UE_LOG(LogTemp, Warning, TEXT("LEVEL DATA CHECK --> Final Path: %s"), *MainNamePath);
		// Construct the DataAsset path
		FString DataAssetPath = LevelPathWithoutPersistentLevel +  TEXT("_WorldData.") + MainNameLevel + TEXT("_WorldData");
		
		// Print the DataAsset path to the log for debugging
		UE_LOG(LogTemp, Warning, TEXT("LEVEL DATA CHECK --> DataAssetPath: %s"), *DataAssetPath);

		// Load the DataAsset
		UOmegaLevelData* OmegaLevelData = Cast<UOmegaLevelData>(StaticLoadObject(UOmegaLevelData::StaticClass(), nullptr, *DataAssetPath));

		if (!OmegaLevelData)
		{
			// Handle the case when the DataAsset is not found.
			UE_LOG(LogTemp, Warning, TEXT("OmegaLevelData '%s' not found!"), *DataAssetPath);
		}

		return OmegaLevelData;
	}
	else
	{
		// Handle the case when "." is not found in the path.
		UE_LOG(LogTemp, Warning, TEXT("Invalid level path: %s"), *CurrentLevelPath);
		return nullptr;
	}
}

UOmegaLevelData* UOmegaZoneSubsystem::GetCurrentLevelData()
{
	if(!LevelData)
	{
		LevelData = GetLevelData(GetCurrentLevelSoftReference());
	}
	return LevelData;
}

TSoftObjectPtr<UWorld> UOmegaZoneSubsystem::GetCurrentLevelSoftReference()
{
	UWorld* CurrentWorld = GetWorld();
	if (!CurrentWorld)
	{
		UE_LOG(LogTemp, Warning, TEXT("No current world found!"));
		return nullptr;
	}
	const TSoftObjectPtr<UWorld> LevelAssetPtr(GetWorld()->GetCurrentLevel()->GetOuter()); // Get the level path name
	return LevelAssetPtr;
}

void UOmegaZoneSubsystem::OnLoadFromLevelComplete()
{
	AOmegaZonePoint* OutPoint = nullptr;
	for(auto* TempPoint : ZonePoints)
	{
		if(TempPoint
			&& TempPoint->FromLevel==GetWorld()->GetGameInstance()->GetSubsystem<UOmegaZoneGameInstanceSubsystem>()->PreviousLevel
			&& TempPoint->ZonePointID==GetWorld()->GetGameInstance()->GetSubsystem<UOmegaZoneGameInstanceSubsystem>()->TargetSpawnPointTag)
		{
			OutPoint = TempPoint;
			break;
		}
	}
	if(OutPoint)
	{
		TransitPlayerToPoint(OutPoint, UGameplayStatics::GetPlayerController(this,0));
	}
	
}


void UOmegaZoneSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	GamInstSubsys = GetWorld()->GetGameInstance()->GetSubsystem<UOmegaZoneGameInstanceSubsystem>();
		
	FTimerHandle LocalTimer;
	GetWorld()->GetTimerManager().SetTimer(LocalTimer, this, &UOmegaZoneSubsystem::SpawnFromStartingPoint, 0.1f, false);
}

void UOmegaZoneSubsystem::SpawnFromStartingPoint()
{
	// Is in Level Transit?
	if(GamInstSubsys->IsInlevelTransit)
	{
		TArray<AActor*> TempPoints;
		AOmegaZonePoint* TempPoint = nullptr;

		// If loading from a save game, spawn at saves spot.
		if(GetWorld()->GetGameInstance()->GetSubsystem<UOmegaGameManager>()->IsFlagActive("$$LoadingLevel$$"))
		{
			GetWorld()->GetGameInstance()->GetSubsystem<UOmegaGameManager>()->SetFlagActive("$$LoadingLevel$$",false);
				
			const UOmegaSaveGame* TempSave = GetWorld()->GetGameInstance()->GetSubsystem<UOmegaSaveSubsystem>()->ActiveSaveData;

			TempPoint = GetWorld()->SpawnActorDeferred<AOmegaZonePoint>(AOmegaZonePoint::StaticClass(),TempSave->SavedPlayerTransform);
			TempPoint->SetLifeSpan(1);
			TempPoint->ZoneToLoad=TempSave->ActiveZone;
			TempPoint->FinishSpawning(TempSave->SavedPlayerTransform);
		}
		else
		{
			//Try Get Level Transit spawn point
			UGameplayStatics::GetAllActorsOfClass(this,AOmegaZonePoint::StaticClass(),TempPoints);
			for (auto* TempActor: TempPoints)
			{
				const AOmegaZonePoint* CheckedPoint = Cast<AOmegaZonePoint>(TempActor);
				if(CheckedPoint && CheckedPoint->FromLevel==GamInstSubsys->PreviousLevel && GamInstSubsys->TargetSpawnPointTag==CheckedPoint->ZonePointID) //Is valid spawn point;
					{
						TempPoint = Cast<AOmegaZonePoint>(TempActor);
						break;

					}
			}
		}
		
		if(TempPoint)
		{
			UE_LOG(LogTemp, Display, TEXT("Begining Level Spawn Transit: %s"), *TempPoint->GetName());
			TransitPlayerToPoint(TempPoint, UGameplayStatics::GetPlayerController(this,0));
		}
	}
	else
	{
		LoadDefaultZone();
	}
	GamInstSubsys->IsInlevelTransit = false;
}

void UOmegaZoneSubsystem::LoadDefaultZone()
{
	if(GetCurrentLevelData() && GetCurrentLevelData()->GetDefaultZoneData())  // Load Default Level Data
	{
		LoadZone(GetCurrentLevelData()->GetDefaultZoneData());
	}
	else
	{
		LoadZone(FallbackZone);
	}
}


// Begins the a transition event
void UOmegaZoneSubsystem::LoadZone(UOmegaZoneData* Zone, bool UnloadPreviousZones)
{
	if(!IsMidPlayerTransit)
	{
		if(Zone)
		{
			UE_LOG(LogTemp, Display, TEXT("Begin Zone Load: %s"), *Zone->GetName());
			IncomingZone_Load = Zone;
		}
		bUnloadPreviousZones = UnloadPreviousZones;
		IsMidPlayerTransit = true;		//marks the transition event as active
		
		Local_PreBeginTransitActions();
		Local_OnBeginTransitSequence(true);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Cannot changes zones, Player is mid-transit"));
	}
}

void UOmegaZoneSubsystem::UnloadZone(UOmegaZoneData* Zone)
{
	if(Zone)
	{
		UE_LOG(LogTemp, Warning, TEXT("Unloading Zone: %s"), *Zone->GetName());
		IncomingZone_Unload = Zone;
		Local_OnBeginLoadTaskList(IncomingZone_Unload->StreamedLevels, false);
	}
}

TArray<UOmegaZoneData*> UOmegaZoneSubsystem::GetLoadedZones()
{
	return LoadedZones;
}

UOmegaZoneData* UOmegaZoneSubsystem::GetTopLoadedZones()
{
	if(GetLoadedZones().IsValidIndex(0))
	{
		return GetLoadedZones()[0];
	}
	return nullptr;
}

bool UOmegaZoneSubsystem::IsZoneLoaded(UOmegaZoneData* Zone)
{
	if(Zone)
	{
		return GetLoadedZones().Contains(Zone);
	}
	return false;
}

AOmegaZonePoint* UOmegaZoneSubsystem::GetZonePointFromID(FGameplayTag ID)
{
	for(auto* TempPoint : ZonePoints)
	{
		if(TempPoint && (TempPoint->ZonePointID.MatchesTagExact(ID)))
		{
			return TempPoint;
		}
	}
	return nullptr;
}

//#########################################################################################################
// TRANSIT
//#########################################################################################################

void UOmegaZoneSubsystem::TransitPlayerToPointID(FGameplayTag PointID, APlayerController* Player)
{
	TransitPlayerToPoint(GetZonePointFromID(PointID), Player);
}

void UOmegaZoneSubsystem::TransitPlayerToLevel_Name(FName Level, FGameplayTag SpawnID)
{
	UOmegaZoneGameInstanceSubsystem* InstSubsys = GetWorld()->GetGameInstance()->GetSubsystem<UOmegaZoneGameInstanceSubsystem>();
	bIsInLevelTransit = true;
	InstSubsys->IsInlevelTransit = true;
	InstSubsys->TargetSpawnPointTag = SpawnID;
	
	
	const UWorld* CurrentWorld = GetWorld();
	const ULevel* CurrentLevel = CurrentWorld->GetCurrentLevel();
	const TSoftObjectPtr<UWorld> LevelAssetPtr(CurrentLevel->GetOuter());
	
	InstSubsys->PreviousLevel=LevelAssetPtr;
	
	IncomingLevelName = Level;

	UE_LOG(LogTemp, Warning, TEXT("BEGINING Level Transit: %s"), *IncomingLevelName.ToString());
	Local_PreBeginTransitActions();
	Local_OnBeginTransitSequence(true);
}

void UOmegaZoneSubsystem::TransitPlayerToLevel(TSoftObjectPtr<UWorld> Level, FGameplayTag SpawnID)
{
	const FString StartPath = Level.ToString();
	FString EmptyPath;
	FString targetLevel;

	StartPath.Split(TEXT("."),&EmptyPath,&targetLevel);
	
	TransitPlayerToLevel_Name(FName(targetLevel),SpawnID);
	
}

void UOmegaZoneSubsystem::TransitPlayerToPoint(AOmegaZonePoint* Point, APlayerController* Player)
{
	if(Point)
	{
		Incoming_SpawnPoint = Point;
		const APlayerController* TargetPlayer = UGameplayStatics::GetPlayerController(this, 0);
		if(Player)
		{
			TargetPlayer = Player;
		}
		if(Point->ZoneToLoad)
		{
			LoadZone(Point->ZoneToLoad);
		}
		else
		{
			LoadDefaultZone();
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Tried to transit but POINT was invalid"));
	}
	
}

//------------------------------------
// ADVANCED
//------------------------------------

// COMPLETES Transition Event
void UOmegaZoneSubsystem::Local_CompleteTransit()
{
	GetWorld()->GetSubsystem<UOmegaGameplaySubsystem>()->ShutdownGameplaySystem(GetZoneGameplaySystem(), this, "");
	if(GetTopLoadedZones())
	{
		GetWorld()->GetSubsystem<UOmegaBGMSubsystem>()->PlayBGM(GetTopLoadedZones()->ZoneBGM, GetMutableDefault<UOmegaSettings>()->ZoneBGMSlot,false,true);

	}
	IsMidPlayerTransit=false;
	Local_IsWaitForLoad = false;
}

void UOmegaZoneSubsystem::Local_PreBeginTransitActions()
{
	GetWorld()->GetSubsystem<UOmegaGameplaySubsystem>()->ActivateGameplaySystem(GetZoneGameplaySystem(), this, "");
}


void UOmegaZoneSubsystem::Local_FinishZoneLoad()
{
	Local_IsWaitForLoad = false;
	TArray<FName> LocalLevels;
	if(IncomingZone_Load)
	{
		LoadedZones.Add(IncomingZone_Load);
		LocalLevels = IncomingZone_Load->StreamedLevels;
	}
	
	Local_OnBeginLoadTaskList(LocalLevels, true);
}

TSubclassOf<AOmegaGameplaySystem> UOmegaZoneSubsystem::GetZoneGameplaySystem()
{
	if(TSubclassOf<AOmegaGameplaySystem> LocalSystem = GetMutableDefault<UOmegaSettings>()->ZoneTransitSystem.TryLoadClass<AOmegaGameplaySystem>())
	{
		return LocalSystem;
	}
	return nullptr;
}

void UOmegaZoneSubsystem::Local_OnBeginLoadTaskList(TArray<FName> Levels, bool Loaded)
{
	Incoming_LoadList = Levels;
	Incoming_LoadState = Loaded;
	Local_OnNextLoadEvent();
	
}

void UOmegaZoneSubsystem::Local_OnNextLoadEvent()
{
	FName Local_Name;
	//End if List is empty
	if(Incoming_LoadList.IsValidIndex(0))
	{
		UE_LOG(LogTemp, Warning, TEXT("STARTING next level stream load/unload"));
		Local_Name = Incoming_LoadList[0];
		Incoming_LoadList.Remove(Local_Name);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("COMPLETE Load/Unload Steam Events"));
		Local_OnFinishLoadTask(Incoming_LoadState);
		return;
	}

	// Setup Latant action
	FLatentActionInfo Local_LatentLoad;
	Local_LatentLoad.CallbackTarget = this;
	Local_LatentLoad.ExecutionFunction = FName("Local_OnNextLoadEvent");
	Local_LatentLoad.Linkage = 0;
	
	if(Incoming_LoadState)
	{
		UGameplayStatics::LoadStreamLevel(this, Local_Name, true, false, Local_LatentLoad);
		UE_LOG(LogTemp, Warning, TEXT("Loading Steam Level: %s"), *Local_Name.ToString());
	}
	else
	{
		UGameplayStatics::UnloadStreamLevel(this, Local_Name, Local_LatentLoad, true);
		UE_LOG(LogTemp, Warning, TEXT("Unloading Steam Level: %s"), *Local_Name.ToString());
	}
}

// Runs after all the levels are loaded/unloaded
void UOmegaZoneSubsystem::Local_OnFinishLoadTask(bool LoadState)
{
	if(LoadState)
	{
		UE_LOG(LogTemp, Display, TEXT("Finish Zone LOAD"));
		
		if(Incoming_LoadTaskZone)
		{
			// LOAD FINISH
			OnZoneLoaded.Broadcast(Incoming_LoadTaskZone);
		
			//ACTIVATE SYSTEMS
			for(TSubclassOf<AOmegaGameplaySystem> TempSys : Incoming_LoadTaskZone->SystemsActivatedInZone)
			{
				if(TempSys)
				{
					GetWorld()->GetSubsystem<UOmegaGameplaySubsystem>()->ActivateGameplaySystem(TempSys);
				}
			}
		}
		
		//Teleport Player to spawn point
		if(Incoming_SpawnPoint)
		{
			APawn* PawnRef = UGameplayStatics::GetPlayerPawn(this,0);
			
			PawnRef->SetActorTransform(Incoming_SpawnPoint->GetActorTransform());
			UGameplayStatics::GetPlayerController(this,0)->SetControlRotation(PawnRef->GetActorRotation());
			OnPlaySpawnedAtPoint.Broadcast(UGameplayStatics::GetPlayerController(this,0),Incoming_SpawnPoint);
		}
		
		Local_OnBeginTransitSequence(false);
	}
	else
	{
		UE_LOG(LogTemp, Display, TEXT("Finish Zone UNLOAD"));
		// UNLOAD FINISH
		if(LoadedZones.Contains(IncomingZone_Unload))
		{
			LoadedZones.Remove(IncomingZone_Unload);
		}
		
		if(Incoming_LoadTaskZone)
		{
			OnZoneUnloaded.Broadcast(Incoming_LoadTaskZone);

			//SHUTDOWN SYSTEMS
			for(TSubclassOf<AOmegaGameplaySystem> TempSys : Incoming_LoadTaskZone->SystemsActivatedInZone)
			{
				if(TempSys)
				{
					GetWorld()->GetSubsystem<UOmegaGameplaySubsystem>()->ShutdownGameplaySystem(TempSys);
				}
			}
		}
	}

	if(Local_IsWaitForLoad)
	{
		Local_FinishZoneLoad();
	}
}

//#########################################################################################################
// TRANSIT Anime
//#########################################################################################################

ULevelSequence* UOmegaZoneSubsystem::GetTransitSequence()
{
	if(ULevelSequence* LevelSequence = Cast<ULevelSequence>(GetMutableDefault<UOmegaSettings>()->TransitSequence.TryLoad()))
	{
		return LevelSequence;
	}
	return nullptr;
}

ULevelSequencePlayer* UOmegaZoneSubsystem::GetTransitSequencePlayer()
{
	if(!LocalSeqPlayer)
	{
		UE_LOG(LogTemp, Warning, TEXT("Initialized SequencePlayer"));
		FMovieSceneSequencePlaybackSettings Settings;
		ULevelSequencePlayer::CreateLevelSequencePlayer(GetWorld(), GetTransitSequence(), Settings, LocalSeqPlayer);
		LocalSeqPlayer->SequencePlayer->OnFinished.AddDynamic(this, &UOmegaZoneSubsystem::Local_OnFinishTransitSequence);
	}
	return LocalSeqPlayer->SequencePlayer;
}

// TASKS -----------------------

void UOmegaZoneSubsystem::Local_OnBeginTransitSequence(bool bForward)
{
	if(!bSequenceTransit_IsPlaying)
	{
		bSequenceTransit_IsPlaying = true;
		bSequenceTransit_IsForward = bForward;
		
		if(GetTransitSequence()) // If a level sequence is VALID
		{
			if(bSequenceTransit_IsForward)
			{
				UE_LOG(LogTemp, Warning, TEXT("ATTEMPT PLAYER TRNASIT SEQUENCE: Forward"));
				GetTransitSequencePlayer()->Play();
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("ATTEMPT PLAYER TRNASIT SEQUENCE: Reverse"));
				//GetTransitSequencePlayer()->RestoreState();
				GetTransitSequencePlayer()->ChangePlaybackDirection();
				GetTransitSequencePlayer()->PlayReverse();
			}
		}
		else // If a level sequence is INVALID
		{
			Local_OnFinishTransitSequence();
		}
	}
}

void UOmegaZoneSubsystem::Local_OnFinishTransitSequence()
{
	//Check if in Level Transit
	if(GamInstSubsys->IsInlevelTransit)
	{
		float LastPos;
		GetWorld()->GetSubsystem<UOmegaBGMSubsystem>()->StopBGM(true,LastPos);
		UE_LOG(LogTemp, Warning, TEXT("Fire Level Tranist: %s"), *IncomingLevelName.ToString());
		UGameplayStatics::OpenLevel(this, IncomingLevelName);
		return;
	}
	if(bSequenceTransit_IsPlaying)
	{
		bSequenceTransit_IsPlaying = false;
		if(bSequenceTransit_IsForward)
		{
			UE_LOG(LogTemp, Warning, TEXT("Sequence Finished: Fade Out"));
			//SEQUENCE FINISHED: Fade Out 
			if(GetTopLoadedZones() && bUnloadPreviousZones)
			{
				Local_IsWaitForLoad = true;
				UnloadZone(GetTopLoadedZones());
			}
			else
			{
				Local_FinishZoneLoad();
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Sequence Finished: Fade In"));
			//SEQUENCE FINISHED: Fade In
			Local_CompleteTransit();
		}
	}
}