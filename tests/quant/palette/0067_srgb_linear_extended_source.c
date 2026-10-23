/* SPDX-License-Identifier: MIT */
#include "srgb_contract_common.h"
int
test_srgb_0067(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return srgb_encoder_contract(1);
}
