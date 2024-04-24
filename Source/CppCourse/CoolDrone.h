// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CoolDrone.generated.h"

class ATurretProjectile;
class UBoxComponent;
class USphereComponent;
class UCoolHealthComponent;

UENUM()
enum class EDroneState
{
	Idle,
	Chase,
	Aggressive
};

UCLASS()
class CPPCOURSE_API ACoolDrone : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ACoolDrone();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

private:
	UPROPERTY(VisibleAnywhere)
	UBoxComponent* Root = nullptr;

	UPROPERTY(VisibleAnywhere)
	USphereComponent* VisionSphere = nullptr;

	UPROPERTY(VisibleAnywhere)
	USceneComponent* ProjectileSpawnPoint = nullptr;

	UPROPERTY(VisibleAnywhere)
	UCoolHealthComponent* HealthComponent = nullptr;

	UPROPERTY(EditAnywhere)
	TArray<AActor*> ControlPoints;

	UPROPERTY(EditAnywhere)
	TSubclassOf<ATurretProjectile> ProjectileClass;

	UPROPERTY(EditAnywhere)
	float MovementSpeed = 100.f;

	UPROPERTY(EditAnywhere)
	float TurnRate = 5.f;

	UPROPERTY(EditAnywhere)
	float TargetPositionTolerance = 5.f;

	UPROPERTY(EditAnywhere, meta = (ClampMin = 0, ClampMax = 1))
	float AimThreshold = 0.98f;

	UPROPERTY(EditAnywhere)
	float FireRate = 2.f;

	UPROPERTY(VisibleAnywhere)
	AActor* Target = nullptr;

	UPROPERTY(VisibleAnywhere)
	EDroneState CurrentState = EDroneState::Idle;

	TMap<EDroneState, void (ACoolDrone::*)(float DeltaTime)> StateHandlers =
	{
		{EDroneState::Idle, &ACoolDrone::HandleIdle},
		{EDroneState::Chase, &ACoolDrone::HandleChase},
		{EDroneState::Aggressive, &ACoolDrone::HandleAggressive}
	};

	AActor* CurrentControlPoint = nullptr;

	FVector LastKnownTargetPosition;

	float LastFireTime = 0.f;

	void HandleIdle(float DeltaTime);

	void HandleChase(float DeltaTime);

	void HandleAggressive(float DeltaTime);

	void RotateTowards(const FVector& Position, float DeltaTime);

	void MoveTowards(const FVector& Position, float DeltaTime);

	void SelectRandomControlPoint();

	void TryFire();

	bool TargetIsVisible();

	UFUNCTION()
	void HandleVisionBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult
	);

	UFUNCTION()
	void HandleVisionEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex
	);

	UFUNCTION()
	void HandleDeath();
};
