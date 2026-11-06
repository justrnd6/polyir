;; The host side of the one import tiny programs need (`putchar`): this stub discards the character, so a wasm run can
;; still call the exported functions (`wasmtime run --preload env=host-stub.wat --invoke sumsq 10`).
(module
  (func (export "putchar") (param i32) (result i32) (local.get 0)))
