# Debugging

Run `make debug` to build the debug image. VirtualBox debugging must be enabled through a separately configured VirtualBox debug provider before connecting GDB:

```gdb
gdb build/kyronos.kernel
(gdb) target remote :1234
(gdb) continue
```

Use `break kmain` to stop at kernel entry. The kernel is linked without a host runtime, so inspect memory and registers directly when diagnosing boot failures.
