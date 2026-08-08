# Frog Hardware Targets

This document records the real machines Frog intends to support. QEMU remains the automated development platform, but its defaults must not silently become hardware requirements.

## Target 1: M6117 386SX System

Status: Planned first real-hardware target

The following configuration was supplied by the system owner on 2026-08-08.

| Area | Target hardware | Details still required |
| --- | --- | --- |
| CPU and chipset | 386SX-compatible M6117 family at 40 MHz | Confirm the exact chip marking and revision, expected to be M6117D |
| Realtime clock | Optional until measured on the board | Confirm an RTC is present and usable, including the 32.768 kHz source, CMOS access, battery, and retained calendar state |
| Memory | 16 MiB RAM | DRAM type, bank layout, BIOS memory map, and any reserved regions |
| Storage | 1 GB CompactFlash card | Confirm IDE-compatible mode, channel, geometry/LBA behavior, and adapter wiring |
| Serial | RS-232 | UART model, port count, I/O bases, IRQ routing, and connector wiring |
| Parallel | DB25 parallel port | SPP/EPP/ECP capability, I/O base, IRQ, and whether Covox shares this port |
| Expansion | 16-bit ISA bus | Slot count, fixed resource assignments, and Plug and Play policy |
| Network | Realtek RTL8019AS | I/O base, IRQ, PnP configuration, EEPROM configuration, and transceiver connector |
| Audio | Yamaha YMF262-M plus Covox | OPL3 base ports, companion DAC/mixer hardware, IRQ/DMA use, and Covox wiring |
| Video | SVGA | Controller model, video memory, VBE BIOS support, available modes, and linear-framebuffer capability |

The M6117D manufacturer data describes a 25-40 MHz 386SX-compatible core, support for up to 16 MiB on a 386SX system, two cascaded 8259 interrupt controllers, an 8254 programmable counter, built-in RTC, PS/2 keyboard/mouse controller, ISA interface, and an IDE decoder. These capabilities are chipset features; the exact board wiring and BIOS configuration still need to be measured on the target machine.

Primary reference: [DM&P M6117D datasheet](https://www.dmp.com.tw/app/webcamera/pdf/m6117d.pdf).

## Architectural Baseline

Frog code intended for Target 1 must assume an i386/PC-compatible baseline:

- no CPUID requirement;
- no timestamp counter requirement;
- no local APIC or APIC timer requirement;
- no HPET or ACPI requirement;
- single CPU;
- at most 16 MiB of physical RAM;
- 8259 PIC interrupt routing;
- 8254/PIT channel 0 as the baseline periodic clock event;
- RTC/CMOS as an optional wall-clock source when the actual board exposes a valid retained clock;
- ISA-style programmed I/O and explicit I/O-port/IRQ resource configuration.

Newer clocks or buses may be detected and used on later targets, but every such path needs a fallback to this baseline.

## Timekeeping Consequences

Target 1 is expected to provide the two hardware roles Frog currently needs, but only the periodic source is required for graphical startup:

- **Periodic clock event**: 8254 channel 0 drives IRQ0 through the 8259 PIC. Frog uses this to advance scheduling and timer queues.
- **Optional wall-clock origin**: if the board exposes a working RTC with valid retained calendar state, it supplies realtime at boot. The M6117D family integrates RTC logic, but the exact chip, clock source, battery, and board wiring still require confirmation.

Frog should accumulate PIT interrupts into a wrap-safe software monotonic timeline. When realtime is available, `gettimeofday` combines the RTC boot value with monotonic elapsed time; otherwise realtime calls return `-ENODATA`. Poudland frame scheduling always consumes monotonic time directly and therefore does not depend on RTC hardware.

Before this is reliable, the current PIT divisor high-byte bug must be fixed and the configured tick rate must be measured in QEMU and on Target 1. A 64-bit or equivalent wrap-safe software counter is also required because a 32-bit millisecond counter wraps after about 49.7 days.

## Bring-up Order

1. Boot and protected-mode execution on the 386SX-compatible core.
2. Verify the BIOS memory map and constrain allocation to the installed 16 MiB.
3. Validate 8259 interrupt routing, 8254 frequency, monotonic ticks, and RTC reads.
4. Bring up an interrupt-driven RS-232 diagnostic console.
5. Detect and read the CompactFlash card through the actual IDE-compatible path using read-only probes first.
6. Validate PS/2 keyboard and mouse event devices if the board exposes the chipset controllers.
7. Identify the SVGA controller and prove a conservative display mode before relying on VBE or a linear framebuffer.
8. Add explicit ISA resource configuration and probe the RTL8019AS without disturbing unrelated ports.
9. Bring up the parallel port and Covox path.
10. Bring up YMF262-M register I/O and any companion DAC/mixer hardware.

Each stage needs a text or serial diagnostic fallback. Destructive disk writes, ISA probing across unknown ports, and unverified video-mode changes must be opt-in until the board resource map is documented.

## Information to Capture from the Machine

- Clear photographs or transcriptions of chipset, Super I/O, SVGA, NIC, and audio chip markings.
- BIOS vendor/version and every relevant setup-screen value.
- BIOS equipment and memory reports.
- I/O base, IRQ, and DMA settings from jumpers, EEPROM utilities, or DOS diagnostics.
- CompactFlash adapter type and reported CHS/LBA geometry.
- SVGA VBE controller/mode information, if present.
- RS-232 UART identification and loopback results.
- Parallel-port mode and Covox electrical connection.
- RTL8019AS PnP/EEPROM settings and MAC address handling policy.
- YMF262 clock, base address, and companion chip connections.
