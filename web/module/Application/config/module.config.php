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
    'router' => [
        'routes' => [
            'home' => [
                'type'    => Literal::class,
                'options' => [
                    'route'    => '/',
                    'defaults' => [
                        'controller' => IndexController::class,
                        'action'     => 'index',
                    ],
                ],
            ],
            'api-topology' => [
                'type'    => Literal::class,
                'options' => [
                    'route'    => '/api/topology',
                    'defaults' => [
                        'controller' => ApiController::class,
                        'action'     => 'topology',
                    ],
                ],
            ],
            'api-devices' => [
                'type'    => Literal::class,
                'options' => [
                    'route'    => '/api/devices',
                    'defaults' => [
                        'controller' => ApiController::class,
                        'action'     => 'devices',
                    ],
                ],
            ],
            'api-device' => [
                'type'    => Segment::class,
                'options' => [
                    'route'    => '/api/device/:name',
                    'constraints' => ['name' => '[a-zA-Z0-9._-]+'],
                    'defaults' => [
                        'controller' => ApiController::class,
                        'action'     => 'device',
                    ],
                ],
            ],
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
    'controllers' => [
        'factories' => [
            IndexController::class => InvokableFactory::class,
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
    'view_manager' => [
        'strategies' => ['ViewJsonStrategy'],
        'display_not_found_reason' => true,
        'display_exceptions'       => true,
        'doctype'                  => 'HTML5',
        'not_found_template'       => 'error/404',
        'exception_template'       => 'error/index',
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
