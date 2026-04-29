# Sprint Status

Maintained by the agile plugin. One row per sprint — updated by `/agile:shape`, `/agile:execute`.

| Sprint | Slug | Status | Description |
|--------|------|--------|-------------|
| Project Scaffolding & Architecture Lock-in | project-scaffolding-architecture | completed | Buildable C extension skeleton, four exception classes, CI matrix, decisions in system.md. |
| ndarray Struct & Creation API | ndarray-struct-and-creation-api | completed | Refcounted buffer, ndarray struct, creation methods (zeros/ones/full/eye/arange/fromArray), toArray. |
| Indexing & Slicing | indexing-and-slicing | completed | ArrayAccess + Countable, offsetGet/Set/Exists, slice() axis-0 view, refcounted views share buffer. |
| Broadcasting & Element-wise Ops | broadcasting-and-elementwise-ops | completed | nd-iterator with broadcasting, dtype promotion, add/sub/mul/div, operator overloading. |
| Shape Manipulation | shape-manipulation | completed | reshape, transpose, flatten, squeeze, expandDims, concatenate, stack. |
| BLAS Integration | blas-integration | completed | dot, matmul, inner, outer wired to OpenBLAS (s* and d* paths); int → f64 promotion. |
| Statistical & Mathematical Functions | stats-and-math-functions | completed | Reductions (sum/mean/min/max/std/var/argmin/argmax) with axis+keepdims, Welford, NaN-aware variants, element-wise math, sort/argsort. |
