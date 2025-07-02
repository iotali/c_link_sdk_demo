# LinkSDK 修改版本使用说明

## 项目简介

本项目基于阿里云物联网平台的 LinkSDK，通过一些必要的修改（如更换证书、调整连接参数等），实现了与新 IoT 平台的无缝对接。您可以利用这个修改版本轻松实现设备连接、消息收发、远程调用等功能。

## 主要功能

- **MQTT 基础连接**：支持设备安全可靠地连接到 IoT 平台
- **消息发布与订阅**：实现设备与云端的双向通信
- **远程过程调用(RRPC)**：支持云端对设备的实时指令调用
- **设备动态注册**：支持一型一密和免白名单动态注册模式

## 快速开始

### 1. 修改设备信息

在使用 SDK 前，需要将示例代码中的设备信息修改为您自己的设备信息：

```c
/* 替换为自己设备的三元组 */
const char *product_key       = "你的产品Key";
const char *device_name       = "你的设备名称";
const char *device_secret     = "你的设备密钥";

/* 替换为新 IoT 平台的接入点 */
const char *mqtt_host = "平台域名或IP";
const uint16_t port = 8883;  // 新平台端口
```

### 2. 证书配置

SDK 已配置使用自定义证书连接新的 IoT 平台：

```c
cred.x509_server_cert = new_custom_cert;  // 使用自定义证书
cred.x509_server_cert_len = strlen(new_custom_cert);
```

如需更新证书，可修改 `external/my_ca_cert.c` 文件中的证书内容。

## 示例说明

### MQTT 基础连接

`new_mqtt_demo.c` 演示了如何与平台建立 MQTT 连接，发布和订阅消息：

- 建立安全连接
- 自动心跳保活
- 消息发布
- 消息订阅与处理
- 自动断线重连

### 设备动态注册

`new_dynregmq_demo.c` 演示了设备动态注册流程：

- 支持白名单模式（返回 DeviceSecret）
- 支持免白名单模式（返回 MQTT 连接信息）
- 安全地获取设备凭证

### RRPC 远程过程调用

`new_rrpc_demo.c` 演示了如何处理云端下发的 RRPC 请求：

- 自动解析 RRPC 请求主题
- 提取请求 ID
- 构造并发送 RRPC 响应

### 物模型相关调用

`data_model_basic_demo.c` 演示了物模型相关功能：

```c
/* 上报单个属性*/
demo_send_property_post(dm_handle, "{\"temperature\": 25}");

/* 上报多个属性*/
demo_send_property_post(dm_handle, "{\"humidity\": 60, \"pm25\": 12}");
```

## 开发注意事项

1. **三元组安全**：设备三元组是设备身份凭证，请妥善保管
2. **连接参数**：根据您的网络环境调整连接参数和超时时间
3. **证书更新**：证书有效期到期前需及时更新
4. **资源释放**：不再使用 SDK 时，调用相应的资源释放函数

## 优势特点

- 基于成熟的阿里云 LinkSDK，具有稳定可靠的特性
- 通过简单修改即可对接新 IoT 平台，迁移成本低
- 支持多种设备认证方式，适应不同安全需求
- 完整实现 MQTT 协议功能，支持复杂业务场景
