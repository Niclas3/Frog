---
status: accepted
---

# Packagefs uses record-oriented reference-counted sessions

Packagefs is a named, bounded, bidirectional record transport with one server and multiple client sessions, not a byte-stream filesystem or shared-memory substitute. Client identities are opaque values rather than kernel pointers; descriptors hold reference-counted endpoint objects, and only the final endpoint close disconnects a session. Peer closure preserves queued records for draining, reports hangup, ends reads with EOF, fails later writes with `-EPIPE`, and generates a client-specific disconnect record so servers such as Poudland can release owned resources after explicit close or abrupt process exit.
