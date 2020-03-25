// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "Interactions/UxtGrabTargetComponent.h"
#include "Engine/World.h"
#include "Interactions/UxtInteractionUtils.h"

FVector UUxtGrabPointerDataFunctionLibrary::GetGrabLocation(const FTransform &Transform, const FUxtGrabPointerData &GrabData)
{
	return Transform.TransformPosition(GrabData.LocalGrabPoint.GetLocation());
}

FRotator UUxtGrabPointerDataFunctionLibrary::GetGrabRotation(const FTransform &Transform, const FUxtGrabPointerData &GrabData)
{
	return Transform.TransformRotation(GrabData.LocalGrabPoint.GetRotation()).Rotator();
}

FTransform UUxtGrabPointerDataFunctionLibrary::GetGrabTransform(const FTransform &Transform, const FUxtGrabPointerData &GrabData)
{
	return GrabData.LocalGrabPoint * Transform;
}

FVector UUxtGrabPointerDataFunctionLibrary::GetTargetLocation(const FUxtGrabPointerData &GrabData)
{
	return GrabData.PointerData.Location;
}

FRotator UUxtGrabPointerDataFunctionLibrary::GetTargetRotation(const FUxtGrabPointerData &GrabData)
{
	return GrabData.PointerData.Rotation.Rotator();
}

FTransform UUxtGrabPointerDataFunctionLibrary::GetTargetTransform(const FUxtGrabPointerData &GrabData)
{
	return FTransform(GrabData.PointerData.Rotation, GrabData.PointerData.Location);
}

FVector UUxtGrabPointerDataFunctionLibrary::GetLocationOffset(const FTransform &Transform, const FUxtGrabPointerData &GrabData)
{
	return UUxtGrabPointerDataFunctionLibrary::GetTargetLocation(GrabData) - UUxtGrabPointerDataFunctionLibrary::GetGrabLocation(Transform, GrabData);
}

FRotator UUxtGrabPointerDataFunctionLibrary::GetRotationOffset(const FTransform &Transform, const FUxtGrabPointerData &GrabData)
{
	return (FQuat(UUxtGrabPointerDataFunctionLibrary::GetTargetRotation(GrabData)) * FQuat(UUxtGrabPointerDataFunctionLibrary::GetGrabRotation(Transform, GrabData).GetInverse())).Rotator();
}


UUxtGrabTargetComponent::UUxtGrabTargetComponent()
{
	bTickOnlyWhileGrabbed = true;
}

const TArray<FUxtGrabPointerData> &UUxtGrabTargetComponent::GetGrabPointers() const
{
	return GrabPointers;
}

FVector UUxtGrabTargetComponent::GetGrabPointCentroid(const FTransform &Transform) const
{
	FVector centroid = FVector::ZeroVector;
	for (const FUxtGrabPointerData &GrabData : GrabPointers)
	{
		centroid += UUxtGrabPointerDataFunctionLibrary::GetGrabLocation(Transform, GrabData);
	}
	centroid /= FMath::Max(GrabPointers.Num(), 1);
	return centroid;
}

FVector UUxtGrabTargetComponent::GetTargetCentroid() const
{
	FVector centroid = FVector::ZeroVector;
	for (const FUxtGrabPointerData &GrabData : GrabPointers)
	{
		centroid += UUxtGrabPointerDataFunctionLibrary::GetTargetLocation(GrabData);
	}
	centroid /= FMath::Max(GrabPointers.Num(), 1);
	return centroid;
}

bool UUxtGrabTargetComponent::FindGrabPointerInternal(int32 PointerId, FUxtGrabPointerData const *&OutData, int &OutIndex) const
{
	for (int i = 0; i < GrabPointers.Num(); ++i)
	{
		const FUxtGrabPointerData &GrabData = GrabPointers[i];
		if (GrabData.PointerId == PointerId)
		{
			OutData = &GrabData;
			OutIndex = i;
			return true;
		}
	}

	OutData = nullptr;
	OutIndex = -1;
	return false;
}

