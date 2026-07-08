#include "collector.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <unistd.h>

#define PID_FILE "/tmp/async_collector.pid"

sig_atomic_t running = 1;

void signal_handler(int sig) {
    (void)sig;
    running = 0;
}

// 创建/锁定 PID 文件，保证单实例运行
int acquire_pidfile(const char *path) {
    // open(): 打开或创建文件
    int fd = open(path, O_RDWR | O_CREAT, 0644);
    if (fd < 0) {
        fprintf(stderr, "pidfile open(%s): %s\n", path, strerror(errno));
        return -1;
    }
    // flock(): BSD 文件锁，LOCK_EX 排他锁，LOCK_NB 非阻塞（已被锁时立即返回而非等待）
    if (flock(fd, LOCK_EX | LOCK_NB) != 0) {
        // 锁被占用，读取已有 PID 告知用户
        char buf[32];
        ssize_t n = read(fd, buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            char *nl = strchr(buf, '\n');
            if (nl) *nl = '\0';
            fprintf(stderr, "another instance is already running (pid=%s)\n", buf);
        } else {
            fprintf(stderr, "another instance is already running\n");
        }
        close(fd);
        return -1;
    }
    // ftruncate(): 截断文件到指定长度，这里清空文件内容
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

int main(int argc, char *argv[]) {
    collector_config_t config;
    int pid_fd = -1;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <config-file>\n", argv[0]);
        return 1;
    }

    // 初始化 Net-SNMP 库，参数为应用名称（用于日志前缀）
    init_snmp("async_collector");
    snmp_disable_log();

    // 加载配置文件
    if (load_collector_config(argv[1], &config) != 0) return 1;

    // daemon 前获取 PID 锁，这样锁失败时可以在终端看到错误输出
    pid_fd = acquire_pidfile(PID_FILE);
    if (pid_fd < 0)
        return 1;

    // daemon(): glibc 函数，nochdir=1 不切工作目录，noclose=0 重定向 stdio 到 /dev/null
    if (daemon(1, 0) != 0) {
        perror("daemon");
        return 1;
    }

    // fork 后 PID 变了，清空并重写 PID 文件
    if (ftruncate(pid_fd, 0) == 0) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%d\n", getpid());
        // lseek(): 移动文件读写位置到开头
        lseek(pid_fd, 0, SEEK_SET);
        if (write(pid_fd, buf, strlen(buf)) != (ssize_t)strlen(buf)) {}
    }

    // 注册信号：SIGTERM(kill 默认信号), SIGINT(Ctrl+C)
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);

    // 主循环：每 60s 发起一轮异步 SNMP 采集
    while (running) {
        run_async_collection(&config);
        if (!running) break;
        sleep(60);
    }

    // 清理锁文件和 PID 文件
    if (pid_fd >= 0) {
        flock(pid_fd, LOCK_UN);
        close(pid_fd);
    }
    unlink(PID_FILE);

    return 0;
}
