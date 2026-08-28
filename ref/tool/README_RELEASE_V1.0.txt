BLUETTI Device Studio V1.0 - 正式发布版
============================================================

产品名称：BLUETTI Device Studio
发布版本：V1.0
发布日期：2026-08-20
编辑人：zrj / ChatGPT (OpenAI)
发布基线：BLUETTI Device Studio V1.3.9 最终测试版

本版本为正式发布版本。
V1.0 直接由已完成实机测试的 V1.3.9 基线发布，不重新修改通信与升级业务逻辑。

主要能力：
- Serial OTA：Modbus RTU + XMODEM-1K
- CAN OTA：CANalyst-II 升级链路
- BLE：扫描、连接、设备信息、SOC/功率读取、AC/DC 控制、自定义 Modbus
- BLE OTA：AES 自动加密、Modbus OTA Start、XMODEM-1K、自动重连
- BLE OTA 两阶段进度：PC -> IOT 占前 50%，IOT -> MCU 占后 50%
- 多固件队列：每个固件独立选择芯片平台并读取验证
- 统一自绘选择器与稳定双缓冲界面

正式发布原则：
1. V1.3.9 为功能与行为验证基线。
2. V1.0 仅进行正式版本品牌化、发布信息整理与重新构建。
3. BLE/CAN/Serial/OTA/Modbus 核心业务函数未做逻辑修改。

共同完成，正式发布。
