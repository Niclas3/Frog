#ifndef POUDLAND_P0_TEST_H
#define POUDLAND_P0_TEST_H

#include <frog/types.h>

void poudland_p0_test_report(uint_32 id, bool passed);
int_32 poudland_p0_test_sync(uint_32 command);
void poudland_p0_test_finish(void) __attribute__((noreturn));

#endif
