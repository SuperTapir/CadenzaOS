// This is an intentionally failing characterization probe. The current
// top-level "core" headers still include Arduino/TFT types. CTest marks the
// failed build as the expected legacy state. Platform decoupling must move
// these contracts into cadenza_core and invert this probe to require success.
#include "app_runtime.h"
#include "apps.h"
#include "display_profile.h"
#include "input.h"
#include "mono_canvas.h"

int main() { return 0; }
