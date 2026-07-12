#!/bin/bash
# 绕过 systemd-resolved，让 DNS 直连 BIND9
set -e

echo "=== 移除 systemd-resolved 的 symlink ==="
sudo rm -f /etc/resolv.conf

echo "=== 创建静态 resolv.conf 指向本地 BIND9 ==="
echo "nameserver 127.0.0.1" | sudo tee /etc/resolv.conf

echo "=== 验证 ==="
ping -c 1 c1.bjfu.edu.cn
