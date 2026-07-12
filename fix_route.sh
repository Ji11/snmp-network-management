#!/bin/bash
# 修复 VM 到 eNSP 的路由：网关应为 C2 Vlanif10 (192.168.179.1)
set -e

for net in 10.0.0.0/8 192.168.0.0/16; do
    echo "=== 更新 $net 路由 ==="
    sudo ip route del "$net" 2>/dev/null || true
    sudo ip route add "$net" via 192.168.179.1
done

echo "=== 验证 ==="
ip route | grep "via 192.168.179.1"
echo ""
ping -c 2 10.1.1.6
