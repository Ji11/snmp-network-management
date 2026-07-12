const fs = require("fs");
const {
  Document, Packer, Paragraph, TextRun, Table, TableRow, TableCell,
  Footer, AlignmentType, LevelFormat, HeadingLevel,
  BorderStyle, WidthType, ShadingType, PageNumber, PageBreak
} = require("docx");

// ── 读取外部文件 ──
const collectorCfg = fs.readFileSync("config/collector.cfg", "utf8");
const namedCpuSh = fs.readFileSync("aux/named_cpu.sh", "utf8");
const namedMemSh = fs.readFileSync("aux/named_mem.sh", "utf8");
const snmpdConf = fs.readFileSync("aux/snmpd.conf", "utf8");
const bindSetupSh = fs.readFileSync("bind_setup.sh", "utf8");

// ── 复用样式常量 ──
const border = { style: BorderStyle.SINGLE, size: 1, color: "999999" };
const borders = { top: border, bottom: border, left: border, right: border };
const cellMargins = { top: 60, bottom: 60, left: 100, right: 100 };
const headerBg = { fill: "E8F0FE", type: ShadingType.CLEAR };
const codeBg = { fill: "F5F5F5", type: ShadingType.CLEAR };

function p(text, opts = {}) {
  const runs = typeof text === "string"
    ? [new TextRun({ text, ...opts })]
    : text.map(t => new TextRun(t));
  return new Paragraph({ children: runs, spacing: { line: 360 }, ...opts });
}

function heading(level, text) {
  return new Paragraph({
    heading: level,
    children: [new TextRun({ text, font: "SimHei" })],
    spacing: { before: 240, after: 120 },
  });
}

function cell(text, width, opts = {}) {
  const isHeader = opts.header || false;
  return new TableCell({
    borders,
    width: { size: width, type: WidthType.DXA },
    margins: cellMargins,
    shading: isHeader ? headerBg : undefined,
    children: [p([{ text: String(text), bold: isHeader, font: isHeader ? "SimHei" : "SimSun", size: isHeader ? 22 : 20 }])],
  });
}

// 代码块: 小号等宽字体, 灰色背景
function codeBlock(text, maxLen) {
  const lines = text.split("\n");
  const cols = maxLen || Math.max(...lines.map(l => l.length));
  return new Table({
    width: { size: DOC_WIDTH, type: WidthType.DXA },
    columnWidths: [DOC_WIDTH],
    rows: [new TableRow({
      children: [new TableCell({
        borders: { top: border, bottom: border, left: border, right: border },
        width: { size: DOC_WIDTH, type: WidthType.DXA },
        margins: { top: 80, bottom: 80, left: 120, right: 120 },
        shading: codeBg,
        children: lines.map(l => new Paragraph({
          spacing: { line: 240 },
          children: [new TextRun({ text: l || " ", font: "Courier New", size: 16 })],
        })),
      })],
    })],
  });
}

// 标记: 【截图: xxx】红色, 【测试: xxx】蓝色
function marker(text, type) {
  const color = type === "screenshot" ? "CC0000" : "0066CC";
  const label = type === "screenshot" ? "【截图】" : "【测试】";
  return new Paragraph({
    spacing: { before: 120, after: 120 },
    children: [new TextRun({ text: label + " " + text, bold: true, color, font: "SimHei", size: 22 })],
  });
}

const DOC_WIDTH = 9026;
const CW = {
  label: 1800, value: 7226,
  s1: 2200, s2: 3400, s3: 3400,
  eq: 4513,
};

// ═══════════════════════════════════════════════════════════
// 一、实验目的与意义
// ═══════════════════════════════════════════════════════════
const section1 = [
  heading(HeadingLevel.HEADING_1, "一、实验目的与意义"),
  p("本课程实践旨在通过构建企业网络拓扑、配置DNS服务、编写SNMP异步采集程序及开发Web监控前端，全面掌握网络与系统管理的基本手段。具体目标如下："),
  p("1. 掌握在虚拟环境中构建企业级网络拓扑的方法，包括VLAN划分、OSPF动态路由配置及Eth-Trunk链路聚合技术；", { numbering: { reference: "numbers", level: 0 } }),
  p("2. 掌握DNS服务（BIND9）的安装、Zone文件编写与验证方法；", { numbering: { reference: "numbers", level: 0 } }),
  p("3. 掌握基于libnetsnmp库的异步SNMP数据采集编程技术，理解select()事件驱动模型与callback回调机制；", { numbering: { reference: "numbers", level: 0 } }),
  p("4. 掌握Linux守护进程编程规范及fcntl记录锁单实例保证机制；", { numbering: { reference: "numbers", level: 0 } }),
  p("5. 掌握Web MVC框架（Laminas/PHP）的API开发及前端多库协同（vis.js拓扑 + EasyUI表格 + ECharts图表）的数据可视化技术。", { numbering: { reference: "numbers", level: 0 } }),
  p(""),
];

