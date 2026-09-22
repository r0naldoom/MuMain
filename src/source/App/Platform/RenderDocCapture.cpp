#include "stdafx.h"
#include "App/Platform/RenderDocCapture.h"

#if defined(__linux__)
#include <dlfcn.h>
#include <memory>

#include "renderdoc_app.h"

namespace
{
using RenderDocLibrary = std::unique_ptr<void, decltype(&dlclose)>;

[[nodiscard]] RENDERDOC_API_1_0_0* LoadRenderDocApi()
{
    RenderDocLibrary library(dlopen("librenderdoc.so", RTLD_NOW | RTLD_NOLOAD), &dlclose);
    if (!library)
    {
        return nullptr;
    }

    const auto getApi = reinterpret_cast<pRENDERDOC_GetAPI>(dlsym(library.get(), "RENDERDOC_GetAPI"));
    if (getApi == nullptr)
    {
        return nullptr;
    }

    void* rawApi = nullptr;
    if (getApi(eRENDERDOC_API_Version_1_0_0, &rawApi) != 1 || rawApi == nullptr)
    {
        return nullptr;
    }

    return static_cast<RENDERDOC_API_1_0_0*>(rawApi);
}
} // namespace
#endif

bool App::Platform::RenderDoc::TriggerCapture()
{
#if defined(__linux__)
    static RENDERDOC_API_1_0_0* const api = LoadRenderDocApi();
    if (api == nullptr)
    {
        return false;
    }

    api->TriggerCapture();
    return true;
#else
    return false;
#endif
}
