# Frog Domain Language

This context names Frog's graphical, filesystem, and startup concepts and keeps
implemented milestones distinct from accepted future architecture.

## Current Implementation Boundary

As of 2026-08-10, normal startup locates the read-only FrogFS System Image by
volume name `frog-root`, stages it at `/sysroot`, performs the one-time Root
Switch while preserving `/dev`, loads `/sbin/init` as PID1, selects
`/sbin/init-graphical`, and starts `/bin/compositor` plus `/bin/desktop`.
`/test` is only a Test Mount Point or an explicit historical Poudland P0
fixture; it is never an alias for the production Root Filesystem. Current
commands and evidence are in `doc/root-filesystem-implementation-handoff.md`.

## Language

**Compositor**:
The graphical authority that combines window surfaces and the pointer into the scene shown to the user.
_Avoid_: Desktop, GUI

**Poudland**:
Frog's compositor project and graphical window environment. The current compositor implementation and its client-facing window concepts belong to Poudland.
_Avoid_: Desktop Shell

**Poudland Server**:
The server role of the Compositor. It owns the graphical scene and accepts window and display updates from Poudland Clients.
_Avoid_: Desktop Shell, Client

**Desktop Shell**:
A Poudland Client that owns the user-facing desktop environment, including the background, panel, launcher, and other session-wide interface elements.
_Avoid_: Compositor

**Poudland Client**:
A program outside the compositor that asks the Poudland Server to create and manage its windows and supplies their display updates.
_Avoid_: Built-in Window

**Poudland Transport**:
The message-carrying boundary between Poudland Clients and the Poudland Server.
_Avoid_: Framebuffer

**Poudland Session**:
One client's live relationship with the Poudland Server. A Session has an opaque identity and remains live until the final reference to its transport endpoint closes.
_Avoid_: File descriptor, Kernel pointer

**Poudland Request ID**:
A nonzero identifier chosen by a Poudland Client to associate one request with its response inside a Poudland Session. Zero is reserved for asynchronous events.
_Avoid_: Client ID, Window ID

**Window ID**:
A nonzero identifier assigned by the Poudland Server to one client-owned window. It remains unique for one server lifetime and is never a pointer.
_Avoid_: Client ID, Request ID, Window pointer

**Built-in Window**:
A window owned directly by the compositor, without a Poudland Client. Built-in Windows validate composition and interaction before client communication is available.
_Avoid_: Client Window

**Input Event Source**:
A file-descriptor-backed source of discrete input events consumed by the Poudland Server. Keyboard and pointer devices are initial Input Event Sources, but the concept is not limited to them.
_Avoid_: TTY, Framebuffer

**Window Surface**:
Pixel storage containing a Poudland Client's visual content for one window. A Window Surface is separate from the physical framebuffer and is composed by the Poudland Server.
_Avoid_: Framebuffer, Window

**Damage**:
A region of a Window Surface or the graphical scene whose displayed pixels may no longer match current state and therefore need recomposition.
_Avoid_: Full redraw

**Frame Deadline**:
A monotonic-time boundary at which accumulated Damage may be presented. It limits presentation frequency without requiring unchanged pixels to be redrawn.
_Avoid_: Refresh rate, Wall-clock time

## Filesystem and Startup

**Bootstrap Root**:
A temporary root namespace that exists only to make the devices and storage needed for system startup available.
_Avoid_: Root Filesystem, System Image

**Root Filesystem**:
The production filesystem namespace rooted at `/`. It contains system files and the mount points through which volatile devices, temporary data, and mutable state become visible.
_Avoid_: `/test`, Bootstrap Root, Root Image

**System Image**:
A deterministic, read-only build artifact containing the Root Filesystem's system programs, configuration defaults, and shared resources. Writable runtime data belongs to another mounted filesystem.
_Avoid_: Boot Image, Data Disk, Root Filesystem

**Boot Image**:
The artifact responsible for loading Frog's bootloader and kernel. It is distinct from the System Image and does not define the production filesystem namespace.
_Avoid_: System Image, Root Filesystem

**Mutable State**:
Persistent data expected to survive reboot and system-image replacement. It belongs to separately writable storage rather than the System Image.
_Avoid_: System File, Temporary Data

**Temporary Data**:
Runtime data that may be discarded at reboot. It does not belong to the System Image or Mutable State.
_Avoid_: Mutable State, System File

**System Init**:
The fixed first userspace entry point that selects one Startup Mode and hands the PID1 role to that mode's init program.
_Avoid_: Graphical Init, TTY Init, Kernel Init

**Startup Mode**:
The mutually exclusive choice of the user environment started after the Root Filesystem is ready. Frog initially defines Graphical and TTY modes.
_Avoid_: Display Driver, Init Process

**Boot Override**:
A choice supplied for one startup by the Boot Image or bootloader that takes precedence over a default stored in the System Image.
_Avoid_: System Configuration, Runtime Setting

**Graphical Init**:
The mode-specific PID1 program that owns startup and supervision of the Compositor and Desktop Shell.
_Avoid_: System Init, Compositor, Desktop Shell

**TTY Init**:
The mode-specific PID1 program that owns startup and supervision of the text-based user environment.
_Avoid_: System Init, TTY Device

**Root Switch**:
The one-time startup transition that replaces the Bootstrap Root with the verified Root Filesystem while preserving the mounted device namespace.
_Avoid_: Remount, Root Image Build

**Root Locator**:
The startup rule that identifies exactly one block volume as the System Image. Frog's initial Root Locator is the reserved FrogFS volume name `frog-root`.
_Avoid_: Device Order, Mount Point, Root Filesystem

**System Root Staging Point**:
The `/sysroot` location in the Bootstrap Root where the selected System Image is mounted immediately before the Root Switch.
_Avoid_: Root Filesystem, Test Mount Point, Persistent Mount Point

**System Image Manifest**:
The versioned declaration of every directory and file included in a deterministic System Image.
_Avoid_: Root Filesystem, Runtime Package List

**Test Image Overlay**:
A non-production manifest contribution used to add test-only programs and fixtures to a disposable image derived from the production System Image contents.
_Avoid_: System Image Manifest, Runtime Installation

**Test Mount Point**:
The `/test` namespace location where disposable filesystems are mounted for isolated filesystem validation. It is never the production Root Filesystem.
_Avoid_: Root Filesystem, System Image
