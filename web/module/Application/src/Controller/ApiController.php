<?php

declare(strict_types=1);

namespace Application\Controller;

use Laminas\Mvc\Controller\AbstractActionController;
use Laminas\View\Model\JsonModel;
use PDO;

/**
 * 4 个 JSON API：topology / devices / device / metrics
 * 构造函数注入 PDO 连接 MySQL
 */
class ApiController extends AbstractActionController
{
    private PDO $pdo;

    public function __construct(PDO $pdo)
    {
        $this->pdo = $pdo;
    }

    // GET /api/topology — 网络拓扑 JSON 节点 + 连线 + ICMP 颜色
    public function topologyAction()
    {
        // 12 个节点：设备名 => [标签, IP, vis.js 分组]
        $devices = [
            'InterNet1' => ['InterNet1', '10.1.1.26',       'pc'],
            'R1'        => ['R1',        '10.1.1.25',       'router'],
            'C1'        => ['C1',        '10.1.1.14',       'switch'],
            'C2'        => ['C2',        '10.1.1.18',       'switch'],
            'D1'        => ['D1',        '10.1.1.6',        'switch'],
            'D2'        => ['D2',        '10.1.1.10',       'switch'],
            'Cloud1'    => ['Cloud1',     null,             'cloud'],
            'VM'        => ['VM',        '192.168.179.136', 'server'],
            'PC-A'      => ['PC-A',      '192.168.2.2',     'pc'],
            'PC-B'      => ['PC-B',      '192.168.3.2',     'pc'],
            'PC-C'      => ['PC-C',      '192.168.4.2',     'pc'],
            'PC-D'      => ['PC-D',      '192.168.5.2',     'pc'],
        ];

        // 遍历设备，ping 决定颜色
        $nodes = [];
        foreach ($devices as $name => [$label, $ip, $group]) {
            $color = 'green';
            if ($ip !== null) {
                $color = $this->pingCheck($ip);
            }
            $nodes[] = [
                'id'    => $name,
                'label' => $label,
                'group' => $group,
                'color' => $color,
            ];
        }

        // 12 条边，对应 eNSP 拓扑的实际连线
        $edges = [
            ['from' => 'InterNet1', 'to' => 'R1'],
            ['from' => 'R1',        'to' => 'C1'],
            ['from' => 'R1',        'to' => 'C2'],
            ['from' => 'C1',        'to' => 'C2'],
            ['from' => 'C1',        'to' => 'D1'],
            ['from' => 'C2',        'to' => 'D2'],
            ['from' => 'C2',        'to' => 'Cloud1'],
            ['from' => 'Cloud1',    'to' => 'VM'],
            ['from' => 'D1',        'to' => 'PC-A'],
            ['from' => 'D1',        'to' => 'PC-B'],
            ['from' => 'D2',        'to' => 'PC-C'],
            ['from' => 'D2',        'to' => 'PC-D'],
        ];

        // JsonModel 自动输出 JSON，无需手动 json_encode
        return new JsonModel([
            'nodes' => $nodes,
            'edges' => $edges,
        ]);
    }

    // GET /api/devices — 所有设备指标 用于 EasyUI 表格
    public function devicesAction()
    {
        // 子查询取每个 (device, metric) 的最大 created_at，再 JOIN 回 t1 取完整行
        // 等价于"每组取最新一条"
        $stmt = $this->pdo->query(
            "SELECT t1.device, t1.metric, t1.oid, t1.value, t1.created_at
             FROM snmp_data t1
             INNER JOIN (
                 SELECT device, metric, MAX(created_at) AS max_ts
                 FROM snmp_data
                 GROUP BY device, metric
             ) t2 ON t1.device = t2.device AND t1.metric = t2.metric AND t1.created_at = t2.max_ts
             ORDER BY t1.device, t1.metric"
        );
        // 去掉 SNMP 类型前缀（Counter32: 等）
        $rows = $this->cleanRows($stmt->fetchAll(PDO::FETCH_ASSOC));

        // 按设备名分组，方便前端按设备遍历
        $devices = [];
        foreach ($rows as $row) {
            $devices[$row['device']][] = $row;
        }
        return new JsonModel(['devices' => $devices]);
    }

