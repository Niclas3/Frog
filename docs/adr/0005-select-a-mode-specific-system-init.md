---
status: accepted
---

# Select a mode-specific System Init in userspace

After the Root Filesystem is ready, the kernel starts one fixed System Init from the System Image. System Init selects either Graphical Init or TTY Init and replaces itself with the selected program so that it retains PID1; the kernel therefore does not own graphical-versus-text startup policy, while the two modes can evolve independently. The first System Image contains only the working Graphical Init target and strictly reads `mode=graphical` from `/etc/frog/init.conf`; a missing or malformed configuration, an unknown field, or selection of TTY before a real TTY Init exists is an explicit startup failure rather than a silent fallback or a placeholder that appears usable. A future `init.mode` Boot Override may select one startup mode without rebuilding the System Image and takes precedence over the stored default, but an invalid override remains a startup failure.
