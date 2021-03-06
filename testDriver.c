#include "testLab.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#if defined __linux__
#include <sys/resource.h>
#endif

static int LaunchLabExecutable(char* labExe);
static int UserMemoryLimit = -1;
static int UserTimeout = -1;

static size_t GetMemoryLimit(void) {
    return UserMemoryLimit < 0 ? GetTestMemoryLimit() : (size_t) UserMemoryLimit;
}

static int GetTimeout(void) {
    return UserTimeout < 0 ? GetTestTimeout() : UserTimeout;
}

static const char* GetRunnerCommand(const char* runnerExe, char* labExe) {
    static char runnerCommand[4096] = {0};
    int testRunnerSize = snprintf(
        runnerCommand, sizeof(runnerCommand), "%s -m %zu -t %d -e %s",
        runnerExe, (GetMemoryLimit() + 1023) / 1024, GetTimeout(), labExe);
    if (testRunnerSize < 0) {
        fprintf(stderr, "\nInternal error: snprintf returned negative value\n");
        return NULL;
    }
    if (testRunnerSize == sizeof(runnerCommand) - 1) {
        fprintf(stderr, "\nInternal error: snprintf stopped at the end of buffer\n");
        return NULL;
    }
    return runnerCommand;
}

int main(int argc, char* argv[])
{
    int i;
    const char* runnerExe = argv[0];

    if (argc >= 4 && strcmp(argv[1], "-m") == 0 && atoi(argv[2]) != 0) {
        UserMemoryLimit = atoi(argv[2])*1024;
        argv += 2;
        argc -= 2;
    }
    if (argc >= 4 && strcmp(argv[1], "-t") == 0 && atoi(argv[2]) != 0) {
        UserTimeout = atoi(argv[2]);
        argv += 2;
        argc -= 2;
    }
    if (argc >= 3 && strcmp(argv[1], "-e") == 0) {
        return LaunchLabExecutable(argv[2]);
    }

    printf("\nKOI FIT NSU Lab Tester (c) 2009-2020 by Evgueni Petrov\n");

    if (argc < 2) {
        printf("\nTo test mylab.exe, do %s mylab.exe\n", runnerExe);
        return 1;
    }

    printf("\nTesting %s...\n", GetTesterName());

    for (i = 0; i < GetTestCount(); i++) {
        printf("TEST %d/%d: ", i+1, GetTestCount());
        if (GetLabTest(i).Feeder() != 0) {
            break;
        }
        const char* runnerCommand = GetRunnerCommand(runnerExe, argv[1]);
        if (!runnerCommand) {
            break;
        }
        if (system(runnerCommand) != 0) {
            break;
        }
        if (GetLabTest(i).Checker() != 0) {
            break;
        }
    }

    if (i < GetTestCount()) {
        printf("\n:-(\n\n"
        "Executable file %s failed for input file in.txt in the current directory.\n"
        "Please fix and try again.\n", argv[1]);
        return 1;
    } else {
        printf("\n:-)\n\n"
        "Executable file %s passed all tests.\n"
        "Please review the source code with your teaching assistant.\n", argv[1]);
        return 0;
    }
}

int HaveGarbageAtTheEnd(FILE* out) {
    while (1) {
        char c;
        int status = fscanf(out, "%c", &c);
        if (status < 0) {
            return 0;
        }
        if (!strchr(" \t\r\n", c)) {
            printf("garbage at the end -- ");
            return 1;
        }
    }
}

const char Pass[] = "PASSED";
const char Fail[] = "FAILED";

const char* ScanUintUint(FILE* out, unsigned* a, unsigned* b) {
    int status = fscanf(out, "%u%u", a, b);
    if (status < 0) {
        printf("output too short -- ");
        return Fail;
    } else if (status < 2) {
        printf("bad output format -- ");
        return Fail;
    }
    return Pass;
}

const char* ScanIntInt(FILE* out, int* a, int* b) {
    if (ScanInt(out, a) == Pass && ScanInt(out, b) == Pass) {
        return Pass;
    }
    return Fail;
}

const char* ScanInt(FILE* out, int* a) {
    int status = fscanf(out, "%d", a);
    if (status < 0) {
        printf("output too short -- ");
        return Fail;
    } else if (status < 1) {
        printf("bad output format -- ");
        return Fail;
    }
    return Pass;
}

