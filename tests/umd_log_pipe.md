# UMD logging regression

`libs/vkd3d-common/debug.c` must not write to an application's inherited
stderr when built with `VKD3D_BUILD_UMD`. Adapter detectors which only consume
stdout can otherwise block in device creation once stderr fills.

Standalone build with LLVM-MinGW, from the repository root:

```
x86_64-w64-mingw32-clang -std=c11 -O2 -DVKD3D_BUILD_UMD -Iinclude/private -Iinclude tests/umd_log_pipe.c libs/vkd3d-common/debug.c -static -o umd-log-pipe.exe
```

Run in a writable directory. It starts fresh child processes with an undrained
4 KiB stderr pipe, emits 1,000 log messages, and checks normal exit and no pipe
bytes for default logging, `VKD3D_LOG_BUFFERED`, an explicit file, and an invalid
file path. The explicit file case also checks that diagnostics were written.
`umd-log-pipe-output.txt` is a disposable test artifact. Buffered logging is
ignored in UMD builds so all default messages go directly to the debugger.

Validation on 2026-11-09: ARM64, ARM64EC and x64 cross-compilation passed.
The x64 executable passed all four cases on Windows. Building without
`VKD3D_BUILD_UMD` reproduced timeouts in cases 0, 1 and 3, and passed explicit
file logging. This validates the real logging implementation, not a full driver
package or GPU rendering.

Device evidence: on a Windows 11 ARM64 DroidVM guest, the installed
`adapter_info.exe --disable_vulkan` blocked with 4,097 bytes in its undrained
stderr pipe. With process-local `VKD3D_DEBUG=none` and `MESA_LOG_LEVEL=error`, it
exited normally in 266 ms with no stderr bytes. Do not infer that other startup
issues or missing D3D12 feature levels are fixed by this logging change.
