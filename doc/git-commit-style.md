# Git Commit Style Guide for Frog OS

This document defines the recommended commit message style for the Frog operating system project. The goal is to make the commit history readable, traceable by subsystem, and friendly to cherry-pick and refactor workflows.

`git config commit.template .gitmessage`
run this frist at root dir of frog/

---

## 🧱 Commit Message Format

```text
[type](subsystem): summary of what you did

WHY:
- Why was this change made? What context or problem led to it?

WHAT:
- What specific changes were introduced?
- Which modules/functions/files were affected?
- Is there any interface or behavior change?

HOW TESTED:
- Can the system still boot?
- Did you verify output (e.g. DEBUG, VGA)?
- Did you test edge cases or downstream components?

Tags: [core], [fs], [printk], [stage1]
```

Each section should be clear but concise. Feel free to use bullet points in the WHAT section for clarity.

---

## ✅ Commit Type Keywords

| Type       | Meaning                                      |
|------------|----------------------------------------------|
| `feat`     | New functionality or module                  |
| `fix`      | Bug fix                                      |
| `refactor` | Code structure change, no logic change       |
| `style`    | Naming, formatting, comments only            |
| `test`     | Adding or changing tests                     |
| `doc`      | Documentation or inline comments             |
| `build`    | Build system or linker script changes        |
| `debug`    | Temporary debugging commits (rebaseable)     |
| `chore`    |    etc things |

---

## 📦 Subsystem Prefix Examples

| Subsystem    | Description                      |
|--------------|----------------------------------|
| `core`       | Kernel base (entry, main loop)   |
| `fs`         | File system                      |
| `block`      | Block device & IDE driver        |
| `vga`        | VGA console output               |
| `printk`     | Logging infrastructure           |
| `poudland`   | Compositor / GUI layer           |
| `mm`         | Memory management                |
| `init`       | Kernel and boot init process     |

---

## 🧩 Example

```text
feat(printk): add structured kernel logging macros

WHY:
- Needed consistent debugging output with severity levels
- Prepare for future serial port integration

WHAT:
- Added printk.c and debug.h
- Macros: DEBUG, INFO, WARN, PANIC, ASSERT
- Output goes to VGA console for now

HOW TESTED:
- System boots to UkiMain
- DEBUG messages are visible on screen

tags: [core, printk, vga, stage1]
```
---

## 🧠 Best Practices

- Each commit must be logically self-contained (even if part of a series)
- If splitting refactor and feature logic, always ensure intermediate commits still compile
- Use tags:` to mark module boundaries for easier filtering (e.g., `git log --grep="[fs]"`)
- Prefer clarity over cleverness — future you will thank you

---

Happy hacking 🐸


