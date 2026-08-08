# Frog Graphical Shell

This context names the user-visible parts of Frog's graphical environment and keeps delivery milestones distinct from the eventual desktop shell.

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
The user-facing environment built as a Poudland client, such as a background, panel, launcher, and application-facing windows.
_Avoid_: Compositor

**Poudland Demo Client**:
The current client scenario that creates and removes simple windows to exercise Poudland. It may evolve into the Desktop Shell, but it is not one yet.
_Avoid_: Desktop Shell

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
