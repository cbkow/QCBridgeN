// SPDX-License-Identifier: MIT
// The NDI® runtime, loaded at run time (QCBridgeN/PLAN.md O10, P3).
//
// Nothing from the SDK is linked or shipped by this repository: the headers
// come from the installed SDK at build time (never committed — they are
// confidential under the SDK licence), and the library is found at run
// time where the NDI runtime installer or the SDK put it. If it is not
// there, the sink says so and points at the runtime download, the way the
// SDK documents it.

#pragma once

#include <string>

struct NDIlib_v6_3;   // the SDK's function table (Processing.NDI.DynamicLoad.h)

class NdiLib {
public:
    // Loads the library and calls initialize(). Returns false with `error`
    // set when the runtime is absent or refuses to start.
    bool load(std::string *error);
    void unload();
    ~NdiLib() { unload(); }

    const NDIlib_v6_3 *api()  const { return m_api; }
    const std::string &path() const { return m_path; }
    std::string version() const;

    // Where the SDK says the runtime lives, for the error message.
    static std::string redistHint();

private:
    void              *m_handle = nullptr;
    const NDIlib_v6_3 *m_api    = nullptr;
    std::string        m_path;
};
