<?php

declare(strict_types=1);

namespace Application;

use Laminas\Router\Http\Literal;
use Laminas\Router\Http\Segment;
use Application\Controller\ApiController;
use Application\Controller\IndexController;
use Laminas\ServiceManager\Factory\InvokableFactory;
use PDO;

return [
    // URL 路由
    // type: Literal = 精确匹配, Segment = 路径参数匹配（如 /api/device/:name）
    // defaults: 指定匹配成功后调哪个 Controller 的哪个 Action
    'router' => [
        'routes' => [
            // GET / → IndexController::indexAction（首页）
            'home' => [
                'type'    => Literal::class,    // 精确匹配 '/'
                'options' => [
                    'route'    => '/',
                    'defaults' => [
                        'controller' => IndexController::class,
                        'action'     => 'index',
                    ],
                ],
            ],
            // GET /api/topology → ApiController::topologyAction
            'api-topology' => [
                'type'    => Literal::class,    // 精确匹配 '/api/topology'
                'options' => [
                    'route'    => '/api/topology',
                    'defaults' => [
                        'controller' => ApiController::class,
                        'action'     => 'topology',
                    ],
                ],
            ],
            // GET /api/devices → ApiController::devicesAction
            'api-devices' => [
                'type'    => Literal::class,    // 精确匹配 '/api/devices'
                'options' => [
                    'route'    => '/api/devices',
                    'defaults' => [
                        'controller' => ApiController::class,
                        'action'     => 'devices',
                    ],
                ],
            ],
            // GET /api/device/:name → ApiController::deviceAction
            'api-device' => [
                'type'    => Segment::class,     // :name 匹配路径参数，如 /api/device/R1
                'options' => [
                    'route'    => '/api/device/:name',
                    'constraints' => ['name' => '[a-zA-Z0-9._-]+'],
                    'defaults' => [
                        'controller' => ApiController::class,
                        'action'     => 'device',
                    ],
                ],
            ],
            // GET /api/metrics/ifInOctets → 时序数据 折线图 / 饼图
            'api-metrics' => [
                'type'    => Segment::class,
                'options' => [
                    'route'    => '/api/metrics/:name',
                    'constraints' => ['name' => '[a-zA-Z0-9]+'],
                    'defaults' => [
                        'controller' => ApiController::class,
                        'action'     => 'metrics',
                    ],
                ],
            ],
        ],
    ],

    // 控制器注册
    'controllers' => [
        'factories' => [
            // IndexController 不需要依赖注入，直接 Invokable
            IndexController::class => InvokableFactory::class,
            // ApiController 需要 PDO 连接，用匿名工厂注入
            ApiController::class   => function ($container) {
                $config = $container->get('config');
                $db = $config['db'] ?? [];
                $dsn = sprintf('mysql:host=%s;dbname=%s;charset=utf8',
                    $db['host'] ?? 'localhost',
                    $db['name'] ?? 'snmp_monitor');
                $pdo = new PDO($dsn, $db['user'] ?? 'root', $db['password'] ?? '', [
                    PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION,
                    PDO::ATTR_DEFAULT_FETCH_MODE => PDO::FETCH_ASSOC,
                ]);
                return new ApiController($pdo);
            },
        ],
    ],

    // 视图配置
    'view_manager' => [
        'strategies' => ['ViewJsonStrategy'],  // 允许 Action 返回 JsonModel
        'display_not_found_reason' => true,
        'display_exceptions'       => true,
        'doctype'                  => 'HTML5',
        'not_found_template'       => 'error/404',
        'exception_template'       => 'error/index',
        // 模板文件路径映射
        'template_map' => [
            'layout/layout'           => __DIR__ . '/../view/layout/layout.phtml',
            'application/index/index' => __DIR__ . '/../view/application/index/index.phtml',
            'error/404'               => __DIR__ . '/../view/error/404.phtml',
            'error/index'             => __DIR__ . '/../view/error/index.phtml',
        ],
        'template_path_stack' => [
            __DIR__ . '/../view',
        ],
    ],
];
