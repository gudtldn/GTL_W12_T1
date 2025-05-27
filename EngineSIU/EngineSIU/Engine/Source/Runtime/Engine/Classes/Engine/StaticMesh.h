#pragma once
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "Components/Material/Material.h"
#include "Define.h"

class UBodySetup;
struct FStaticMeshRenderData;

class UStaticMesh : public UObject
{
    DECLARE_CLASS(UStaticMesh, UObject)

public:
    UStaticMesh();

    virtual UObject* Duplicate(UObject* InOuter) override;

    const TArray<FStaticMaterial*>& GetMaterials() const { return Materials; }
    uint32 GetMaterialIndex(FName MaterialSlotName) const;
    void GetUsedMaterials(TArray<UMaterial*>& OutMaterial) const;
    FStaticMeshRenderData* GetRenderData() const { return RenderData; }

    //ObjectName은 경로까지 포함
    FWString GetOjbectName() const;

    void SetData(FStaticMeshRenderData* InRenderData);

    virtual void SerializeAsset(FArchive& Ar) override;

    [[nodiscard]] UBodySetup* GetBodySetup() const
    {
        return BodySetup;
    }

    void SetBodySetup(UBodySetup* NewBodySetup)
    {
        BodySetup = NewBodySetup;
    }

private:
    FStaticMeshRenderData* RenderData = nullptr;
    TArray<FStaticMaterial*> Materials;

    UPROPERTY(
        EditAnywhere | EditInline | Transient | DuplicateTransient, { .Category = "StaticMesh" },
        UBodySetup*, BodySetup, = nullptr;
    )
};
