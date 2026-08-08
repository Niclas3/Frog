# Poudland Protocol Version 1

Status: Accepted for the Poudland P0 runtime

Poudland Version 1 is the application protocol carried inside `FROG_PKG_DATA` records. Packagefs supplies record boundaries, session routing, backpressure, and disconnect notification; Poudland defines graphical objects and requests.

## Compatibility

Version 1 preserves the intended source-level behavior of `desktop.c`, but does not preserve the legacy binary message layout. The Poudland Server, client library, and demo client migrate together. Legacy layouts containing pointers, intrusive list nodes, fixed 1024-byte reads, or ambiguous size fields are rejected rather than translated in the kernel.

## Common Header

Every Poudland message begins with:

```c
struct poudland_msg_header {
        uint_32 magic;
        uint_16 version;
        uint_16 header_size;
        uint_32 type;
        uint_32 request_id;
        uint_32 payload_size;
};
```

No pointer, `struct list_head`, native `long`, or compiler-private object layout crosses the protocol. The packagefs payload length must equal `header_size + payload_size` exactly.

## Requests, Responses, and Errors

- A client assigns a nonzero request ID to every operation that requires a response.
- Request IDs are scoped to one Poudland Session.
- A client begins at ID 1 and increments, skipping zero and every ID that is still pending after wraparound.
- An ID is not reused until its response has been consumed.
- The response echoes the request ID.
- Asynchronous server events use request ID zero.
- A failed request returns `PL_MSG_ERROR` with the same request ID, a signed Frog errno status, and the failed request type.
- Malformed frames that cannot be safely associated with a request are protocol violations and may close the session.

```c
struct poudland_error {
        int_32 status;
        uint_32 failed_type;
};
```

## Version Negotiation

The first implementation supports only protocol version 1. `HELLO` advertises the client's supported range; `WELCOME` selects the common version and reports server capabilities. No common version returns an error and closes the Poudland Session.

## Window Identity and Ownership

- A window ID is a nonzero 32-bit value assigned by the Poudland Server.
- It is not reused during one server lifetime.
- Every window is owned by exactly one Poudland Session.
- An operation on another session's window fails with `-EPERM`.
- Final session disconnect destroys all windows owned by that session and damages their former screen regions.

## Geometry

- Window `x` and `y` are signed 32-bit coordinates.
- Width and height are unsigned values from 1 through 4096.
- Validation performs widened checked arithmetic for every coordinate plus extent.
- Windows may be partly outside the display so the existing demo's third window remains valid.
- Composition clips every affected rectangle to the display bounds.
- Interactive movement keeps at least part of the window visible.

## Logical Color

Version 1 solid-color windows use logical XRGB8888. The client protocol is independent of the physical framebuffer format. Poudland converts logical color to the selected 8-, 16-, or 32-bpp backend representation.

## Window Close Versus Session Close

`WINDOW_CLOSE` is an application request to destroy one owned window while retaining the Poudland Session and packagefs fd. `WINDOW_CLOSED`, carrying the same window and request IDs, confirms that the object no longer exists. It is not a transport half-close or a TCP-style connection shutdown handshake.

Closing the final packagefs endpoint reference remains sufficient to terminate a session. Version 1 removes the legacy application `GOODBYE`; correct cleanup never depends on receiving an application goodbye message.

## P0 Message Set

The initial connection and object path is implemented first:

- `HELLO` -> `WELCOME` or `ERROR`;
- `WINDOW_NEW` -> `WINDOW_INIT` or `ERROR`;
- `WINDOW_CLOSE` -> `WINDOW_CLOSED` or `ERROR`.

The interaction path then adds:

- `WINDOW_CONFIGURE`, carrying the absolute position after the server moves a window;
- `POINTER_EVENT`, covering click, down, raise, enter, leave, move, and drag;
- `KEY_EVENT`, delivered to the window with keyboard focus.

Resize negotiation, Window Surface attachment, clipboard, advertisement, subscription, and other legacy message numbers are outside Version 1 P0.

## Resource Limits

- One Poudland Session may own at most 16 windows.
- One Poudland Server may own at most 64 windows across all sessions.
- Exceeding either window limit returns `-ENOSPC` through `PL_MSG_ERROR`.
- These limits are named UAPI-independent implementation limits and may be raised without changing the message layout.

## Deferred Work

- Window Surface attachment, commit, damage, and resize negotiation.
- Clipboard, advertisements, subscriptions, and shell-specific control messages.
- Capability evolution beyond the initial Version 1 feature bits.
