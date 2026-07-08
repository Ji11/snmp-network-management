#!/bin/bash
#==============================================================================
#  BIND DNS 配置脚本 — bjfu.edu.cn 正向解析
#  五台网络设备: R1, C1, C2, D1, D2
#  运行: bash bind_setup.sh
#==============================================================================

set -e

echo "=== 安装 BIND9 ==="
sudo apt install bind9 -y

echo "=== 添加 zone 定义 ==="
sudo tee -a /etc/bind/named.conf.local << 'EOF'

zone "example.edu.cn" {
    type master;
    file "/etc/bind/db.example.edu.cn";
};
EOF

echo "=== 创建 zone 文件 ==="
sudo tee /etc/bind/db.example.edu.cn << 'EOF'
$TTL    604800
@       IN      SOA     ns.example.edu.cn. admin.example.edu.cn. (
                        2026060501    ; Serial
                        604800        ; Refresh
                        86400         ; Retry
                        2419200       ; Expire
                        604800 )      ; Negative Cache TTL

@       IN      NS      ns.example.edu.cn.

ns      IN      A       192.168.179.136

r1      IN      A       10.1.1.25
c1      IN      A       10.1.1.14
c2      IN      A       10.1.1.18
d1      IN      A       10.1.1.6
d2      IN      A       10.1.1.10
EOF

echo "=== 重启 BIND9 ==="
sudo systemctl restart bind9

echo "=== 验证 DNS 解析 ==="
echo ""
echo "--- nslookup 测试 ---"
for host in r1 c1 c2 d1 d2; do
    echo -n "${host}.example.edu.cn → "
    nslookup ${host}.bjfu.edu.cn 127.0.0.1 | grep -A1 "Name:" | tail -1 | awk '{print $2}'
done

echo ""
echo "--- ping 域名测试 ---"
ping -c 1 c1.example.edu.cn && echo "c1.example.edu.cn 可达"
ping -c 1 c2.example.edu.cn && echo "c2.example.edu.cn 可达"
ping -c 1 d1.example.edu.cn && echo "d1.example.edu.cn 可达"
ping -c 1 d2.example.edu.cn && echo "d2.example.edu.cn 可达"
ping -c 1 r1.example.edu.cn && echo "r1.example.edu.cn 可达"

echo ""
echo "=== BIND DNS 配置完成 ==="