// ═══════════════════════════════════════════════════════════
// 二、实验环境
// ═══════════════════════════════════════════════════════════
const section2 = [
  heading(HeadingLevel.HEADING_1, "二、实验环境"),
  heading(HeadingLevel.HEADING_2, "2.1 软件环境"),
  new Table({
    width: { size: DOC_WIDTH, type: WidthType.DXA },
    columnWidths: [CW.label, CW.value],
    rows: [
      new TableRow({ children: [cell("类别", CW.label, { header: true }), cell("软件/工具", CW.value, { header: true })] }),
      new TableRow({ children: [cell("虚拟仿真平台", CW.label), cell("eNSP (Enterprise Network Simulation Platform)", CW.value)] }),
      new TableRow({ children: [cell("虚拟机", CW.label), cell("Ubuntu 22.04 Server (VMware, 桥接网络)", CW.value)] }),
      new TableRow({ children: [cell("DNS 服务", CW.label), cell("BIND9 (named)", CW.value)] }),
      new TableRow({ children: [cell("采集程序语言", CW.label), cell("C语言 (gcc)", CW.value)] }),
      new TableRow({ children: [cell("SNMP 库", CW.label), cell("libnetsnmp (Net-SNMP v5.9)", CW.value)] }),
      new TableRow({ children: [cell("配置解析", CW.label), cell("libconfig", CW.value)] }),
      new TableRow({ children: [cell("数据库", CW.label), cell("MySQL 8.0 (libmysqlclient)", CW.value)] }),
      new TableRow({ children: [cell("构建工具", CW.label), cell("Make", CW.value)] }),
      new TableRow({ children: [cell("Web 框架", CW.label), cell("Laminas MVC (PHP 8.1) + nginx", CW.value)] }),
      new TableRow({ children: [cell("前端图表库", CW.label), cell("vis.js 9.1.6 / EasyUI / ECharts 5.5.0", CW.value)] }),
      new TableRow({ children: [cell("SNMP 协议版本", CW.label), cell("SNMP v2c, community: bjfu11mx", CW.value)] }),
    ],
  }),
  p(""),
  heading(HeadingLevel.HEADING_2, "2.2 硬件设备型号"),
  new Table({
    width: { size: DOC_WIDTH, type: WidthType.DXA },
    columnWidths: [CW.s1, CW.s2, CW.s3],
    rows: [
      new TableRow({ children: [cell("设备名", CW.s1, { header: true }), cell("型号", CW.s2, { header: true }), cell("管理 IP", CW.s3, { header: true })] }),
      new TableRow({ children: [cell("R1", CW.s1), cell("AR2220 路由器", CW.s2), cell("10.1.1.25", CW.s3)] }),
      new TableRow({ children: [cell("C1", CW.s1), cell("CE12800 核心交换机", CW.s2), cell("10.1.1.14", CW.s3)] }),
      new TableRow({ children: [cell("C2",CW.s1), cell("CE12800 核心交换机", CW.s2), cell("10.1.1.18", CW.s3)] }),
      new TableRow({ children: [cell("D1", CW.s1), cell("S3700 汇聚交换机", CW.s2), cell("10.1.1.6", CW.s3)] }),
      new TableRow({ children: [cell("D2", CW.s1), cell("S3700 汇聚交换机", CW.s2), cell("10.1.1.10", CW.s3)] }),
      new TableRow({ children: [cell("VM", CW.s1), cell("Ubuntu 22.04 VM", CW.s2), cell("192.168.179.136", CW.s3)] }),
    ],
  }),
  p(""),
];

