#include "collector.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

#define PID_FILE "/tmp/snmp_collector.pid"

static volatile sig_atomic_t g_running = 1;

static void signal_handler(int sig) {
    (void)sig;
    g_running = 0;
}

static int acquire_pidfile(const char *path) {
    int fd = open(path, O_RDWR | O_CREAT, 0644);
    if (fd < 0) {
        fprintf(stderr, "pidfile open(%s): %s\n", path, strerror(errno));
        return -1;
    }
    if (flock(fd, LOCK_EX | LOCK_NB) != 0) {
        fprintf(stderr, "another instance is already running (pidfile locked)\n");
        close(fd);
        return -1;
    }
    if (ftruncate(fd, 0) != 0) {
        close(fd);
        return -1;
    }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d\n", getpid());
    if (write(fd, buf, strlen(buf)) != (ssize_t)strlen(buf)) {
        close(fd);
        return -1;
    }
    return fd;
}

static void daemonize(void) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(1);
    }
    if (pid > 0)
        _exit(0);

    if (setsid() < 0) {
        perror("setsid");
        exit(1);
    }

    pid = fork();
    if (pid < 0) {
        perror("fork2");
        exit(1);
    }
    if (pid > 0)
        _exit(0);

    if (chdir("/") < 0) {
        perror("chdir");
        exit(1);
    }
    umask(0);

    int fd = open("/dev/null", O_RDWR);
    if (fd >= 0) {
        dup2(fd, STDIN_FILENO);
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        if (fd > 2) close(fd);
    }
}

int main(int argc, char *argv[]) {
    collector_config_t config;
    int pid_fd = -1;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <config-file>\n", argv[0]);
        return 1;
    }

    init_snmp("snmp_collector");
    snmp_disable_log(); // question

    if (load_collector_config(argv[1], &config) != 0) return 1;

    pid_fd = acquire_pidfile(PID_FILE);
    if (pid_fd < 0)
        return 1;

    daemonize();

    /* rewrite PID after fork */
    if (ftruncate(pid_fd, 0) == 0) { // question
        char buf[32];
        snprintf(buf, sizeof(buf), "%d\n", getpid());
        lseek(pid_fd, 0, SEEK_SET);
        if (write(pid_fd, buf, strlen(buf)) != (ssize_t)strlen(buf)) {}
    }

    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);

    while (g_running) {
        run_async_collection(&config);
        if (!g_running) break;
        sleep(60);
    }

    if (pid_fd >= 0) {
        flock(pid_fd, LOCK_UN);
        close(pid_fd);
    }
    unlink(PID_FILE);

    return 0;
}
