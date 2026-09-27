# Transform groups

Select transform nodes and choose **Create transform group from selection** in
the context menu. You can also add a **Transform > Transform Group** node, select
existing transforms, and click **Add selected transforms**. To add nodes without maintaining a canvas selection,
click **Add transforms...** and choose from the searchable list. You can also
right-click a transform (or a selected batch) and choose **Add to transform
group > group name**. Already-grouped transforms are excluded from these actions.
Expand **Members**
to select or remove members. Renaming the group and moving nodes does not change
membership. Membership is saved by node ID and output pin name; copied groups
retain only members included in the same copy/paste operation.

Connect **Content Root** to render jobs that must receive type-12 placement.
Members are measurement dependencies, not type-12 targets: connect them to
**Content Root** where appropriate, but a render job bound directly to a member
will retain that member's unplaced transform. **Root Size** sets the early root
record's dimensions. Connect a placement parent to **Parent**; **Position** and
**Pivot** control the later type-4 frame. **Content Size** exposes measured
bounds; **Frame** exposes the later placement.

## Export

Content Root is an early type-0 record with the configured Root Size. Members
can occupy a contiguous range after the root; an empty group rebases the root
itself. The exporter executes the content transforms, calls
`funcs->rebaseRuiTransformBounds` to measure that range, and assigns the bounds
to a later type-4 frame's size. Type 12 is emitted at the end of the stream
with the later frame index and the half-open interval covering only the early
root: `[root, root + 1)`. It mutates that root; it does not allocate a new record.
The frame uses Parent, Position, and Pivot. No size is overwritten after
execution, and no templates are generated.

Members contribute to measured bounds but are not type-12 targets. Frame is
separate from Content Root and may feed transforms outside the group.
Existing graphs retain member connections; connect Content Root explicitly
when local coordinates are desired. Preview uses the same type-4 placement
math and engine type-12 composition, including its translation cross term.

## Current limits

Groups are flat and non-overlapping. Include dependent transforms in the same
group; member outputs may feed render jobs only if those jobs intentionally
remain unplaced by type 12. A member may use its own Content Root but cannot
depend on Frame or Content Size, and placement cannot depend on member
transforms or another group's Frame. Missing members and dependency conflicts
are shown on the group and block export. Adding unrelated nodes or rearranging
the graph cannot change the exported member range.

## Verification

Build SERE, then run:

```text
python tests/transform_groups_export.py <path-to-SERE.exe>
python tests/arguments_export.py <path-to-SERE.exe>
```

The tests check root-only type-12 targets, early type-0 slots, exclusive
endpoints, monotonic execution barriers, multiple/large groups, stable ordering,
the `gamestate_info_ffa` transform projection, and rejection of invalid graphs.
`tests/arguments_export.py` checks that every exported argument keeps a private
slot with its own type and data offset, that each one resolves by name through
the package's argument name section, and that the table widens when a smaller
one has no collision free hash.
`tests/transform_group_math.cpp` checks bounds, negative basis vectors,
rebasing, later-frame placement, and the engine's type-12 cross term.