// ═══════════════════════════════════════════════════════════
// 三、企业内部网络拓扑构建
// ═══════════════════════════════════════════════════════════
const section3 = [
  heading(HeadingLevel.HEADING_1, "三、企业内部网络拓扑构建"),
  heading(HeadingLevel.HEADING_2, "3.1 拓扑架构"),
  p("网络拓扑采用分层设计：核心层（C1/C2 CE12800双核心）、汇聚层（D1/D2 S3700）、接入层（4台PC终端）。核心层通过Eth-Trunk链路聚合互联，提供核心层冗余。全网运行OSPF动态路由协议（Area 0），R1通过import-route static将InterNet1的默认路由注入OSPF域。VM通过Cloud1桥接至C2的Vlanif10接口。"),
  heading(HeadingLevel.HEADING_2, "3.2 IP 地址与 VLAN 规划"),
  p("全网划分为10个网段，各VLAN对应独立的IP子网："),
  new Table({
    width: { size: DOC_WIDTH, type: WidthType.DXA },
    columnWidths: [1600, 2400, 2500, 2500],
    rows: [
      new TableRow({ children: [cell("VLAN", 1600, { header: true }), cell("网段", 2400, { header: true }), cell("用途", 2500, { header: true }), cell("网关", 2500, { header: true })] }),
      new TableRow({ children: [cell("VLAN 10", 1600), cell("10.1.1.0/24", 2400), cell("管理网段（eNSP设备）", 2500), cell("10.1.1.1", 2500)] }),
      new TableRow({ children: [cell("VLAN 20", 1600), cell("192.168.2.0/24", 2400), cell("PC-A 接入", 2500), cell("192.168.2.1", 2500)] }),
      new TableRow({ children: [cell("VLAN 30", 1600), cell("192.168.3.0/24", 2400), cell("PC-B 接入", 2500), cell("192.168.3.1", 2500)] }),
      new TableRow({ children: [cell("VLAN 40", 1600), cell("192.168.4.0/24", 2400), cell("PC-C 接入", 2500), cell("192.168.4.1", 2500)] }),
      new TableRow({ children: [cell("VLAN 50", 1600), cell("192.168.5.0/24", 2400), cell("PC-D 接入", 2500), cell("192.168.5.1", 2500)] }),
      new TableRow({ children: [cell("VLAN 101", 1600), cell("10.1.2.0/24", 2400), cell("C1-D1 互联", 2500), cell("10.1.2.254", 2500)] }),
      new TableRow({ children: [cell("VLAN 102", 1600), cell("10.1.3.0/24", 2400), cell("C2-D2 互联", 2500), cell("10.1.3.254", 2500)] }),
      new TableRow({ children: [cell("VLAN 200", 1600), cell("10.1.4.0/24", 2400), cell("C1-C2 Eth-Trunk1", 2500), cell("10.1.4.254", 2500)] }),
      new TableRow({ children: [cell("VLAN 201", 1600), cell("10.1.5.0/24", 2400), cell("C1-C2 Eth-Trunk1", 2500), cell("10.1.5.254", 2500)] }),
      new TableRow({ children: [cell("VM桥接", 1600), cell("192.168.179.0/24", 2400), cell("桥接至C2 Vlanif10", 2500), cell("192.168.179.2", 2500)] }),
    ],
  }),
  p(""),
  heading(HeadingLevel.HEADING_2, "3.3 路由设计"),
  p("全网运行OSPF单区域（Area 0），R1通过import-route static将InterNet1的默认路由注入OSPF域。C1/C2之间运行Eth-Trunk链路聚合，提供核心层冗余。C1连接D1，C2连接D2，汇聚层交换机以Vlanif接口作为各VLAN的网关，为终端PC提供三层路由服务。"),
  heading(HeadingLevel.HEADING_2, "3.4 连通性验证"),
  p("全部设备完成基础配置与OSPF邻居建立后，通过以下命令验证全网连通性："),
  p("(1) 各设备间互ping验证三层可达", { numbering: { reference: "bullets", level: 0 } }),
  p("(2) display ospf peer brief 检查OSPF邻居状态为Full", { numbering: { reference: "bullets", level: 0 } }),
  p("(3) display ip routing-table 确认每台设备拥有全网路由条目", { numbering: { reference: "bullets", level: 0 } }),
  p("(4) 终端PC ping VM验证端到端可达", { numbering: { reference: "bullets", level: 0 } }),
  marker("eNSP 拓扑全貌（含设备互联、VLAN划分、路由表验证）", "screenshot"),
  p(""),
];

// ═══════════════════════════════════════════════════════════
// 四、DNS 服务配置
// ═══════════════════════════════════════════════════════════
const section4 = [
  heading(HeadingLevel.HEADING_1, "四、DNS 服务配置"),
  heading(HeadingLevel.HEADING_2, "4.1 BIND9 安装与部署"),
  p("在Ubuntu虚拟机中安装BIND9 DNS服务器，通过自动化脚本bind_setup.sh完成全部部署流程。脚本内容如下："),
  codeBlock(bindSetupSh),
  p(""),
  p("脚本执行流程："),
  p("(1) apt install bind9 \u2014 安装BIND9软件包", { numbering: { reference: "bullets", level: 0 } }),
  p("(2) 在named.conf.local中添加bjfu.edu.cn的Zone声明（type: master, file: /etc/bind/db.bjfu.edu.cn）", { numbering: { reference: "bullets", level: 0 } }),
  p("(3) 创建Zone文件，定义ns/r1/c1/c2/d1/d2共6条A记录", { numbering: { reference: "bullets", level: 0 } }),
  p("(4) 重启named服务并逐一验证nslookup和ping", { numbering: { reference: "bullets", level: 0 } }),

  heading(HeadingLevel.HEADING_2, "4.2 DNS Zone 配置"),
  p("Zone文件 /etc/bind/db.bjfu.edu.cn 定义了bjfu.edu.cn域的正向解析记录，包含以下A记录："),
  new Table({
    width: { size: DOC_WIDTH, type: WidthType.DXA },
    columnWidths: [CW.eq, CW.eq],
    rows: [
      new TableRow({ children: [cell("域名", CW.eq, { header: true }), cell("IP 地址", CW.eq, { header: true })] }),
      new TableRow({ children: [cell("ns.bjfu.edu.cn", CW.eq), cell("192.168.179.136 (VM)", CW.eq)] }),
      new TableRow({ children: [cell("r1.bjfu.edu.cn", CW.eq), cell("10.1.1.25", CW.eq)] }),
      new TableRow({ children: [cell("c1.bjfu.edu.cn", CW.eq), cell("10.1.1.14", CW.eq)] }),
      new TableRow({ children: [cell("c2.bjfu.edu.cn", CW.eq), cell("10.1.1.18", CW.eq)] }),
      new TableRow({ children: [cell("d1.bjfu.edu.cn", CW.eq), cell("10.1.1.6", CW.eq)] }),
      new TableRow({ children: [cell("d2.bjfu.edu.cn", CW.eq), cell("10.1.1.10", CW.eq)] }),
    ],
  }),
  p(""),

  heading(HeadingLevel.HEADING_2, "4.3 DNS 验证"),
  p("使用nslookup工具验证DNS解析功能，在VM上执行以下命令："),
  codeBlock("nslookup\nserver 192.168.179.136\nc1.bjfu.edu.cn\nd1.bjfu.edu.cn"),
  p(""),
  marker("在VM上执行 nslookup c1.bjfu.edu.cn，应返回10.1.1.14", "test"),
  marker("在VM上执行 ping c1.bjfu.edu.cn，验证DNS+连通性", "test"),
  marker("DNS解析验证结果（nslookup + ping 域名截图）", "screenshot"),
  p(""),

  heading(HeadingLevel.HEADING_2, "4.4 DNS 进程资源监控"),
  p("为满足采集named进程CPU时间和内存占用的需求，编写两个Shell脚本并注册到snmpd的exec扩展中。"),
  p(""),
  p("脚本1 \u2014 named_cpu.sh（CPU 时间采集）："),
  codeBlock(namedCpuSh),
  p(""),
  p("脚本2 \u2014 named_mem.sh（RSS 内存采集）："),
  codeBlock(namedMemSh),
  p(""),
  p("VM端snmpd配置文件 /etc/snmp/snmpd.conf 完整内容："),
  codeBlock(snmpdConf),
  p(""),
  p("部署步骤：将named_cpu.sh和named_mem.sh复制到/usr/local/bin/并赋予执行权限(chmod +x)，将snmpd.conf复制到/etc/snmp/snmpd.conf并重启snmpd(systemctl restart snmpd)。两个exec脚本注册为extResult.1（named CPU时间，OID .1.3.6.1.4.1.2021.8.1.101.1）和extResult.2（named RSS内存，OID .1.3.6.1.4.1.2021.8.1.101.2），采集程序后续即可通过SNMP轮询获取named进程的运行数据。"),
  marker("在VM上执行 snmpwalk -v2c -c bjfu11mx localhost .1.3.6.1.4.1.2021.8.1.101，验证exec扩展OID有返回值", "test"),
  p(""),
];

