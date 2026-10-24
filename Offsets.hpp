#pragma once

#include <cstdint>


namespace UEOffsets
{

namespace Config
{
    constexpr bool isUsingCasePreservingName = false;
    constexpr bool IsUsingFNamePool = true;
    constexpr bool isUsingOutlineNumberName = false;
}

namespace FName
{
    constexpr uintptr_t ComparisonIndex = 0x0;
    constexpr uintptr_t Number = 0x4;
    constexpr uintptr_t DisplayIndex = 0x0;
    constexpr uintptr_t Size = 0x8;
}

namespace FNameEntry
{
    constexpr uintptr_t Index = 0x0;
    constexpr uintptr_t Name = 0x0;
}

namespace FNamePool
{
    constexpr int32_t Stride = 2;
    constexpr int32_t BlocksBit = 18;
    constexpr uintptr_t BlocksOff = 0x38;
}

namespace FNamePoolEntry
{
    constexpr uintptr_t Header = 0x0;
}

namespace FUObjectArray
{
    constexpr uintptr_t ObjObjects = 0x10;
}

namespace TUObjectArray
{
    constexpr uintptr_t Objects = 0x10;
    constexpr uintptr_t NumElements = 0x4;
    constexpr uintptr_t NumElementsPerChunk = 0x10000;
}

namespace FUObjectItem
{
    constexpr uintptr_t Object = 0x0;
    constexpr uintptr_t Size = 0x18;
}

namespace UObject
{
    constexpr uintptr_t ObjectFlags = 0x18;
    constexpr uintptr_t InternalIndex = 0x24;
    constexpr uintptr_t ClassPrivate = 0x8;
    constexpr uintptr_t NamePrivate = 0x1c;
    constexpr uintptr_t OuterPrivate = 0x10;
}

namespace UField
{
    constexpr uintptr_t Next = 0x28;
}

namespace UEnum
{
    constexpr uintptr_t Names = 0x40;
}

namespace UStruct
{
    constexpr uintptr_t SuperStruct = 0x40;
    constexpr uintptr_t Children = 0x50;
    constexpr uintptr_t ChildProperties = 0x68;
    constexpr uintptr_t PropertiesSize = 0x3c;
}

namespace UFunction
{
    constexpr uintptr_t EFunctionFlags = 0xb8;
    constexpr uintptr_t NumParams = 0xb0;
    constexpr uintptr_t ParamSize = 0xb2;
    constexpr uintptr_t Func = 0xd8;
}

namespace UProperty
{
    constexpr uintptr_t ArrayDim = 0x0;
    constexpr uintptr_t ElementSize = 0x0;
    constexpr uintptr_t PropertyFlags = 0x0;
    constexpr uintptr_t Offset_Internal = 0x0;
    constexpr uintptr_t Size = 0x0;
}

namespace FField
{
    constexpr uintptr_t ClassPrivate = 0x20;
    constexpr uintptr_t Next = 0x18;
    constexpr uintptr_t NamePrivate = 0x28;
    constexpr uintptr_t FlagsPrivate = 0x8;
}

namespace FProperty
{
    constexpr uintptr_t ArrayDim = 0x38;
    constexpr uintptr_t ElementSize = 0x3c;
    constexpr uintptr_t PropertyFlags = 0x40;
    constexpr uintptr_t Offset_Internal = 0x4c;
    constexpr uintptr_t Size = 0x78;
}


}

namespace UEPointers
{
    constexpr uintptr_t Names = 0x18bacd80;
    constexpr uintptr_t UObjectArray = 0x18bca1a0;
    constexpr uintptr_t ObjObjects = 0x18bca1b0;
    constexpr uintptr_t Engine = 0x0;
    constexpr uintptr_t World = 0x0;
}

// ---------------------------------------------------------------------------
// Delta Force 1.14.0.772 — offset build-specific (Track A).
// Satu-satunya sumber angka offset untuk core.cpp & tool. Update di sini saja.
// Format: RVA relative terhadap base image (g_base), tag pointer: raw & 0xFFFFFFFFFFFF.
// ---------------------------------------------------------------------------
namespace DF
{
namespace Globals
{
    constexpr uintptr_t GWorld      = 0x1D9E4E78;   // UWorld** (plain)
    constexpr uintptr_t GWorldMirror = 0x1D9E6E78;  // UWorld** (encrypted mirror)
    constexpr uintptr_t GNamesBlocks = 0x1E5A7788;  // FNamePool blocks
}

namespace UObject_
{
    constexpr uintptr_t ClassPrivate = 0x08;
    constexpr uintptr_t NamePrivate  = 0x1C;
}

namespace UWorld_
{
    constexpr uintptr_t PersistentLevel = 0xF8;
    constexpr uintptr_t OwningGameInstance = 0x190;
    constexpr uintptr_t Levels          = 0x158;    // TArray<ULevel*>
}

namespace ULevel_
{
    constexpr uintptr_t Actors = 0x98;              // TArray<AActor*>; count at +8
}

namespace UGameInstance_
{
    constexpr uintptr_t LocalPlayers = 0x38;        // TArray<ULocalPlayer*>
}

namespace ULocalPlayer_
{
    constexpr uintptr_t PlayerController = 0x30;
}

namespace UPlayerController_
{
    constexpr uintptr_t AcknowledgedPawn = 0x3F0;
    constexpr uintptr_t PlayerCameraManager = 0x408;
    constexpr uintptr_t ControlRotation = 0x3D8;    // FRotator (pitch,yaw,roll)
    constexpr uintptr_t RotationInput   = 0x570;    // FRotator — aim path (stealth)
}

namespace AActor_
{
    constexpr uintptr_t RootComponent = 0x180;
    constexpr uintptr_t PlayerState   = 0x390;
}

namespace USceneComponent_
{
    constexpr uintptr_t RelativeLocation = 0x168;
    constexpr uintptr_t ComponentToWorld = 0x210;   // quat @+0, trans @+0x10
}

namespace ACharacter_
{
    constexpr uintptr_t Mesh = 0x3D0;               // USkeletalMeshComponent*
}

namespace USkeletalMeshComponent_
{
    constexpr uintptr_t BoneArray = 0x760;          // ptr; count at +8
    constexpr uintptr_t BoneStride = 0x30;
    constexpr uintptr_t BoneLocalPos = 0x10;        // inside bone entry
}

namespace APlayerState_
{
    constexpr uintptr_t PlayerName = 0x478;         // FString
    constexpr uintptr_t Camp       = 0x660;         // verified V15 (faction)
    constexpr uintptr_t TeamId     = 0x66C;
    constexpr uintptr_t CampAlt    = 0x670;         // alt/lobby layout
}

namespace UCameraManager_
{
    constexpr uintptr_t CameraCacheCandidate = 0x30870; // POV candidate (unverified)
    constexpr uintptr_t PovA = 0x2C5E0;
    constexpr uintptr_t PovB = 0x2DEC0;
    constexpr uintptr_t PovC = 0x2DEF0;
    constexpr uintptr_t PovD = 0x2F310;
}

// tag helper (UE5 tagged pointer)
constexpr uint64_t PtrTagMask = 0xFFFFFFFFFFFFULL;
} // namespace DF
