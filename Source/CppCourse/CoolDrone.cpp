// Fill out your copyright notice in the Description page of Project Settings.

#include "CoolDrone.h"
#include "Components/SphereComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Character.h"
#include "TurretProjectile.h"
#include "CoolHealthComponent.h"

// Sets default values
ACoolDrone::ACoolDrone()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<UBoxComponent>("Root");
	Root->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Root->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Block);
	Root->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Overlap);
	SetRootComponent(Root);

	VisionSphere = CreateDefaultSubobject<USphereComponent>("VisionSphere");
	VisionSphere->SetupAttachment(Root);
	VisionSphere->SetSphereRadius(1000.f);
	VisionSphere->OnComponentBeginOverlap.AddDynamic(this, &ACoolDrone::HandleVisionBeginOverlap);
	VisionSphere->OnComponentEndOverlap.AddDynamic(this, &ACoolDrone::HandleVisionEndOverlap);

	ProjectileSpawnPoint = CreateDefaultSubobject<USceneComponent>("ProjectileSpawnPoint");
	ProjectileSpawnPoint->SetupAttachment(Root);

	HealthComponent = CreateDefaultSubobject<UCoolHealthComponent>("Health");
	HealthComponent->OnDeath.AddDynamic(this, &ACoolDrone::HandleDeath);

	StateHandlers.Add(EDroneState::Idle, &ACoolDrone::HandleIdle);
	StateHandlers.Add(EDroneState::Chase, &ACoolDrone::HandleChase);
	StateHandlers.Add(EDroneState::Aggressive, &ACoolDrone::HandleAggressive);
}

// Called when the game starts or when spawned
void ACoolDrone::BeginPlay()
{
	Super::BeginPlay();

	SelectRandomControlPoint();
}

// Called every frame
void ACoolDrone::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	(this->*StateHandlers[CurrentState])(DeltaTime);
}

void ACoolDrone::HandleIdle(float DeltaTime)
{
	FVector ControlPointLocation = CurrentControlPoint->GetActorLocation();

	RotateTowards(ControlPointLocation, DeltaTime, true);
	MoveTowards(ControlPointLocation, DeltaTime);

	if (FVector::Dist(GetActorLocation(), CurrentControlPoint->GetActorLocation()) < TargetPositionTolerance)
	{
		SelectRandomControlPoint();
	}

	if (Target)
	{
		if (TargetIsVisible())
		{
			CurrentState = EDroneState::Aggressive;
		}
	}
}

void ACoolDrone::HandleChase(float DeltaTime)
{
	FVector IgnoreZPos = FVector(LastKnownTargetPosition.X, LastKnownTargetPosition.Y, GetActorLocation().Z);

	MoveTowards(IgnoreZPos, DeltaTime);
	RotateTowards(IgnoreZPos, DeltaTime);

	if (FVector::Dist(GetActorLocation(), IgnoreZPos) < TargetPositionTolerance)
	{
		CurrentState = EDroneState::Idle;
	}

	if (Target)
	{
		if (TargetIsVisible())
		{
			CurrentState = EDroneState::Aggressive;
		}
	}
}

void ACoolDrone::HandleAggressive(float DeltaTime)
{
	if (!Target)
	{
		CurrentState = EDroneState::Idle;
		return;
	}

	if(!TargetIsVisible())
	{
		LastKnownTargetPosition = Target->GetActorLocation();
		CurrentState = EDroneState::Chase;
		return;
	}

	RotateTowards(Target->GetTargetLocation(), DeltaTime);

	TryFire();
}

void ACoolDrone::RotateTowards(const FVector& Position, float DeltaTime, bool DisablePitch)
{
	FVector Direction = Position - GetActorLocation();
	if (DisablePitch) Direction.Z = 0.f;
	Direction.Normalize();
	FRotator TargetRotation = FRotationMatrix::MakeFromX(Direction).Rotator();
	FRotator CurrentRotation = GetActorRotation();
	FRotator NewRotation = FMath::RInterpTo(CurrentRotation, TargetRotation, DeltaTime, TurnRate);
	SetActorRotation(NewRotation);
}

void ACoolDrone::MoveTowards(const FVector& Position, float DeltaTime, bool DisableY)
{
	FVector Direction = Position - GetActorLocation();
	if (DisableY) Direction.Z = 0.f;
	Direction.Normalize();

	FHitResult SweepHit;
	AddActorWorldOffset(Direction * MovementSpeed * DeltaTime, true, &SweepHit);

	if (SweepHit.bBlockingHit)
	{
		UE_LOG(LogTemp, Warning, TEXT("Hit actor: %s"), *SweepHit.GetActor()->GetName());

		if (CurrentState == EDroneState::Chase)
		{
			CurrentState = EDroneState::Idle;
		}
		else if (CurrentState == EDroneState::Idle)
		{
			SelectRandomControlPoint();
		}
	}

}

void ACoolDrone::SelectRandomControlPoint()
{
	TArray<AActor*> FilteredControlPoints = ControlPoints.FilterByPredicate(
		[this](AActor* ControlPoint)
		{
			if (ControlPoint == CurrentControlPoint) return false;

			FHitResult HitResult;
			FCollisionQueryParams CollisionQueryParams;
			CollisionQueryParams.AddIgnoredActor(this);
			CollisionQueryParams.AddIgnoredActor(CurrentControlPoint);
			GetWorld()->LineTraceSingleByChannel(
				HitResult,
				GetActorLocation(),
				ControlPoint->GetActorLocation(),
				ECollisionChannel::ECC_PhysicsBody,
				CollisionQueryParams
			);

			return !HitResult.bBlockingHit;
		}
	);

	if (FilteredControlPoints.Num() == 0) return;

	CurrentControlPoint = FilteredControlPoints[FMath::RandRange(0, FilteredControlPoints.Num() - 1)];
}

void ACoolDrone::TryFire()
{
	if (!ProjectileSpawnPoint) return;

	if (GetWorld()->TimeSince(LastFireTime) < 1.f / FireRate) return;

	float AimDotProduct = FVector::DotProduct(
		GetActorForwardVector(),
		(Target->GetActorLocation() - GetActorLocation()).GetSafeNormal()
	);
	if (AimDotProduct < AimThreshold) return;

	GetWorld()->SpawnActor<ATurretProjectile>(
		ProjectileClass,
		ProjectileSpawnPoint->GetComponentLocation(),
		ProjectileSpawnPoint->GetComponentRotation()
	);

	LastFireTime = GetWorld()->GetTimeSeconds();
}

bool ACoolDrone::TargetIsVisible()
{
	if (!Target) return false;

	FHitResult HitResult;
	FCollisionQueryParams CollisionQueryParams;
	CollisionQueryParams.AddIgnoredActor(this);
	CollisionQueryParams.AddIgnoredActor(Target);
	GetWorld()->LineTraceSingleByChannel(
		HitResult,
		GetActorLocation(),
		Target->GetActorLocation(),
		ECollisionChannel::ECC_Visibility,
		CollisionQueryParams
	);

	return !HitResult.bBlockingHit;
}

void ACoolDrone::HandleVisionBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult
)
{
	if (OtherActor->IsA<ACharacter>())
	{
		Target = OtherActor;
	}
}

void ACoolDrone::HandleVisionEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex
)
{
	if (OtherActor == Target)
	{
		LastKnownTargetPosition = OtherActor->GetActorLocation();

		Target = nullptr;

		CurrentState = EDroneState::Chase;
	}
}

void ACoolDrone::HandleDeath()
{
	Destroy();
}