// ═══════════════════════════════════════════════════════════
// 五、基于SNMP协议的异步数据采集
// ═══════════════════════════════════════════════════════════
const section5 = [
  heading(HeadingLevel.HEADING_1, "五、基于 SNMP 协议的异步数据采集"),
  p("本系统的数据采集层采用C语言编写，使用libnetsnmp库的异步API实现并发SNMP数据采集。采集程序由四个源文件组成：main.c（守护进程入口+fcntl单实例锁）、config_loader.c（libconfig配置解析）、async_collector.c（select()事件循环+callback异步采集）、db.c（MySQL建库建表+INSERT）。"),
  p("项目使用Makefile管理编译，依赖libconfig、libnetsnmp、libmysqlclient三个库。编译命令：make"),

  heading(HeadingLevel.HEADING_2, "5.1 异步采集机制"),
  p("异步采集基于Net-SNMP的asynchapp.c模式实现。程序为每个设备创建独立的SNMP v2c会话（snmp_open），并行发出GET请求（snmp_send），然后通过select()事件循环等待所有会话的异步响应。响应到达时，Net-SNMP库自动调用预注册的回调函数response_callback()，在其中解析PDU变量（snprint_value格式化值）并将结果存入数据库（db_insert），随后推进到该设备的下一个OID指标继续发送请求。所有设备的全部指标采集完毕后退出select循环，完成一轮采集。"),
  p("与同步采集相比，异步方式的优势在于：所有设备的SNMP请求是并发的（非串行逐个等待），大幅缩短了每轮采集的总耗时，特别适合监控设备数量较多的场景。"),
  p("每台网络设备采集的9项接口指标为：ifInOctets, ifInUcastPkts, ifInNUcastPkts, ifInErrors, ifOutOctets, ifOutUcastPkts, ifOutNUcastPkts, ifOutErrors，加sysName共9项/接口。VM采集7项指标（memTotalReal, memAvailReal, dskTotal, memTotalSwap, dskUsed, namedCPU, namedRSS）。总计配置138+项SNMP指标。"),

  heading(HeadingLevel.HEADING_2, "5.2 守护进程实现"),
  p("采集程序使用标准库函数daemon(1, 0)实现守护进程化。该函数内部完成经典的守护进程6个步骤：fork\u2192setsid\u2192二次fork\u2192chdir（参数1表示不chdir到/，保持当前工作目录）\u2192关闭stdin/stdout/stderr\u2192重定向/dev/null。"),
  p("守护进程化后，程序进入无限循环：每60秒调用一次collect_async()执行一轮SNMP异步采集，然后sleep(60)等待下一轮。通过SIGTERM（kill命令）和SIGINT（Ctrl+C）信号处理函数设置退出标志running=0，实现优雅退出，退出时释放fcntl锁并删除PID文件。"),

  heading(HeadingLevel.HEADING_2, "5.3 单实例保证机制"),
  p("为防止多个采集进程同时运行导致重复写入数据库，程序使用fcntl()记录锁实现单实例保证。核心代码（main.c中的lock_pidfile函数）："),
  codeBlock("int lock_pidfile(char *path) {\n    int fd = open(path, O_RDWR | O_CREAT, 0644);\n\n    struct flock lock;\n    lock.l_type   = F_WRLCK;   // 排他写锁\n    lock.l_whence = SEEK_SET;  // 从文件开头\n    lock.l_start  = 0;\n    lock.l_len    = 0;         // 0 = 锁定整个文件\n\n    if (fcntl(fd, F_SETLK, &lock) != 0) {\n        // 锁被占用, 读取已有PID告知用户\n        char buf[32];\n        ssize_t n = read(fd, buf, sizeof(buf) - 1);\n        buf[n] = '\\0';\n        char *nl = strchr(buf, '\\n');\n        if (nl) *nl = '\\0';\n        fprintf(stderr, \"another process is running (pid=%s)\\n\", buf);\n        close(fd);\n        return -1;\n    }\n\n    // 锁成功, 清空文件并写入当前PID\n    ftruncate(fd, 0);\n    char buf[32];\n    snprintf(buf, sizeof(buf), \"%d\\n\", getpid());\n    write(fd, buf, strlen(buf));\n    return fd;\n}"),
  p(""),
  p("实现要点："),
  p("(1) F_WRLCK排他写锁 + F_SETLK非阻塞模式：若锁已被占用，立即返回错误而非阻塞等待", { numbering: { reference: "numbers", level: 0 } }),
  p("(2) PID文件路径：/tmp/async_collector.pid", { numbering: { reference: "numbers", level: 0 } }),
  p("(3) 加锁成功后用ftruncate清空文件，再写入当前进程PID", { numbering: { reference: "numbers", level: 0 } }),
  p("(4) 退出时发送F_UNLCK释放锁，并unlink删除PID文件", { numbering: { reference: "numbers", level: 0 } }),
  p("(5) 关键细节：加锁操作必须在daemon()调用之后进行。因为fcntl记录锁绑定(inode, PID)，fork产生的子进程拥有新PID，父进程持有的锁不会被继承到子进程。daemon()内部两次fork，若在其之前加锁则锁在daemon化后失效。", { numbering: { reference: "numbers", level: 0 } }),

  heading(HeadingLevel.HEADING_2, "5.4 配置文件设计"),
  p("使用libconfig库解析结构化配置文件collector.cfg。完整配置内容如下："),
  codeBlock(collectorCfg),
  p(""),
  p("配置结构说明："),
  p("database节 \u2014 MySQL连接参数（host/user/password/name）", { numbering: { reference: "bullets", level: 0 } }),
  p("devices数组 \u2014 6个设备（R1/C1/C2/D1/D2/VM），每个包含name/peer/community和metrics数组", { numbering: { reference: "bullets", level: 0 } }),
  p("metrics数组 \u2014 每项含name（指标名）和oid（点分十进制OID字符串），程序加载时调用snmp_parse_oid()解析为数值oid数组", { numbering: { reference: "bullets", level: 0 } }),
  p("R1 28项 / C1 25项 / C2 32项 / D1 23项 / D2 23项 / VM 7项，合计138+项SNMP指标", { numbering: { reference: "bullets", level: 0 } }),

  heading(HeadingLevel.HEADING_2, "5.5 数据库存储设计"),
  p("数据存储使用MySQL 8.0，数据库名为snmp_monitor。程序启动时（db_connect函数）自动建库建表，无需手动执行SQL脚本。核心存储表结构："),
  p(""),
  p("表名：snmp_data"),
  new Table({
    width: { size: DOC_WIDTH, type: WidthType.DXA },
    columnWidths: [1600, 1600, 5800],
    rows: [
      new TableRow({ children: [cell("字段", 1600, { header: true }), cell("类型", 1600, { header: true }), cell("说明", 5800, { header: true })] }),
      new TableRow({ children: [cell("id", 1600), cell("INT AUTO_INCREMENT", 1600), cell("主键，自增", 5800)] }),
      new TableRow({ children: [cell("device", 1600), cell("VARCHAR(32)", 1600), cell("设备名称", 5800)] }),
      new TableRow({ children: [cell("metric", 1600), cell("VARCHAR(64)", 1600), cell("指标名称（如 ifInOctets.6）", 5800)] }),
      new TableRow({ children: [cell("oid", 1600), cell("VARCHAR(128)", 1600), cell("SNMP OID字符串", 5800)] }),
      new TableRow({ children: [cell("value", 1600), cell("VARCHAR(256)", 1600), cell("采集值", 5800)] }),
      new TableRow({ children: [cell("created_at", 1600), cell("DATETIME", 1600), cell("采集时间戳，DEFAULT CURRENT_TIMESTAMP", 5800)] }),
    ],
  }),
  p(""),
  p("每次采集执行INSERT新增记录，使用mysql_real_escape_string()对所有字符串字段进行转义防止SQL注入。不更新已有记录，保留完整的历史时序数据用于图表绘制。"),

  heading(HeadingLevel.HEADING_2, "5.6 程序验证"),
  p("编译与运行验证："),
  marker("编译: cd /path/to/CourseDesign && make", "test"),
  marker("启动采集: ./build/async_collector config/collector.cfg", "test"),
  marker("ps aux | grep async_collector \u2014 确认守护进程正在运行，应显示1个进程", "test"),
  marker("第二次启动 ./build/async_collector config/collector.cfg \u2014 应提示 another process is running 并退出", "test"),
  marker("mysql -u root -p -e \"SELECT COUNT(*) FROM snmp_monitor.snmp_data\" \u2014 确认数据库有新数据写入", "test"),
  p(""),
  marker("守护进程运行状态截图（ps aux | grep async_collector）", "screenshot"),
  marker("单实例验证截图（第二次启动报错信息）", "screenshot"),
  marker("MySQL数据截图（SELECT * FROM snmp_data ORDER BY created_at DESC LIMIT 20）", "screenshot"),
  p(""),
];

