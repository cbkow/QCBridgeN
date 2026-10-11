// SPDX-License-Identifier: MIT
#include "ndi_lib.h"

#include <Processing.NDI.Lib.h>

#include <cstdlib>
#include <vector>

#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#else
#  include <dlfcn.h>
#endif

namespace {

std::vector<std::string> candidates()
{
    std::vector<std::string> out;
    // The SDK's rule first: the runtime's own environment variable.
    if (const char *dir = std::getenv(NDILIB_REDIST_FOLDER); dir && *dir) {
#if defined(_WIN32)
        out.push_back(std::string(dir) + "\\" + NDILIB_LIBRARY_NAME);
#else
        out.push_back(std::string(dir) + "/" + NDILIB_LIBRARY_NAME);
#endif
    }
#if defined(__APPLE__)
    out.push_back("/usr/local/lib/" NDILIB_LIBRARY_NAME);
    out.push_back("/Library/NDI SDK for Apple/lib/macOS/" NDILIB_LIBRARY_NAME);
    out.push_back("@executable_path/../Frameworks/" NDILIB_LIBRARY_NAME);
#endif
    out.push_back(NDILIB_LIBRARY_NAME);   // the loader's own search
    return out;
}

} // namespace

std::string NdiLib::redistHint()
{
    return std::string("the NDI runtime was not found; set ") + NDILIB_REDIST_FOLDER
         + " or install it from " + NDILIB_REDIST_URL;
}

bool NdiLib::load(std::string *error)
{
    if (m_api) return true;
    for (const std::string &c : candidates()) {
#if defined(_WIN32)
        HMODULE h = ::LoadLibraryA(c.c_str());
        if (!h) continue;
        auto loadFn = reinterpret_cast<const NDIlib_v6_3 *(*)(void)>(::GetProcAddress(h, "NDIlib_v6_load"));
        if (!loadFn) { ::FreeLibrary(h); continue; }
#else
        void *h = ::dlopen(c.c_str(), RTLD_LOCAL | RTLD_LAZY);
        if (!h) continue;
        auto loadFn = reinterpret_cast<const NDIlib_v6_3 *(*)(void)>(::dlsym(h, "NDIlib_v6_load"));
        if (!loadFn) { ::dlclose(h); continue; }
#endif
        const NDIlib_v6_3 *api = loadFn();
        if (!api || !api->initialize()) {
            if (error) *error = "NDI runtime at " + c + " refused to initialize (CPU not supported?)";
#if defined(_WIN32)
            ::FreeLibrary(h);
#else
            ::dlclose(h);
#endif
            return false;
        }
        m_handle = h;
        m_api    = api;
        m_path   = c;
        return true;
    }
    if (error) *error = redistHint();
    return false;
}

void NdiLib::unload()
{
    if (m_api) { m_api->destroy(); m_api = nullptr; }
    if (m_handle) {
#if defined(_WIN32)
        ::FreeLibrary(static_cast<HMODULE>(m_handle));
#else
        ::dlclose(m_handle);
#endif
        m_handle = nullptr;
    }
}

std::string NdiLib::version() const
{
    return m_api && m_api->version ? std::string(m_api->version()) : std::string();
}