const char* ScanChars(FILE* out, size_t bufferSize, char* buffer) {
    if (fgets(buffer, bufferSize, out) == NULL) {
        printf("no output -- ");
        return Fail;
    }
    char* newlinePtr = strchr(buffer, '\n');
    if (newlinePtr != NULL) {
        *newlinePtr = '\0';
    }
    return Pass;
}

size_t GetLabPointerSize(void) {
    return 8; // TODO: determine from bitness of the lab executable
}

unsigned RoundUptoThousand(unsigned int n) {
    return (n + 999) / 1000 * 1000;
}

static void ReportTimeout(const char labExe[]) {
    printf("\nExecutable file \"%s\" didn't terminate in %d seconds\n", labExe, RoundUptoThousand(GetTimeout()) / 1000);
}

static void ReportOutOfMemory(const char labExe[], unsigned labMem) {
    printf("\nExecutable file \"%s\" used %dKi > %dKi\n", labExe, RoundUptoThousand(labMem) / 1000, RoundUptoThousand(GetMemoryLimit()) / 1000);
}

#if defined _WIN32
#include <windows.h>
int LaunchLabExecutable(char* labExe)
{
    SECURITY_ATTRIBUTES labInhertitIO = {sizeof(SECURITY_ATTRIBUTES), NULL, TRUE};
    const HANDLE labIn = CreateFile("in.txt",
        GENERIC_READ,          // open for reading
        FILE_SHARE_READ,       // share for reading
        &labInhertitIO,        // default security
        OPEN_EXISTING,         // existing file only
        FILE_ATTRIBUTE_NORMAL, // normal file
        NULL);                 // no attr. template

    const HANDLE labOut = CreateFile("out.txt",
        GENERIC_WRITE,         // open for writing
        FILE_SHARE_WRITE,      // share for writing (0 = do not share)
        &labInhertitIO,        // default security
        CREATE_ALWAYS,         // overwrite existing
        FILE_ATTRIBUTE_NORMAL, // normal file
        NULL);                 // no attr. template
    const HANDLE labErr = GetStdHandle(STD_ERROR_HANDLE);
    STARTUPINFO labStartup = {
        sizeof(STARTUPINFO), // DWORD  cb;
        NULL, // LPTSTR lpReserved; must be NULL, see MSDN
        NULL, // LPTSTR lpDesktop;
        NULL, // LPTSTR lpTitle;
        0, // DWORD  dwX; ignored, see dwFlags
        0, // DWORD  dwY; ignored, see dwFlags
        0, // DWORD  dwXSize; ignored, see dwFlags
        0, // DWORD  dwYSize; ignored, see dwFlags
        0, // DWORD  dwXCountChars; ignored, see dwFlags
        0, // DWORD  dwYCountChars; ignored, see dwFlags
        0, // DWORD  dwFillAttribute; ignored, see dwFlags
        STARTF_USESTDHANDLES, // DWORD  dwFlags;
        0, // WORD   wShowWindow; ignored, see dwFlags
        0, // WORD   cbReserved2; must be 0, see MSDN
        NULL, // LPBYTE lpReserved2; must be NULL, see MSDN
        NULL,
        NULL,
        NULL,
    };
    labStartup.hStdInput = labIn; // lab stdin is in.txt
    labStartup.hStdOutput = labOut; // lab stdout is out.txt
    labStartup.hStdError = labErr; // lab and tester share stderr
    PROCESS_INFORMATION labInfo = {0};
    int exitCode = 1;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION labJobLimits = {
        { // JOBOBJECT_BASIC_LIMIT_INFORMATION
        {.QuadPart = 0}, // ignored -- LARGE_INTEGER PerProcessUserTimeLimit;
        {.QuadPart = 0}, // ignored -- LARGE_INTEGER PerJobUserTimeLimit;
                JOB_OBJECT_LIMIT_PROCESS_MEMORY
                +JOB_OBJECT_LIMIT_JOB_MEMORY
                +JOB_OBJECT_LIMIT_ACTIVE_PROCESS, // DWORD         LimitFlags;
                0, // ignored -- SIZE_T        MinimumWorkingSetSize;
                0, // ignored -- SIZE_T        MaximumWorkingSetSize;
                1, // DWORD         ActiveProcessLimit;
                0, // ignored -- ULONG_PTR     Affinity;
                0, // ignored -- DWORD         PriorityClass;
                0, // ignored -- DWORD         SchedulingClass;
        }, // JOBOBJECT_BASIC_LIMIT_INFORMATION BasicLimitInformation;
        {0}, // reserved --- IO_COUNTERS                       IoInfo;
        0, // SIZE_T                            ProcessMemoryLimit;
        0, // SIZE_T                            JobMemoryLimit;
        0, // SIZE_T                            PeakProcessMemoryUsed;
        0, // SIZE_T                            PeakJobMemoryUsed;
    };
    labJobLimits.ProcessMemoryLimit = GetMemoryLimit();
    labJobLimits.JobMemoryLimit = GetMemoryLimit();
    labJobLimits.PeakProcessMemoryUsed = GetMemoryLimit();
    labJobLimits.PeakJobMemoryUsed = GetMemoryLimit();
    const HANDLE labJob = CreateJobObject(&labInhertitIO, "labJob");
    size_t labMem0 = SIZE_MAX;

    if (labJob == 0) {
        printf("\nSystem error: %u in CreateJobObject\n", (unsigned)GetLastError());
        CloseHandle(labIn);
        CloseHandle(labOut);
        return 1;
    }

    if (!SetInformationJobObject(labJob, JobObjectExtendedLimitInformation, &labJobLimits, sizeof(labJobLimits))) {
        printf("\nSystem error: %u in SetInformationJobObject\n", (unsigned)GetLastError());
        CloseHandle(labJob);
        CloseHandle(labIn);
        CloseHandle(labOut);
        return 1;
    }

    if (!CreateProcess(NULL, // LPCTSTR lpApplicationName,
        labExe, // __inout_opt  LPTSTR lpCommandLine,
        &labInhertitIO, // __in_opt     LPSECURITY_ATTRIBUTES lpProcessAttributes,
        &labInhertitIO, // __in_opt     LPSECURITY_ATTRIBUTES lpThreadAttributes,
        TRUE, // __in         BOOL bInheritHandles,
        // CREATE_BREAKAWAY_FROM_JOB -- otherwise this process is assigned to the job which runs labtest*.exe and we can't assign it to labJob
        CREATE_SUSPENDED+CREATE_BREAKAWAY_FROM_JOB, // __in         DWORD dwCreationFlags
        NULL, // __in_opt     LPVOID lpEnvironment,
        NULL, // __in_opt     LPCTSTR lpCurrentDirectory,
        &labStartup, // __in         LPSTARTUPINFO lpStartupInfo,
        &labInfo //__out        LPPROCESS_INFORMATION lpProcessInformation
        ))
    {
        printf("\nSystem error: %u in CreateProcess\n", (unsigned)GetLastError());
        CloseHandle(labJob);
        CloseHandle(labIn);
        CloseHandle(labOut);
        return 1;
    }

    if (!AssignProcessToJobObject(labJob, labInfo.hProcess)) {
        printf("\nSystem error: %u in AssignProcessToJobObject\n", (unsigned)GetLastError());
        TerminateProcess(labInfo.hProcess, EXIT_FAILURE);
        CloseHandle(labInfo.hThread);
        CloseHandle(labInfo.hProcess);
        CloseHandle(labJob);
        CloseHandle(labIn);
        CloseHandle(labOut);
        return 1;
    }

    {
        BOOL in;
        if (!IsProcessInJob(labInfo.hProcess, labJob, &in)) {
            printf("\nSystem error: %u in IsProcessInJob\n", (unsigned)GetLastError());
            TerminateProcess(labInfo.hProcess, EXIT_FAILURE);
            CloseHandle(labInfo.hThread);
            CloseHandle(labInfo.hProcess);
            CloseHandle(labJob);
            CloseHandle(labIn);
            CloseHandle(labOut);
            return 1;
        }
    }
    {
        QueryInformationJobObject(
            labJob,
            JobObjectExtendedLimitInformation,
            &labJobLimits,
            sizeof(labJobLimits),
            NULL);


        //fprintf(stderr, "PeakProcessMemoryUsed %d\n", labJobLimits.PeakProcessMemoryUsed);
        //fprintf(stderr, "PeakJobMemoryUsed %d\n", labJobLimits.PeakJobMemoryUsed);
        labMem0 = labJobLimits.PeakProcessMemoryUsed;
    }


    if (ResumeThread(labInfo.hThread) == (DWORD)-1) {
        printf("\nSystem error: %u in ResumeThread\n", (unsigned)GetLastError());
        TerminateProcess(labInfo.hProcess, EXIT_FAILURE);
        CloseHandle(labInfo.hThread);
        CloseHandle(labInfo.hProcess);
        CloseHandle(labJob);
        CloseHandle(labIn);
        CloseHandle(labOut);
        return 1;
    }

    switch (WaitForSingleObject(labInfo.hProcess, GetTimeout())) {
    case WAIT_OBJECT_0:
        {
            DWORD labExit = EXIT_FAILURE;
            if (!GetExitCodeProcess(labInfo.hProcess, &labExit)) {
                printf("\nSystem error: %u in GetExitCodeProcess\n", (unsigned)GetLastError());
            } else if (labExit >= 0x8000000) {
                printf("\nExecutable file \"%s\" terminated with exception 0x%08x\n", labExe, (unsigned)labExit);
            } else if (labExit > 0) {
                printf("\nExecutable file \"%s\" terminated with exit code 0x%08x != 0\n", labExe, (unsigned)labExit);
            } else {
                exitCode = 0; // + check memory footprint after this switch (...) {...}
            }
            break;
        }
    case WAIT_TIMEOUT:
        ReportTimeout(labExe);
        TerminateProcess(labInfo.hProcess, EXIT_FAILURE);
        break;
    case WAIT_FAILED:
        printf("\nSystem error: %u in WaitForSingleObject\n", (unsigned)GetLastError());
        TerminateProcess(labInfo.hProcess, EXIT_FAILURE);
        break;
    default:
        printf("\nInternal error: WaitForSingleObject returned WAIT_ABANDONED\n");
        TerminateProcess(labInfo.hProcess, EXIT_FAILURE);
    }
    {
        QueryInformationJobObject(
            labJob,
            JobObjectExtendedLimitInformation,
            &labJobLimits,
            sizeof(labJobLimits),
            NULL);
        //fprintf(stderr, "PeakProcessMemoryUsed %d\n", labJobLimits.PeakProcessMemoryUsed);
        //fprintf(stderr, "PeakJobMemoryUsed %d\n", labJobLimits.PeakJobMemoryUsed);
        labMem0 = labJobLimits.PeakProcessMemoryUsed-labMem0;
    }
    if ((long long)labMem0 > GetMemoryLimit()) {
        exitCode = 1;
        ReportOutOfMemory(labExe, labMem0);
    }

    CloseHandle(labInfo.hThread);
    CloseHandle(labInfo.hProcess);
    CloseHandle(labJob);
    CloseHandle(labIn);
    CloseHandle(labOut);
    return exitCode;
}
#else
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>             //for open with constants
#include <unistd.h>            //for close
#include <sys/types.h>         //for pid_t type
#include <errno.h>             //for errno
#include <sys/wait.h>          //for wait4
#include <time.h>              //for nanosleep
#include <signal.h>            //for sigaction
/* Задаём некие параметры безопасности.
 * Перехватываем поток ошибок?
 * Задаём начальные параметры для будущего процесса.
 * Что-то с проверкой семафоров на ошибки?
 * Задаём ограничения для будущего процесса.
 * Создаём некий рабочий объект labJob. Если не удалось, то ругаемся.
 * Привязываем к объекту некую информацию. Если не удалось, то ругаемся.
 * Проверяем процесс на какие-то ошибки.
 * Запрашиваем какую-то информацию об объекте.
 * Проверяем поток на какие-то ошибки.
 * Проверяем на завершение и выдаём букет ошибок.
 * Запрашиваем информацию об объекте.
 * Проверяем кол-во использованной памяти.
 * Закрываемся.
 */