// ═══════════════════════════════════════════════════════════
// 六、数据采集与 Web 显示
// ═══════════════════════════════════════════════════════════
const section6 = [
  heading(HeadingLevel.HEADING_1, "六、数据采集与 Web 显示"),
  heading(HeadingLevel.HEADING_2, "6.1 后端 MVC API 设计"),
  p("Web后端采用PHP Laminas MVC框架，nginx作为反向代理（配置见web/nginx-site.conf），PHP-FPM处理请求。控制器ApiController通过构造函数注入PDO数据库连接（匿名工厂在module.config.php中配置），提供4个JSON REST API："),
  new Table({
    width: { size: DOC_WIDTH, type: WidthType.DXA },
    columnWidths: [2400, 1200, 5400],
    rows: [
      new TableRow({ children: [cell("路由", 2400, { header: true }), cell("方法", 1200, { header: true }), cell("功能", 5400, { header: true })] }),
      new TableRow({ children: [cell("GET /api/topology", 2400), cell("GET", 1200), cell("返回拓扑JSON：12个节点 + 12条边，ICMP ping决定颜色", 5400)] }),
      new TableRow({ children: [cell("GET /api/devices", 2400), cell("GET", 1200), cell("所有设备最新指标（子查询MAX(created_at)），按设备名分组", 5400)] }),
      new TableRow({ children: [cell("GET /api/device/{name}", 2400), cell("GET", 1200), cell("单设备指标详情（所有指标当前最新值）", 5400)] }),
      new TableRow({ children: [cell("GET /api/metrics/{name}", 2400), cell("GET", 1200), cell("时序数据（最近300条），白名单校验指标名", 5400)] }),
    ],
  }),
  p(""),
  p("安全设计：所有API使用PDO参数化查询（prepare + execute占位符绑定），/api/metrics路由对指标名做白名单校验（仅允许ifInOctets/ifOutOctets/memTotalReal/memAvailReal/dskTotal/dskUsed），/api/topology中的ping命令使用escapeshellarg()防止命令注入。"),

  heading(HeadingLevel.HEADING_2, "6.2 数据 List 显示"),
  p("使用EasyUI DataGrid组件以表格形式展示所有设备的实时监控数据。前端通过loadFilter回调将API返回的{devices: {R1: [...], ...}}分组格式转换为Flat表格行。每行显示：设备名、系统名称(sysName)、入流量汇总（所有接口ifInOctets总和）、出流量汇总（所有接口ifOutOctets总和）、指标数、最近更新时间。流量数值通过formatTraffic()函数自动转换显示单位（B/KB/MB/GB）。"),
  p("每行附带\u201C查看\u201D按钮，点击弹出EasyUI Dialog详情窗口，展示该设备所有指标的汇总表（接口指标按基名聚合统计总数+接口数）和分接口明细表（逐指标显示metric/oid/value/time）。"),

  heading(HeadingLevel.HEADING_2, "6.3 图形化显示"),
  p("采用ECharts实现两组数据可视化图表，每60秒自动刷新："),
  p("(1) ifInOctets折线图：每个\u201C设备.接口\u201D组合绘制一条时序折线。前端按\u201C设备名.接口后缀\u201D分组，过滤掉全零数据的接口，X轴为采集时间，Y轴为Octets值。支持图例滚动、窗口自适应resize。", { numbering: { reference: "numbers", level: 0 } }),
  p("(2) VM物理内存饼图：从API获取memTotalReal（总内存）和memAvailReal（可用内存），计算已用=总-可用，以实心饼图展示已用/可用比例及百分比。", { numbering: { reference: "numbers", level: 0 } }),

  heading(HeadingLevel.HEADING_2, "6.4 Web 页面验证"),
  p("在浏览器中访问Web首页（http://<服务器IP>），应看到四栏布局：左上拓扑图、右上设备表格、左下ifInOctets折线图、右下VM内存饼图。"),
  marker("浏览器访问首页，完整四栏布局（拓扑+表格+折线图+饼图）", "screenshot"),
  marker("点击某个设备的\u201C查看\u201D按钮，弹出详情汇总表+分接口明细表", "screenshot"),
  marker("ECharts折线图（有多条设备.接口曲线）", "screenshot"),
  marker("ECharts饼图（VM 已用/可用内存）", "screenshot"),
  p(""),
];

