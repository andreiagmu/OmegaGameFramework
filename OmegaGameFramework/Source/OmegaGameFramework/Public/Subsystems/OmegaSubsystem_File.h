// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/EngineSubsystem.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Sound/SoundBase.h"
#include "OmegaSubsystem_File.generated.h"


USTRUCT()
struct FOmegaFileObjectList
{
	GENERATED_BODY()
	UPROPERTY()
	TMap<FString, UObject*> file_objects;
};


UCLASS()
class OMEGAGAMEFRAMEWORK_API UOmegaFileSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	
	UFUNCTION()
	TArray<FString> GetFilesOfExtension(FString path, FString extension);

	UPROPERTY()
	FString over_path="ovr";
	UPROPERTY()
	FString override_images_path="images";
	UPROPERTY()
	FString override_audio_path="audio";

	UPROPERTY()
	TMap<UClass*, FOmegaFileObjectList> imported_overrides;
	
public:
	UFUNCTION(BlueprintPure,Category="Omega|File")
	UOmegaFileManagerSettings* GetFileManagerSettings() const;
	
	UFUNCTION(BlueprintCallable,Category="Omega|File")
	void ImportFileAsOverrideAsset(const FString& path);
	
	UFUNCTION(BlueprintCallable,Category="Omega|File")
	void ImportFileAsOverrideAsset_List(const FString& path);
	
	UFUNCTION(BlueprintPure, Category="Omega|File")
	FString GetOverrideDirectory() const;

	UFUNCTION(BlueprintCallable,Category="Omega|File",meta=(DeterminesOutputType="Class"))
	UObject* GetOverrideObject(FString name, UClass* Class) const;
	
	
};

UCLASS()
class OMEGAGAMEFRAMEWORK_API UOmegaFileFunctions : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	
};



UCLASS(Blueprintable,BlueprintType,EditInlineNew)
class OMEGAGAMEFRAMEWORK_API UOmegaFileImportScript : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly,Category="FileImport")
	UClass* ImportClass;
	UPROPERTY(EditDefaultsOnly,Category="FileImport")
	TArray<FString> ValidExtensions;
	
	UFUNCTION(BlueprintImplementableEvent,Category="FileImport")
	UObject* ImportAsObject(const FString& path, const FString& name, const FString& extension);
};




UCLASS()
class OMEGAGAMEFRAMEWORK_API UOmegaFileManagerSettings : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Instanced,Category="FileImporter")
	TArray<UOmegaFileImportScript*> ImportScripts;
	
};