    // GET /api/device/{name} — 单个设备指标 详情弹窗用
    public function deviceAction()
    {
        $name = $this->params()->fromRoute('name');
        // 同上子查询逻辑，加 WHERE 限定单个设备，两个占位符值相同但 PDO 需不同名字
        $stmt = $this->pdo->prepare(
            "SELECT t1.device, t1.metric, t1.oid, t1.value, t1.created_at
             FROM snmp_data t1
             INNER JOIN (
                 SELECT device, metric, MAX(created_at) AS max_ts
                 FROM snmp_data
                 WHERE device = :name
                 GROUP BY device, metric
             ) t2 ON t1.device = t2.device AND t1.metric = t2.metric AND t1.created_at = t2.max_ts
             WHERE t1.device = :name2
             ORDER BY t1.metric"
        );
        $stmt->execute(['name' => $name, 'name2' => $name]);
        return new JsonModel([
            'device'  => $name,
            'metrics' => $this->cleanRows($stmt->fetchAll(PDO::FETCH_ASSOC)),
        ]);
    }

    // GET /api/metrics/{name} — 折线图 / 饼图指标的时序数据
    public function metricsAction()
    {
        $metricName = $this->params()->fromRoute('name');

        // 白名单校验，防止 SQL LIKE 注入
        $allowed = ['ifInOctets', 'ifOutOctets', 'memTotalReal', 'memAvailReal', 'dskTotal', 'dskUsed'];
        if (!in_array($metricName, $allowed, true)) {
            $this->getResponse()->setStatusCode(400);
            return new JsonModel(['error' => 'invalid metric name']);
        }

        // LIKE 'ifInOctets%' 匹配所有接口的该指标，限制 300 条防止数据量过大
        $stmt = $this->pdo->prepare(
            "SELECT device, metric, value, created_at
             FROM snmp_data
             WHERE metric LIKE :pattern
             ORDER BY created_at DESC
             LIMIT 300"
        );
        $stmt->execute(['pattern' => $metricName . '%']);
        return new JsonModel([
            'metric' => $metricName,
            'data'   => $this->cleanRows($stmt->fetchAll(PDO::FETCH_ASSOC)),
        ]);
    }

    // 遍历行，去掉 value 的 SNMP 类型前缀
    private function cleanRows(array $rows): array
    {
        foreach ($rows as &$row) {
            if (isset($row['value'])) {
                $row['value'] = $this->stripSnmpPrefix($row['value']);
            }
        }
        return $rows;
    }

    // 把 "Counter32: 12345" → "12345"，"STRING: "R1"" → "R1"
    private function stripSnmpPrefix(string $value): string
    {
        $colon = strpos($value, ':');
        // 冒号位置 < 20 说明是类型前缀（真正的数据不会有这么早的冒号）
        if ($colon !== false && $colon < 20) {
            return trim(substr($value, $colon + 1), " \"\t\n\r\0\x0B");
        }
        return $value;
    }

    // 丢包率 100% 返回 red，否则 green
    private function pingCheck(string $ip): string
    {
        $cmd = sprintf('ping -c 3 -W 1 %s', escapeshellarg($ip)); // escapeshellarg 防止命令注入
        exec($cmd, $output);

        // 从输出末尾向前找第一个非空行
        // 3 packets transmitted, 0 received, 100% packet loss, time 2000ms
        $line = '';
        for ($i = count($output) - 1; $i >= 0; $i--) {
            if (trim($output[$i]) !== '') {
                $line = $output[$i];
                break;
            }
        }

        if (strpos($line, '100% packet loss') !== false) {
            return 'red';
        }
        return 'green';
    }
}
