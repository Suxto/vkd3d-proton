/* Standalone regression for UMD logging with an inherited, undrained stderr.
 * Link this with libs/vkd3d-common/debug.c and -DVKD3D_BUILD_UMD. */
#define VKD3D_DBG_CHANNEL VKD3D_DBG_CHANNEL_API
#include "vkd3d_debug.h"
#include <windows.h>
#include <stdio.h>
#include <string.h>
/* Only platform dependency of debug.c not supplied by its inline headers. */
bool vkd3d_get_env_var(const char *name, char *buffer, size_t size)
{
    DWORD n = GetEnvironmentVariableA(name, buffer, (DWORD)size);
    return n && n < size;
}
int main(int argc, char **argv)
{
    if (argc > 1 && !strcmp(argv[1], "--child")) {
        for (unsigned i = 0; i < 1000; ++i)
            INFO("undrained-pipe startup message %u: 0123456789012345678901234567890123456789\n", i);
        vkd3d_dbg_flush();
        return 0;
    }
    char exe[MAX_PATH], command[2 * MAX_PATH];
    GetModuleFileNameA(NULL, exe, sizeof(exe));
    unsigned failures = 0;
    SetEnvironmentVariableA("VKD3D_DEBUG", "info");
    for (unsigned test = 0; test < 4; ++test) {
        SetEnvironmentVariableA("VKD3D_LOG_BUFFERED", test == 1 ? "8192" : NULL);
        SetEnvironmentVariableA("VKD3D_LOG_FILE", test == 2 ? "umd-log-pipe-output.txt" :
                test == 3 ? "?:\\invalid\\log.txt" : NULL);
        SECURITY_ATTRIBUTES sa = {sizeof(sa), NULL, TRUE};
        HANDLE read_pipe, write_pipe;
        if (!CreatePipe(&read_pipe, &write_pipe, &sa, 4096)) return 2;
        SetHandleInformation(read_pipe, HANDLE_FLAG_INHERIT, 0);
        STARTUPINFOA startup = {.cb = sizeof(startup), .dwFlags = STARTF_USESTDHANDLES,
                .hStdOutput = write_pipe, .hStdError = write_pipe};
        PROCESS_INFORMATION process = {0};
        snprintf(command, sizeof(command), "\"%s\" --child", exe);
        if (!CreateProcessA(exe, command, NULL, NULL, TRUE, CREATE_NO_WINDOW,
                            NULL, NULL, &startup, &process)) return 3;
        CloseHandle(write_pipe);
        DWORD wait = WaitForSingleObject(process.hProcess, 3000), exit_code = 0, available = 0;
        PeekNamedPipe(read_pipe, NULL, 0, NULL, &available, NULL);
        if (wait == WAIT_TIMEOUT) {
            TerminateProcess(process.hProcess, 124);
            WaitForSingleObject(process.hProcess, 3000);
        }
        GetExitCodeProcess(process.hProcess, &exit_code);
        printf("test=%u wait=%lu exit=%lu unread=%lu\n", test, wait, exit_code, available);
        failures += wait != WAIT_OBJECT_0 || exit_code || available;
        CloseHandle(read_pipe); CloseHandle(process.hProcess); CloseHandle(process.hThread);
        if (test == 2) {
            FILE *f = fopen("umd-log-pipe-output.txt", "rb");
            if (!f) ++failures;
            else { fseek(f, 0, SEEK_END); failures += ftell(f) < 10000; fclose(f); }
        }
    }
    return failures ? 1 : 0;
}
