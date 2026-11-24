# Vendored ksflate engine

These files are a copy of `ksflate/src/` and `ksflate/include/ksflate.h` from
the ksflate development tree. They are vendored so `pigz-ks` builds standalone;
nothing here is modified for the pigz fork.

- `ksflate.h` — public C API (`ksflate_compress`, `ksflate_free`).
- `ksflate_api.cpp` — implementation of that API.
- The remaining `.cpp`/`.hpp` files are the deflate engine itself.

To refresh them from a checkout that has `../ksflate`:

```sh
make sync-engine
```

Keep the copy byte-identical to the canonical tree when syncing, so level-10
output stays reproducible.
