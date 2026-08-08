---
status: accepted
---

# Poudland owns the display

The Poudland Server is the only user process allowed to map and write the physical framebuffer. Poudland Clients use packagefs as a control plane for window lifecycle and presentation messages; the existing demo may request server-rendered colored windows, while future clients place pixels in shared Window Surfaces and send attach, commit, and Damage messages. This keeps composition, clipping, focus, and display ownership in one authority without copying complete frames through packagefs or allowing clients to overwrite each other's output.
