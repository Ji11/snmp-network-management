#include "collector.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PID_FILE "/tmp/async_collector.pid"

sig_atomic_t running = 1;

void signal_handler(int sig) {
    (void)sig;
    running = 0;
}

// 创建/锁定 PID 文件，保证单实例运行
int lock_pidfile(char *path) {
    int fd = open(path, O_RDWR | O_CREAT, 0644);

    // F_WRLCK 排他写锁，F_SETLK 非阻塞获取
    struct flock lock;
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET; // 从文件开头开始锁定
    lock.l_start = 0;
    lock.l_len = 0;  // 0 锁定整个文件
    if (fcntl(fd, F_SETLK, &lock) != 0) {
        // 锁被占用，就读取已有 PID 告知用户
        char buf[32];
        ssize_t n = read(fd, buf, sizeof(buf) - 1); // 读取已有 PID
        buf[n] = '\0';
        // PID 文件内容是 12345\n，要定位到然后换成 \0
        char *nl = strchr(buf, '\n');
        if (nl) *nl = '\0';
        fprintf(stderr, "another process is running (pid=%s)\n", buf);
        close(fd);
        return -1;
    }
    // ftruncate(fd, 目标文件大小) 截断文件到指定长度，这里清空文件内容
    if (ftruncate(fd, 0) != 0) {
        close(fd);
        return -1;
    }

    // 写入当前进程 PID 到文件中
    char buf[32];
    snprintf(buf, sizeof(buf), "%d\n", getpid());
    if (write(fd, buf, strlen(buf)) != (ssize_t)strlen(buf)) { // 把 buf 存的 pid 写到文件中
        close(fd);
        return -1;
    }
    return fd;
}

int main(int argc, char *argv[]) {
    collector_t config;
    int pidfile_fd = -1;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <config-file>\n", argv[0]);
        return 1;
    }

    // 初始化 netsnmp
    init_snmp("async_collector");
    snmp_disable_log();

    // 加载配置文件
    if (load_collector_config(argv[1], &config) != 0) return 1;

    // daemon 前加锁
    pidfile_fd = lock_pidfile(PID_FILE);
    if (pidfile_fd < 0) return 1;

    if (daemon(1, 0) != 0) {
        perror("daemon");
        return 1;
    }

    // fork 后 PID 变了，清空并重写 PID 文件
    if (ftruncate(pidfile_fd, 0) == 0) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%d\n", getpid());
        // 之前的 ftruncate 只改文件长度，不重置写入位置
        // 得先用 lseek 把读写位置移到开头
        lseek(pidfile_fd, 0, SEEK_SET);
        if (write(pidfile_fd, buf, strlen(buf)) != (ssize_t)strlen(buf)) {}
    }

    // 注册信号
    //kill 发 SIGTERM 信号，Ctrl+C 发 SIGINT 信号
    // 收到这两个信号时就置 running = 0，退出异步循环，释放记录锁
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);

    // 异步循环 每 60s 进行一轮异步 SNMP 采集
    while (running) {
        run_async_collection(&config);
        if (!running) break;
        sleep(60);
    }

    // 释放 fcntl 锁
    if (pidfile_fd >= 0) {
        struct flock lock;
        lock.l_type = F_UNLCK;
        lock.l_whence = SEEK_SET;
        lock.l_start = 0;
        lock.l_len = 0;
        fcntl(pidfile_fd, F_SETLK, &lock);
        close(pidfile_fd);
    }
    unlink(PID_FILE);

    return 0;
}