#ifdef __APPLE__
// https://unix.stackexchange.com/questions/30940/getrusage-system-call-what-is-maximum-resident-set-size#comment552809_30940
#define RU_MAXRSS_UNITS 1u
#else
#define RU_MAXRSS_UNITS 1024u
#endif

static int CheckMemory(struct rusage rusage, size_t * labMem0) {
    *labMem0 = (size_t) rusage.ru_maxrss * RU_MAXRSS_UNITS;
    if (GetMemoryLimit() < *labMem0) {
        return 1;
    }
    return 0;
}

static void SigchldTrap(int signo) {
    // Заглушка, чтобы спровоцировать EINTR при SIGCHLD
    // По умолчанию SIGCHLD работает как SIG_IGN c SA_RESTART и без SA_NOCLDWAIT
    // Выставить вручную SIG_IGN нельзя, поскольку будет трактоваться как SA_NOCLDWAIT
    (void) signo;
}

static int _LaunchLabExecutable(char* labExe);

static int LaunchLabExecutable(char* labExe) {
    struct sigaction new;
    struct sigaction old;

    memset(&new, 0, sizeof(new));
    new.sa_handler = SigchldTrap;
    (void)sigemptyset(&new.sa_mask); // не умеет завершаться неуспешно
    // Не используем SA_RESETHAND, чтобы не зависеть от использования fork вне LaunchLabExecutable
    new.sa_flags = SA_NOCLDSTOP;
    if (sigaction(SIGCHLD, &new, &old)) {
        fprintf(stderr, "\nSystem error: \"%s\" in sigaction set\n", strerror(errno));
        return -1;
    }

    int retCode = _LaunchLabExecutable(labExe);
    if (sigaction(SIGCHLD, &old, NULL)) {
        fprintf(stderr, "\nSystem error: \"%s\" in sigaction restore\n", strerror(errno));
        return -1;
    }

    return retCode;
}

