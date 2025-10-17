// cc -fPIC -Wall -Wl,-init,pyusdt_init -o libpyusdt.so -shared pyusdt.c
#include <sys/sdt.h>
#include <stdio.h>

void pyusdt_PY_START(const char *code, const char *file, long long line)
{
    DTRACE_PROBE3(pyusdt, PY_START, code, file, line);
    //fprintf(stderr, "probe complete: %s code [%s:%lld]\n", code, file, line);
}

void pyusdt_init(void)
{
    fprintf(stderr, "pyusdt C library loaded\n");
}
