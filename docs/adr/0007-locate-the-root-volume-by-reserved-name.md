---
status: accepted
---

# Locate the initial Root Volume by a reserved name

The first Frog System Image uses the reserved FrogFS volume name `frog-root`, and startup requires exactly one registered partition with that name instead of depending on ATA discovery order or a path such as `/dev/sdbp1`. No match or multiple matches stop startup. A future boot configuration may provide a label or UUID Root Locator override without changing this initial System Image contract.

Discovery is fail-closed. Two structurally valid matches are a definite
duplicate. A readable but structurally invalid candidate whose fixed label is
`frog-root` is corrupt. If any published partition cannot be read and a
duplicate has not already been proven, discovery is indeterminate and returns
an unreadable result rather than selecting an otherwise valid match. A
readable corrupt candidate with a different label does not prevent selection.