static int WaitForProcess(pid_t pid, int* status, struct timespec* ts, struct rusage* rusage) {
    while (ts->tv_sec > 0 || ts->tv_nsec > 0) {
        pid_t child = wait4(pid, status, WNOHANG, rusage);
        if (child) {
            return child;
        }
        if (!nanosleep(ts, ts)) {
            return 0;
        }
        if (errno != EINTR) {
            return -1;
        }
    }
    return 0; // timeout
}

static int _LaunchLabExecutable(char* labExe)
{
    int exitCode = 1;
    pid_t pid;

    fflush(stdout);
    if (-1 == access(".", R_OK | W_OK | X_OK | F_OK)) {
        printf("\nSystem error: \"%s\" in access\n", strerror(errno));
        return exitCode;
    }
    pid = fork();
    if (-1 == pid) {
        printf("\nSystem error: \"%s\" in fork\n", strerror(errno));
        return exitCode;
    } else if (!pid) {
        int ret = 0;

        if (!freopen("in.txt", "r", stdin)) {
            fprintf(stderr, "\nSystem error: \"%s\" in freopen\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        if (!freopen("out.txt", "w", stdout)) {
            fprintf(stderr, "\nSystem error: \"%s\" in freopen\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        ret = execl(labExe, labExe, NULL);
        if (-1 == ret) {
            fprintf(stderr, "\nSystem error: \"%s\" in execl\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        exit(EXIT_SUCCESS);
    }
    {
        int status = 0;
        struct rusage rusage;
        struct timespec rem;

        rem.tv_sec = GetTimeout();
        rem.tv_nsec = 0;
        status = WaitForProcess(pid, &status, &rem, &rusage);
        if (-1 == status) {
            printf("\nSystem error: \"%s\" in wait4\n", strerror(errno));
            return exitCode;
        } else if (0 == status) {
            if (kill(pid, SIGKILL)) {
                printf("\nSystem error: \"%s\" in kill\n", strerror(errno));
                return exitCode;
            }
            ReportTimeout(labExe);
            return exitCode;
        } else {
            size_t labMem0 = 0;

            if (CheckMemory(rusage, &labMem0)) {
                ReportOutOfMemory(labExe, labMem0);
                return exitCode;
            } else {
                exitCode = 0;
            }
        }
    }

    return exitCode;
}
#endif

