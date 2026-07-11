# CourseDesign

## 目录约定
- `src/`：源代码
- `include/`：头文件
- `config/`：配置样例
- `build/`：本地构建产物，不提交
- `aux/`：VM 端辅助脚本（snmpd配置、named采集脚本）
- `web/`：Laminas 前端项目（PHP），不提交课程说明内容

## 命名约定
- C 源文件使用小写下划线命名
- 可执行程序名与功能一致，保持简洁
- 配置文件使用 `.cfg` 后缀

## 本项目实现约束
- 默认使用 C 语言（采集层）
- 配置解析使用 `libconfig`
- SNMP 采集使用 `libnetsnmp`
- MySQL 存储使用 `libmysqlclient`
- SNMP community 统一为 `bjfu11mx`
- 前端使用 Laminas (PHP) + vis.js + EasyUI + ECharts
- 不写与课程目标无关的抽象层

## 验证约定
- 改完至少执行编译检查
- 若本机缺少依赖，说明缺少项与编译命令，不绕过报错
- SNMP 采集验证：`./build/snmp_collector config/collector.cfg`
- 若 VM 端修改 C 源码，需 scp 同步到 VM 后重新 make