// ═══════════════════════════════════════════════════════════
// 七、JS拓扑结构监控
// ═══════════════════════════════════════════════════════════
const section7 = [
  heading(HeadingLevel.HEADING_1, "七、JS 拓扑结构监控"),
  heading(HeadingLevel.HEADING_2, "7.1 静态拓扑设计"),
  p("使用vis.js Network组件渲染网络拓扑图。拓扑定义12个节点（InterNet1 / R1 / C1 / C2 / D1 / D2 / Cloud1 / VM / PC-A / PC-B / PC-C / PC-D）和12条连线，对应eNSP拓扑的实际物理连接关系。"),
  p("节点按设备类型分为5个分组（groups），不同类型使用不同颜色和形状：router（橙色方框）、switch（蓝色方框）、pc（绿色椭圆）、cloud（灰色三角）、server（紫色菱形）。"),
  p("所有节点使用固定坐标（positions）定位，关闭物理引擎（physics: false），节点不可拖拽、视图不可缩放/平移、节点和边不可选中（interaction: dragNodes/dragView/zoomView/selectable均设为false），确保拓扑展示的稳定性和一致性。"),

  heading(HeadingLevel.HEADING_2, "7.2 动态拓扑检测"),
  p("动态拓扑检测的核心机制是ICMP ping响应，每60秒刷新一次："),
  p("(1) 前端每60秒调用GET /api/topology，后端遍历12个节点（Cloud1无IP除外），对每个有IP的设备执行ping -c 3 -W 1检测连通性", { numbering: { reference: "numbers", level: 0 } }),
  p("(2) ping输出末尾行包含丢包统计（如\u201C0% packet loss\u201D或\u201C100% packet loss\u201D），100%丢包时该节点标记为红色(red)，否则为绿色(green)", { numbering: { reference: "numbers", level: 0 } }),
  p("(3) 首次加载时渲染全部节点和边的完整拓扑（含固定坐标），后续刷新仅调用nodesDS.update()更新节点颜色，不重建拓扑结构", { numbering: { reference: "numbers", level: 0 } }),
  p("(4) 当网络中某个节点被关闭或链路断连时，下一次60秒轮询后该节点及关联不可达节点将变为红色，实现动态拓扑状态的可视化展示", { numbering: { reference: "numbers", level: 0 } }),

  heading(HeadingLevel.HEADING_2, "7.3 动态拓扑验证"),
  marker("浏览器访问首页，确认所有节点为绿色（全网连通状态）", "screenshot"),
  marker("在eNSP中 shutdown C1 的 G0/0/0 接口，等待60秒后刷新页面或等待自动刷新", "test"),
  marker("C1节点及通过C1连接的D1、PC-A、PC-B变为红色（动态拓扑检测生效）", "screenshot"),
  marker("恢复C1接口，等待60秒后确认节点恢复绿色", "test"),
  p(""),
];

