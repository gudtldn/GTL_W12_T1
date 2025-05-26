#include "ShapeElem.h"

void FKShapeElem::SetRelativeTransform(const FTransform& InTransform)
{
    RelativeLocation = InTransform.GetTranslation();
    RelativeRotation = InTransform.GetRotation().GetNormalized().Rotator();
    RelativeScale3D = InTransform.GetScale3D();
}

FTransform FKShapeElem::GetRelativeTransform() const
{
    return FTransform{ RelativeRotation, RelativeLocation, RelativeScale3D };
}
