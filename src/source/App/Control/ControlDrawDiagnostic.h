#pragma once

#include "App/Control/ControlProtocol.h"

#include <string>

namespace App::Control::Diagnostics
{

[[nodiscard]] std::string Draw(const Request& request);

} // namespace App::Control::Diagnostics
