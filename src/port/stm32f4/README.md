# stm32f4 port

Only F4 parts that have QUADSPI: F412, F413, F423, F446, F469, F479. Same registers
and addresses as F7; Cortex-M4 has no D-cache (`has_dcache = false`). On a part without
the controller, `_init` must return `-ENOTSUP`. Untested - no board at hand yet.