// ═══════════════════════════════════════════════════════════
// 八、总结与收获
// ═══════════════════════════════════════════════════════════
const section8 = [
  heading(HeadingLevel.HEADING_1, "八、总结与收获"),
  p("通过本次网络管理课程实践，从底层到前端完整实现了一套SNMP网络监控系统，主要收获如下："),
  p("1. 掌握了在eNSP虚拟环境中构建分层企业网络拓扑的完整流程，包括VLAN划分、OSPF单区域路由设计、Eth-Trunk链路聚合等关键技术；", { numbering: { reference: "numbers", level: 0 } }),
  p("2. 掌握了BIND9 DNS服务器的安装部署、Zone文件编写及nslookup/ping域名解析验证方法，以及通过snmpd exec扩展采集自定义进程指标的技巧；", { numbering: { reference: "numbers", level: 0 } }),
  p("3. 深入理解了SNMP v2c协议及libnetsnmp异步API的使用，掌握了select()事件循环驱动并发采集的编程模式；", { numbering: { reference: "numbers", level: 0 } }),
  p("4. 掌握了Linux守护进程编程规范（daemon()函数）、fcntl记录锁单实例保证机制及其与fork交互的底层原理（锁绑定inode+PID，fork后不可继承）；", { numbering: { reference: "numbers", level: 0 } }),
  p("5. 掌握了PHP Laminas MVC框架的API开发、PDO安全数据库操作，以及前端多库协同（vis.js拓扑 + EasyUI表格 + ECharts图表）的数据可视化技术。", { numbering: { reference: "numbers", level: 0 } }),
  p(""),
  p("课程实践综合运用了网络工程专业多门课程的知识，包括计算机网络（OSPF/BGP/VLAN）、Linux系统管理（守护进程/文件锁/snmpd/DNS）、数据库原理（MySQL建表/参数化查询）、Web开发（MVC架构/前后端分离/CDN集成），实现了理论与实践的结合。"),
  p(""),
];

