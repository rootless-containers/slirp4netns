/* SPDX-License-Identifier: BSD-3-Clause */
#include "libslirp.h"
#include "util.h"

const char *
slirp_version_string(void)
{
    return stringify(SLIRP_MAJOR_VERSION) "."
        stringify(SLIRP_MINOR_VERSION) "."
        stringify(SLIRP_MICRO_VERSION);
}
