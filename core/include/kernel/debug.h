#ifndef __OS_DEBUG_H
#define __OS_DEBUG_H
#include <frog/printk.h>

#ifdef CONFIG_DEBUG
#define DEBUG(fmt, ...) \
    printk("[DEBUG] %s:%d %s(): " fmt "\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__)

#define INFO(fmt, ...) \
    printk("[INFO] " fmt "\n", ##__VA_ARGS__)

#define WARN(fmt, ...) \
    printk("[WARN] %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)

#else

#define DEBUG(fmt, ...) ((void)0)
#define INFO(fmt, ...) ((void)0)
#define WARN(fmt, ...) ((void)0)

#endif


#endif