// ═══════════════════════════════════════════════════════════
// 附录: 截图与验证清单
// ═══════════════════════════════════════════════════════════
const appendix = [
  new Paragraph({ children: [new PageBreak()] }),
  heading(HeadingLevel.HEADING_1, "附录：截图与验证清单"),
  p("以下列出实践报告各章节需要准备的截图和验证项目，答辩时需逐项展示。"),

  heading(HeadingLevel.HEADING_2, "A.1 拓扑与连通性（第三章）"),
  p("1. eNSP 拓扑全貌（含设备互联线缆、VLAN划分标注）", { numbering: { reference: "numbers", level: 0 } }),
  p("2. display ospf peer brief 输出（OSPF邻居Full状态）", { numbering: { reference: "numbers", level: 0 } }),
  p("3. PC-A ping VM（端到端连通性验证）", { numbering: { reference: "numbers", level: 0 } }),

  heading(HeadingLevel.HEADING_2, "A.2 DNS 服务（第四章）"),
  p("4. nslookup c1.bjfu.edu.cn 输出（应返回10.1.1.14）", { numbering: { reference: "numbers", level: 0 } }),
  p("5. ping c1.bjfu.edu.cn 输出（域名解析+网络连通）", { numbering: { reference: "numbers", level: 0 } }),
  p("6. snmpwalk -v2c -c bjfu11mx localhost .1.3.6.1.4.1.2021.8.1.101（exec扩展OID返回值）", { numbering: { reference: "numbers", level: 0 } }),

  heading(HeadingLevel.HEADING_2, "A.3 数据采集程序（第五章）"),
  p("7. make 编译输出（无编译错误）", { numbering: { reference: "numbers", level: 0 } }),
  p("8. ps aux | grep async_collector（守护进程运行状态，1个进程）", { numbering: { reference: "numbers", level: 0 } }),
  p("9. 第二次启动程序报错 \u201Canother process is running\u201D（单实例验证）", { numbering: { reference: "numbers", level: 0 } }),
  p("10. MySQL查询：SELECT COUNT(*), MAX(created_at) FROM snmp_data（数据持续写入验证）", { numbering: { reference: "numbers", level: 0 } }),

  heading(HeadingLevel.HEADING_2, "A.4 Web 数据展示（第六章）"),
  p("11. 浏览器首页完整截图（左上拓扑 + 右上EasyUI表格 + 左下ECharts折线图 + 右下ECharts饼图）", { numbering: { reference: "numbers", level: 0 } }),
  p("12. 设备详情弹窗截图（点击\u201C查看\u201D弹出汇总表+分接口明细表）", { numbering: { reference: "numbers", level: 0 } }),

  heading(HeadingLevel.HEADING_2, "A.5 动态拓扑检测（第七章）"),
  p("13. 正常状态拓扑图（所有节点绿色）", { numbering: { reference: "numbers", level: 0 } }),
  p("14. 关闭C1后拓扑图（C1/D1/PC-A/PC-B变红）", { numbering: { reference: "numbers", level: 0 } }),

  p(""),
  p("以上共14项验证/截图。请在答辩前逐项完成并准备好截图文件。"),
];


// ═══════════════════════════════════════════════════════════
// 组装文档
// ═══════════════════════════════════════════════════════════

// 标题页（第一页顶部，无单独封面）
const titlePage = [
  p([{ text: "网络管理课程实践报告", size: 36, bold: true, font: "SimHei" }], { alignment: AlignmentType.CENTER, spacing: { after: 300 } }),
  p([{ text: "基于 SNMP 的网络监控系统设计与实现", size: 24, font: "SimHei" }], { alignment: AlignmentType.CENTER, spacing: { after: 600 } }),
  p([{ text: "姓名：____________    班级：____________    学号：____________", size: 24, font: "SimSun" }], { alignment: AlignmentType.CENTER, spacing: { line: 480 } }),
  p([{ text: "完成方法：方法 A（异步数据采集）", size: 24, font: "SimSun" }], { alignment: AlignmentType.CENTER, spacing: { line: 480 } }),
  p(""),
  new Paragraph({
    spacing: { after: 200 },
    border: { bottom: { style: BorderStyle.SINGLE, size: 6, color: "2E75B6", space: 1 } },
    children: [],
  }),
  p(""),
];

const doc = new Document({
  styles: {
    default: { document: { run: { font: "SimSun", size: 24 } } },
    paragraphStyles: [
      {
        id: "Heading1", name: "Heading 1", basedOn: "Normal", next: "Normal", quickFormat: true,
        run: { size: 32, bold: true, font: "SimHei" },
        paragraph: { spacing: { before: 360, after: 200 }, outlineLevel: 0 },
      },
      {
        id: "Heading2", name: "Heading 2", basedOn: "Normal", next: "Normal", quickFormat: true,
        run: { size: 28, bold: true, font: "SimHei" },
        paragraph: { spacing: { before: 240, after: 160 }, outlineLevel: 1 },
      },
    ],
  },
  numbering: {
    config: [
      {
        reference: "bullets",
        levels: [{ level: 0, format: LevelFormat.BULLET, text: "\u2022", alignment: AlignmentType.LEFT, style: { paragraph: { indent: { left: 720, hanging: 360 } } } }],
      },
      {
        reference: "numbers",
        levels: [{ level: 0, format: LevelFormat.DECIMAL, text: "%1.", alignment: AlignmentType.LEFT, style: { paragraph: { indent: { left: 720, hanging: 360 } } } }],
      },
    ],
  },
  sections: [
    {
      properties: {
        page: {
          size: { width: 11906, height: 16838 },
          margin: { top: 1440, right: 1440, bottom: 1440, left: 1440 },
        },
      },
      footers: {
        default: new Footer({
          children: [new Paragraph({
            alignment: AlignmentType.CENTER,
            children: [new TextRun({ text: "第 ", size: 18 }), new TextRun({ children: [PageNumber.CURRENT], size: 18 }), new TextRun({ text: " 页", size: 18 })],
          })],
        }),
      },
      children: [
        ...titlePage,
        ...section1,
        ...section2,
        ...section3,
        ...section4,
        ...section5,
        ...section6,
        ...section7,
        ...section8,
        ...appendix,
      ],
    },
  ],
});

Packer.toBuffer(doc).then(buf => {
  const out = "/home/mx/NetProgramming/CourseDesign/网络管理课程实践报告.docx";
  fs.writeFileSync(out, buf);
  console.log("OK: " + out);
});