void UUxtGrabTargetComponent::FindGrabPointer(int32 PointerId, bool &Success, FUxtGrabPointerData &PointerData, int &Index) const
{
	FUxtGrabPointerData const *pData;
	Success = FindGrabPointerInternal(PointerId, pData, Index);
	if (Success)
	{
		PointerData = *pData;
	}
}

void UUxtGrabTargetComponent::GetPrimaryGrabPointer(bool &Valid, FUxtGrabPointerData &PointerData) const
{
	if (GrabPointers.Num() >= 1)
	{
		PointerData = GrabPointers[0];
		Valid = true;
	}
	else
	{
		Valid = false;
	}
}

void UUxtGrabTargetComponent::GetSecondaryGrabPointer(bool &Valid, FUxtGrabPointerData &PointerData) const
{
	if (GrabPointers.Num() >= 2)
	{
		PointerData = GrabPointers[1];
		Valid = true;
	}
	else
	{
		Valid = false;
	}
}

bool UUxtGrabTargetComponent::GetTickOnlyWhileGrabbed() const
{
	return bTickOnlyWhileGrabbed;
}

void UUxtGrabTargetComponent::SetTickOnlyWhileGrabbed(bool bEnable)
{
	bTickOnlyWhileGrabbed = bEnable;

	if (bEnable)
	{
		UpdateComponentTickEnabled();
	}
	else
	{
		PrimaryComponentTick.SetTickFunctionEnable(true);
	}
}

void UUxtGrabTargetComponent::UpdateComponentTickEnabled()
{
	if (bTickOnlyWhileGrabbed)
	{
		PrimaryComponentTick.SetTickFunctionEnable(GrabPointers.Num() > 0);
	}
}

void UUxtGrabTargetComponent::BeginPlay()
{
	Super::BeginPlay();

	// Initialize component tick
	UpdateComponentTickEnabled();
}

bool UUxtGrabTargetComponent::GetClosestGrabPoint_Implementation(const FVector& Point, FVector& OutPointOnSurface) const
{
	return FUxtInteractionUtils::GetDefaultClosestPoint(GetOwner(), Point, OutPointOnSurface);
}

void UUxtGrabTargetComponent::OnBeginGrab_Implementation(int32 PointerId, const FUxtPointerInteractionData& Data)
{
	FUxtGrabPointerData GrabData;
	GrabData.PointerId = PointerId;
	GrabData.PointerData = Data;
	GrabData.StartTime = GetWorld()->GetTimeSeconds();
	GrabData.LocalGrabPoint = FTransform(Data.Rotation, Data.Location) * GetComponentTransform().Inverse();

	GrabPointers.Add(GrabData);

	OnBeginGrab.Broadcast(this, GrabData);

	UpdateComponentTickEnabled();
}

void UUxtGrabTargetComponent::OnUpdateGrab_Implementation(int32 PointerId, const FUxtPointerInteractionData& Data)
{
	// Update the copy of the pointer data in the grab pointer array
	for (FUxtGrabPointerData& GrabData : GrabPointers)
	{
		if (GrabData.PointerId == PointerId)
		{
			GrabData.PointerData = Data;
		}
	}
}


void UUxtGrabTargetComponent::OnEndGrab_Implementation(int32 PointerId)
{
	int numRemoved = GrabPointers.RemoveAll([this, PointerId](const FUxtGrabPointerData& GrabData)
		{
			if (GrabData.PointerId == PointerId)
			{
				OnEndGrab.Broadcast(this, GrabData);
				return true;
			}
			return false;
		});

	UpdateComponentTickEnabled();
}

void UUxtGrabTargetComponent::ResetLocalGrabPoint(FUxtGrabPointerData &PointerData)
{
	PointerData.LocalGrabPoint = FTransform(PointerData.PointerData.Rotation, PointerData.PointerData.Location) * GetComponentTransform().Inverse();
}
