# Kernel Internal Headers

This directory contains internal facilities and macros
used exclusively within the kernel.

These headers define:

- Assertion (`assert.h`)
- Fatal crash (`panic.h`)
- Debug logging (`debug.h`)

Do not include these from user-facing or library-level code.
