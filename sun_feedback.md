# Sun development compiler observations

Tested with the published development compiler `cc7686189ee2`.

- `Vec<T>(allocator, 0)` followed by `push` corrupts the heap in this release.
  The current growth implementation doubles capacity, so zero remains zero.
  This project starts vectors at a nonzero capacity. A regression fix belongs
  in the Sun standard library rather than in the UI core.
- A conditional expression selecting between two interpolated owned `String`
  values caused an invalid free in the task-list prototype. Constructing one
  string and conditionally appending the suffix avoids the failure. This is an
  observed compiler/runtime issue, not a proven diagnosis of the lowering bug.
- The host's older `d1b779771504` compiler rejects the current `init` constructor
  syntax. Use the development compiler or the supplied development container.
