#include "Render/Renderer/DrawFilter.h"

namespace Render
{
bool DrawFilter::Matches(const DrawMetadata& draw) const
{
    if (!enabled)
    {
        return false;
    }
    if (hasSubmittedOrdinal &&
        (draw.submittedOrdinal < firstSubmittedOrdinal || draw.submittedOrdinal > lastSubmittedOrdinal))
    {
        return false;
    }
    if (hasTextureId && draw.textureId != textureId)
    {
        return false;
    }
    if (hasTextureSize && (draw.textureWidth != textureWidth || draw.textureHeight != textureHeight))
    {
        return false;
    }
    return !hasBlend || draw.blendEnabled == blendEnabled;
}
} // namespace Render
