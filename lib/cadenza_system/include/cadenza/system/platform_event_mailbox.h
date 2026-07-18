#pragma once

#include <cstddef>

#include "cadenza/system/spsc_mailbox.h"
#include "cadenza/system/system_service_host.h"

namespace cadenza::system {

template <std::size_t Capacity>
using PlatformEventMailbox = SpscMailbox<PlatformEvent, Capacity>;

using PlatformEventMailboxDiagnostics = SpscMailboxDiagnostics;

}  // namespace cadenza::system
