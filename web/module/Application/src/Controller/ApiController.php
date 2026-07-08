<?php

declare(strict_types=1);

namespace Application\Controller;

use Laminas\Mvc\Controller\AbstractActionController;
use Laminas\View\Model\JsonModel;
use PDO;

class ApiController extends AbstractActionController
{
    private PDO $pdo;

    public function __construct(PDO $pdo)
    {
        $this->pdo = $pdo;
    }

    /** GET /api/topology — nodes + edges with ICMP ping color */
    public function topologyAction()
    {
        $devices = [
            'InterNet1' => ['Internet终端', '10.1.1.26',      'pc'],
            'R1'        => ['R1 (出口路由器)', '10.1.1.25',    'router'],
            'C1'        => ['C1 (核心交换机)', '10.1.1.14',    'switch'],
            'C2'        => ['C2 (核心交换机)', '10.1.1.18',    'switch'],
            'D1'        => ['D1 (汇聚交换机)', '10.1.1.6',     'switch'],
            'D2'        => ['D2 (汇聚交换机)', '10.1.1.10',    'switch'],
            'Cloud1'    => ['Cloud1 (桥接)',   null,           'cloud'],
            'VM'        => ['VM (DNS/Manager)', '192.168.179.136', 'server'],
            'PC-A'      => ['PC-A',            '192.168.2.2',  'pc'],
            'PC-B'      => ['PC-B',            '192.168.3.2',  'pc'],
            'PC-C'      => ['PC-C',            '192.168.4.2',  'pc'],
            'PC-D'      => ['PC-D',            '192.168.5.2',  'pc'],
        ];

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

        return new JsonModel([
            'nodes' => $nodes,
            'edges' => $edges,
        ]);
    }

    /** GET /api/devices — device list with latest metrics */
    public function devicesAction()
    {
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
        $rows = $this->cleanRows($stmt->fetchAll(PDO::FETCH_ASSOC));

        $devices = [];
        foreach ($rows as $row) {
            $devices[$row['device']][] = $row;
        }
        return new JsonModel(['devices' => $devices]);
    }

    /** GET /api/device/{name} — single device latest metrics */
    public function deviceAction()
    {
        $name = $this->params()->fromRoute('name');
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

    /** GET /api/metrics/{name} — time-series for chart */
    public function metricsAction()
    {
        $metricName = $this->params()->fromRoute('name');

        $allowed = ['ifInOctets', 'ifOutOctets', 'memTotalReal', 'memAvailReal', 'dskTotal', 'dskUsed'];
        if (!in_array($metricName, $allowed, true)) {
            $this->getResponse()->setStatusCode(400);
            return new JsonModel(['error' => 'invalid metric name']);
        }

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

    private function cleanRows(array $rows): array
    {
        foreach ($rows as &$row) {
            if (isset($row['value'])) {
                $row['value'] = $this->stripSnmpPrefix($row['value']);
            }
        }
        return $rows;
    }

    private function stripSnmpPrefix(string $value): string
    {
        $colon = strpos($value, ':');
        if ($colon !== false && $colon < 20) {
            return trim(substr($value, $colon + 1), " \"\t\n\r\0\x0B");
        }
        return $value;
    }

    private function pingCheck(string $ip): string
    {
        $cmd = sprintf('ping -c 3 -W 1 %s 2>&1', escapeshellarg($ip));
        exec($cmd, $output);
        // find last non-empty line (the summary line)
        $line = '';
        for ($i = count($output) - 1; $i >= 0; $i--) {
            if (trim($output[$i]) !== '') {
                $line = $output[$i];
                break;
            }
        }
        $array = explode(',', $line);
        if (isset($array[2]) && trim($array[2]) === '100% packet loss') {
            return 'red';
        }
        return 'green';
    }
}
