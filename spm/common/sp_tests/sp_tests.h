/*
 * Copyright (c) 2017-2021, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef CACTUS_TESTS_H
#define CACTUS_TESTS_H

#include <spm_common.h>

/*
 * Test functions
 */

void ffa_tests(struct mailbox_buffers *mb, ffa_id_t my_ffa_id);

#endif /* CACTUS_TESTS_H */
