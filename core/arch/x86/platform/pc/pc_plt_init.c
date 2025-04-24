#include <frog/types.h>
#include <asm/i8253.h>   // PIT
#include <asm/i8259a.h>  // PIC
#include <frog/compiler.h>

void __noreturn platform_init(void) {
        init_8259A();
        init_PIT8253();
}
