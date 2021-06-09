// Copyright 2021, Alex Gulikers, 7 Minutes Dead.
// Although these are mostly notes for reference to myself, so you are welcome to pick it apart!

/* https://stackoverflow.com/questions/12934213/how-to-find-out-geometric-median
 * How to find the middle point between two vectors.
 *  (End - Start) = The direction.
 *  (End - Start) / 2 = In between the two vectors.
*/

//

#include "MagneticBalls.h"

#include <activation.h>

#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "PhysicsEngine/PhysicsHandleComponent.h"


// Sets default values for this component's properties.
UMagneticBalls::UMagneticBalls()
{
    // Set this component to be initialized when the game starts, and to be ticked every frame.
    // You can turn these features off to improve performance if you don't need them.
    PrimaryComponentTick.bCanEverTick = true;
    // ...
}


// Called when the game starts.
void UMagneticBalls::BeginPlay()
{
    Super::BeginPlay();

    PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
    PlayerPhysicsHandle = PlayerPawn->FindComponentByClass<UPhysicsHandleComponent>();

    if (!PlayerPhysicsHandle) {
        UE_LOG(LogTemp, Error, TEXT("Player controller/pawn needs a PhysicsHandle Component! Add in editor."));
    }

    GetPhysicsHandle();
    // TODO:
    //  This should be run *once* by some other script,
    //  not every time for every instance of MagneticBalls.
    BallsInLevel = GetAllMagneticBalls();

    // Temporary initial setup before frame.
    CurrentPosition = GetOwner()->GetActorLocation();
    SetDestination();
}


// Called every frame.
void UMagneticBalls::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // All of this is to ensure we don't do access violations on null pointers, like when
    // the player isn't holding anything.
    if (PlayerHoldingItem()) {
        FString GrabbedObjectName = PlayerPhysicsHandle->GrabbedComponent->GetOwner()->GetName();

        if (GrabbedObjectName == GetOwner()->GetName()) {
            BallPhysicsHandle->ReleaseComponent();
        }
    }

    CurrentPosition = GetOwner()->GetActorLocation();
    SetDestination();

    if (EnableDebugView) {
        FRotator TargetRotation;
        BallPhysicsHandle->GetTargetLocationAndRotation(OUT Destination, OUT TargetRotation);
        DrawDebugLine(
            GetWorld(),              // InWorld.
            CurrentPosition,         // LineStart.
            Destination,             // LineEnd.
            DebugLineColor,          // Color.
            false,                   // PersistentLines.
            0.1,                     // LifeTime.
            0,                       // DepthPriority
            1                        // Thickness.
        );

    }
}

// -----------------------------------------------------------------------------
void UMagneticBalls::GetPhysicsHandle()
{
    BallPhysicsHandle = GetOwner()->FindComponentByClass<UPhysicsHandleComponent>();

    if (!BallPhysicsHandle) {
        UE_LOG(LogTemp, Error,
               TEXT("%s has no Physics Handle Component! Needed by MagneticBalls component."),
               *GetOwner()->GetName()
        );
    }
}

// -----------------------------------------------------------------------------
/// Find the two closest balls to this actor. Return result.
TArray<AActor*> UMagneticBalls::FindClosestBalls(TArray<FBallDistances> ListOfBalls)
{
    FBallDistances Closest;
    FBallDistances SecondClosest;

    int32 Count = 0;

    TArray<AActor*> Results;
    bool FirstCheck = true;

    for (FBallDistances Ball: ListOfBalls) {
        Count += 1;

        if (FirstCheck) {
            Closest.Ball = Ball.Ball;
            Closest.Distance = Ball.Distance;
            FirstCheck = false;
        }
        else if (Ball.Distance < Closest.Distance) {
            // Push 1st down to 2nd, if it's not itself.
            SecondClosest.Ball = Closest.Ball;
            // Push newest up to 1st.
            Closest.Ball = Ball.Ball;
            Closest.Distance = Ball.Distance;
        }
    }

    // Order matters I think? We'll be calculating the midpoint vector between the two.
    Results.Push(Closest.Ball);
    Results.Push(SecondClosest.Ball);

    return Results;

}

// -----------------------------------------------------------------------------
/// Retrieve all actors whose name starts with "MagneticBall". Return TArray.
/// \n Not very fool proof, but it works for now.
TArray<AActor*> UMagneticBalls::GetAllMagneticBalls()
{
    const uint32 ThisObjectID = GetOwner()->GetUniqueID();
    TArray<AActor*> Balls;

    // Find all objects with UMagneticBalls component. Add them to our array.
    for (TActorIterator<AActor> Actor(GetWorld()); Actor; ++Actor) {

        // StaticMeshes are sliding into this list causing crashes.
        // If "AActor" has picked up something like StaticMesh, then GetOwner won't be null.
        // TODO: Replace this with a better solution.
        if (Actor->GetName().StartsWith("MagneticBall") && Actor->GetOwner() == nullptr) {
            // Skip itself to avoid marking itself as closest.
            if (Actor->GetUniqueID() == ThisObjectID) {
                continue;
            }
            // Make sure we add the pointer to the list, not the actual object.
            UE_LOG(LogTemp, Warning, TEXT("ADDING TO LIST: %s"), *Actor->GetName());
            Balls.Add(*Actor);
        }
    }

    if (Balls.Num() == 0) {
        UE_LOG(LogTemp, Error, TEXT("Unable to find any balls. Did you name them MagneticBall?"));
    }

    return Balls;
}

// -----------------------------------------------------------------------------
/// Calculate distances to all balls in level, and pair those distances up with each ball Actor pointer. \n\n
/// Return resulting TArray of FBallDistances (custom UStruct in header).
TArray<FBallDistances> UMagneticBalls::GetBallDistancePairs()
{
    // Make Array where we can store each ball pointer and its distance.
    TArray<FBallDistances> Dict;

    for (AActor* Ball : BallsInLevel) {
        if (Ball) {
            const float DistanceToBall = GetOwner()->GetDistanceTo(Ball);

            FBallDistances ThisBall;
            ThisBall.Ball = Ball;
            ThisBall.Distance = DistanceToBall;
            Dict.Add(ThisBall);
        }
        else {
            UE_LOG(LogTemp, Error, TEXT("Encountered null pointer. Expected pointer to MagneticBall actor."));
        }
    }

    return Dict;
}

// -----------------------------------------------------------------------------
/// Set destination based on two closest balls.
void UMagneticBalls::SetDestination()
{
    BallsAndDistances = GetBallDistancePairs();
    ClosestBalls = FindClosestBalls(BallsAndDistances);

    UStaticMeshComponent* ClosestBallMesh = ClosestBalls[0]->FindComponentByClass<UStaticMeshComponent>();
    FVector ClosestBallLocation = ClosestBalls[0]->GetActorLocation();
    BallPhysicsHandle->GrabComponentAtLocation(ClosestBallMesh, NAME_None, ClosestBallLocation);
    // TODO: Causing some weird but interesting configurations.
    BallPhysicsHandle->SetTargetLocation(CurrentPosition - ClosestBallLocation / 2);

}

// -----------------------------------------------------------------------------
/// If the player is currently grabbing a component, then it's safe to interact with said component.
/// \n\n This is to avoid null pointer crashes when the player is not holding an object.
bool UMagneticBalls::PlayerHoldingItem()
{
    if (PlayerPhysicsHandle->GrabbedComponent) {
        return true;
    }
    return false;
}
