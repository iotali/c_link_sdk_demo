/*
 * 这个例程适用于`Linux`这类支持pthread的POSIX设备, 它演示了用SDK配置MQTT参数并建立连接, 之后创建2个线程
 *
 * + 一个线程用于保活长连接
 * + 一个线程用于接收消息, 并在有消息到达时进入默认的数据回调, 在连接状态变化时进入事件回调
 *
 * 需要用户关注或修改的部分, 已经用 TODO 在注释中标明
 *
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "aiot_state_api.h"
#include "aiot_sysdep_api.h"
#include "aiot_mqtt_api.h"

/* TODO: 替换为自己设备的三元组 */
const char *product_key       = "QrjKUuXE";
const char *device_name       = "32test";
const char *device_secret     = "QWt56sOjrbuoCON3";

/*
    TODO: 替换为自己实例的接入点

    对于企业实例, 或者2021年07月30日之后（含当日）开通的物联网平台服务下公共实例
    mqtt_host的格式为"${YourInstanceId}.mqtt.iothub.aliyuncs.com"
    其中${YourInstanceId}: 请替换为您企业/公共实例的Id

    对于2021年07月30日之前（不含当日）开通的物联网平台服务下公共实例，请使用旧版接入点。
    详情请见: https://help.aliyun.com/document_detail/147356.html
*/
const char  *mqtt_host = "60.173.17.244";
/* 
    原端口：1883/443，对应的证书(GlobalSign R1),于2028年1月过期，届时可能会导致设备不能建连。
    (推荐)新端口：8883，将搭载新证书，由阿里云物联网平台自签证书，于2053年7月过期。
*/
const uint16_t port = 8883;

/* 位于portfiles/aiot_port文件夹下的系统适配函数集合 */
extern aiot_sysdep_portfile_t g_aiot_sysdep_portfile;

/* 位于external/ali_ca_cert.c中的服务器证书 */
extern const char *ali_ca_cert;
/* 位于external/my_ca_cert.c中的自定义证书 */
extern const char *new_custom_cert;
static pthread_t g_mqtt_process_thread;
static pthread_t g_mqtt_recv_thread;
static uint8_t g_mqtt_process_thread_running = 0;
static uint8_t g_mqtt_recv_thread_running = 0;

/* TODO: 如果要关闭日志, 就把这个函数实现为空, 如果要减少日志, 可根据code选择不打印
 *
 * 例如: [1577589489.033][LK-0317] {YourDeviceName}&{YourProductKey}
 *
 * 上面这条日志的code就是0317(十六进制), code值的定义见core/aiot_state_api.h
 *
 */

/* 日志回调函数, SDK的日志会从这里输出 */
int32_t demo_state_logcb(int32_t code, char *message)
{
    printf("%s", message);
    return 0;
}

/* MQTT事件回调函数, 当网络连接/重连/断开时被触发, 事件定义见core/aiot_mqtt_api.h */
void demo_mqtt_event_handler(void *handle, const aiot_mqtt_event_t *event, void *userdata)
{
    switch (event->type) {
        /* SDK因为用户调用了aiot_mqtt_connect()接口, 与mqtt服务器建立连接已成功 */
        case AIOT_MQTTEVT_CONNECT: {
            printf("AIOT_MQTTEVT_CONNECT\n");
            /* TODO: 处理SDK建连成功, 不可以在这里调用耗时较长的阻塞函数 */
        }
        break;

        /* SDK因为网络状况被动断连后, 自动发起重连已成功 */
        case AIOT_MQTTEVT_RECONNECT: {
            printf("AIOT_MQTTEVT_RECONNECT\n");
            /* TODO: 处理SDK重连成功, 不可以在这里调用耗时较长的阻塞函数 */
        }
        break;

        /* SDK因为网络的状况而被动断开了连接, network是底层读写失败, heartbeat是没有按预期得到服务端心跳应答 */
        case AIOT_MQTTEVT_DISCONNECT: {
            char *cause = (event->data.disconnect == AIOT_MQTTDISCONNEVT_NETWORK_DISCONNECT) ? ("network disconnect") :
                          ("heartbeat disconnect");
            printf("AIOT_MQTTEVT_DISCONNECT: %s\n", cause);
            /* TODO: 处理SDK被动断连, 不可以在这里调用耗时较长的阻塞函数 */
        }
        break;

        default: {

        }
    }
}

/* 处理RRPC请求的主题前缀，使用product_key和device_name动态构建 */
char RRPC_REQUEST_PREFIX[100];
int RRPC_PREFIX_LEN = 0; /* 将在运行时计算 */

/* MQTT默认消息处理回调, 当SDK从服务器收到MQTT消息时, 且无对应用户回调处理时被调用 */
void demo_mqtt_default_recv_handler(void *handle, const aiot_mqtt_recv_t *packet, void *userdata)
{
    switch (packet->type) {
        case AIOT_MQTTRECV_HEARTBEAT_RESPONSE: {
            printf("heartbeat response\n");
            /* TODO: 处理服务器对心跳的回应, 一般不处理 */
        }
        break;

        case AIOT_MQTTRECV_SUB_ACK: {
            printf("suback, res: -0x%04X, packet id: %d, max qos: %d\n",
                   -packet->data.sub_ack.res, packet->data.sub_ack.packet_id, packet->data.sub_ack.max_qos);
            /* TODO: 处理服务器对订阅请求的回应, 一般不处理 */
        }
        break;

        case AIOT_MQTTRECV_PUB: {
            printf("pub, qos: %d, topic: %.*s\n", packet->data.pub.qos, packet->data.pub.topic_len, packet->data.pub.topic);
            printf("pub, payload: %.*s\n", packet->data.pub.payload_len, packet->data.pub.payload);
            
            /* 处理RRPC请求，提取requestId并响应 */
            if (packet->data.pub.topic_len > 0) {
                /* 打印完整topic用于调试 */
                char full_topic[128] = {0};
                if (packet->data.pub.topic_len < sizeof(full_topic)) {
                    memcpy(full_topic, packet->data.pub.topic, packet->data.pub.topic_len);
                    printf("完整接收主题: %s\n", full_topic);
                }
                
                if (packet->data.pub.topic_len > RRPC_PREFIX_LEN && 
                    strncmp(packet->data.pub.topic, RRPC_REQUEST_PREFIX, RRPC_PREFIX_LEN) == 0) {
                    printf("检测到RRPC请求\n");
                    
                    /* 提取requestId - 确保不包含前导斜杠 */
                    char requestId[64] = {0};
                    size_t requestId_len = packet->data.pub.topic_len - RRPC_PREFIX_LEN;
                    
                    /* 检查是否有前导斜杠，如果有则跳过 */
                    const char *requestId_start = packet->data.pub.topic + RRPC_PREFIX_LEN;
                    if (*requestId_start == '/') {
                        requestId_start++;
                        requestId_len--;
                    }
                    
                    /* 复制正确的requestId */
                    if (requestId_len > 0) {
                        memcpy(requestId, requestId_start, requestId_len);
                        requestId[requestId_len] = '\0';
                    }
                    
                    printf("提取的requestId: %s\n", requestId);
                    
                    /* 构造响应topic - 使用三元组动态构建 */
                    char resp_topic[128] = {0};
                    snprintf(resp_topic, sizeof(resp_topic), "/sys/%s/%s/rrpc/response/%s", product_key, device_name, requestId);
                    
                    printf("构造的响应主题: %s\n", resp_topic);
                    
                    /* 准备响应消息 */
                    char *resp_payload = "{\"id\":\"1\",\"version\":\"1.0\",\"params\":{\"LightSwitch\":0}}";
                    
                    /* 发送响应 */
                    int32_t res = aiot_mqtt_pub(handle, resp_topic, (uint8_t *)resp_payload, 
                                              (uint32_t)strlen(resp_payload), 0);
                    if (res < 0) {
                        printf("RRPC响应失败, res: -0x%04X\n", -res);
                    } else {
                        printf("RRPC响应已发送到主题: %s\n", resp_topic);
                    }
                }
            }
        }
        break;

        case AIOT_MQTTRECV_PUB_ACK: {
            printf("puback, packet id: %d\n", packet->data.pub_ack.packet_id);
            /* TODO: 处理服务器对QoS1上报消息的回应, 一般不处理 */
        }
        break;

        default: {

        }
    }
}

/* 执行aiot_mqtt_process的线程, 包含心跳发送和QoS1消息重发 */
void *demo_mqtt_process_thread(void *args)
{
    int32_t res = STATE_SUCCESS;

    while (g_mqtt_process_thread_running) {
        res = aiot_mqtt_process(args);
        if (res == STATE_USER_INPUT_EXEC_DISABLED) {
            break;
        }
        sleep(1);
    }
    return NULL;
}

/* 执行aiot_mqtt_recv的线程, 包含网络自动重连和从服务器收取MQTT消息 */
void *demo_mqtt_recv_thread(void *args)
{
    int32_t res = STATE_SUCCESS;

    while (g_mqtt_recv_thread_running) {
        res = aiot_mqtt_recv(args);
        if (res < STATE_SUCCESS) {
            if (res == STATE_USER_INPUT_EXEC_DISABLED) {
                break;
            }
            sleep(1);
        }
    }
    return NULL;
}

int main(int argc, char *argv[])
{
    int32_t     res = STATE_SUCCESS;
    void       *mqtt_handle = NULL;
    aiot_sysdep_network_cred_t cred; /* 安全凭据结构体, 如果要用TLS, 这个结构体中配置CA证书等参数 */

    /* 初始化RRPC请求前缀 */
    snprintf(RRPC_REQUEST_PREFIX, sizeof(RRPC_REQUEST_PREFIX), "/sys/%s/%s/rrpc/request/", product_key, device_name);
    RRPC_PREFIX_LEN = strlen(RRPC_REQUEST_PREFIX);

    /* 配置SDK的底层依赖 */
    aiot_sysdep_set_portfile(&g_aiot_sysdep_portfile);
    /* 配置SDK的日志输出 */
    aiot_state_set_logcb(demo_state_logcb);

    /* 创建SDK的安全凭据, 用于建立TLS连接 */
    memset(&cred, 0, sizeof(aiot_sysdep_network_cred_t));
    cred.option = AIOT_SYSDEP_NETWORK_CRED_SVRCERT_CA;  /* 使用RSA证书校验MQTT服务端 */
    cred.max_tls_fragment = 16384; /* 最大的分片长度为16K, 其它可选值还有4K, 2K, 1K, 0.5K */
    cred.sni_enabled = 1;                               /* TLS建连时, 支持Server Name Indicator */
    cred.x509_server_cert = new_custom_cert;            /* 使用自定义证书替代原来的证书 */
    cred.x509_server_cert_len = strlen(new_custom_cert); /* 自定义证书的长度 */


    /* 创建1个MQTT客户端实例并内部初始化默认参数 */
    mqtt_handle = aiot_mqtt_init();
    if (mqtt_handle == NULL) {
        printf("aiot_mqtt_init failed\n");
        return -1;
    }

    /* TODO: 如果以下代码不被注释, 则例程会用TCP而不是TLS连接云平台 */
    /*
    {
        memset(&cred, 0, sizeof(aiot_sysdep_network_cred_t));
        cred.option = AIOT_SYSDEP_NETWORK_CRED_NONE;
    }
    */

    /* 配置MQTT服务器地址 */
    aiot_mqtt_setopt(mqtt_handle, AIOT_MQTTOPT_HOST, (void *)mqtt_host);
    /* 配置MQTT服务器端口 */
    aiot_mqtt_setopt(mqtt_handle, AIOT_MQTTOPT_PORT, (void *)&port);
    /* 配置设备productKey */
    aiot_mqtt_setopt(mqtt_handle, AIOT_MQTTOPT_PRODUCT_KEY, (void *)product_key);
    /* 配置设备deviceName */
    aiot_mqtt_setopt(mqtt_handle, AIOT_MQTTOPT_DEVICE_NAME, (void *)device_name);
    /* 配置设备deviceSecret */
    aiot_mqtt_setopt(mqtt_handle, AIOT_MQTTOPT_DEVICE_SECRET, (void *)device_secret);
    /* 配置网络连接的安全凭据, 上面已经创建好了 */
    aiot_mqtt_setopt(mqtt_handle, AIOT_MQTTOPT_NETWORK_CRED, (void *)&cred);
    /* 配置MQTT默认消息接收回调函数 */
    aiot_mqtt_setopt(mqtt_handle, AIOT_MQTTOPT_RECV_HANDLER, (void *)demo_mqtt_default_recv_handler);
    /* 配置MQTT事件回调函数 */
    aiot_mqtt_setopt(mqtt_handle, AIOT_MQTTOPT_EVENT_HANDLER, (void *)demo_mqtt_event_handler);

    /* 与服务器建立MQTT连接 */
    res = aiot_mqtt_connect(mqtt_handle);
    if (res < STATE_SUCCESS) {
        /* 尝试建立连接失败, 销毁MQTT实例, 回收资源 */
        aiot_mqtt_deinit(&mqtt_handle);
        printf("aiot_mqtt_connect failed: -0x%04X\n\r\n", -res);
        printf("please check variables like mqtt_host, produt_key, device_name, device_secret in demo\r\n");
        return -1;
    }

    /* MQTT 订阅topic功能示例, 请根据自己的业务需求进行使用 */
    {
        char sub_topic[128];
        snprintf(sub_topic, sizeof(sub_topic), "/sys/%s/%s/rrpc/request/+", product_key, device_name);
        printf("Subscribing to topic: %s\n", sub_topic);

        res = aiot_mqtt_sub(mqtt_handle, sub_topic, NULL, 1, NULL);
        if (res < 0) {
            printf("aiot_mqtt_sub failed, res: -0x%04X\n", -res);
            return -1;
        }
        printf("Subscription successful\n");
    }

    /* MQTT 发布消息功能示例, 请根据自己的业务需求进行使用 */
    {
        /* 注释掉这部分代码，因为我们在接收到RRPC请求后再发送响应 */
        /*
        char *pub_topic = "/sys/kdlxqvXX/testcv002/rrpc/response/";
        char *pub_payload = "{\"id\":\"1\",\"version\":\"1.0\",\"params\":{\"LightSwitch\":0}}";

        res = aiot_mqtt_pub(mqtt_handle, pub_topic, (uint8_t *)pub_payload, (uint32_t)strlen(pub_payload), 0);
        if (res < 0) {
            printf("aiot_mqtt_pub failed, res: -0x%04X\n", -res);
            return -1;
        }
        */
    }

    /* 创建一个单独的线程, 专用于执行aiot_mqtt_process, 它会自动发送心跳保活, 以及重发QoS1的未应答报文 */
    g_mqtt_process_thread_running = 1;
    res = pthread_create(&g_mqtt_process_thread, NULL, demo_mqtt_process_thread, mqtt_handle);
    if (res < 0) {
        printf("pthread_create demo_mqtt_process_thread failed: %d\n", res);
        return -1;
    }

    /* 创建一个单独的线程用于执行aiot_mqtt_recv, 它会循环收取服务器下发的MQTT消息, 并在断线时自动重连 */
    g_mqtt_recv_thread_running = 1;
    res = pthread_create(&g_mqtt_recv_thread, NULL, demo_mqtt_recv_thread, mqtt_handle);
    if (res < 0) {
        printf("pthread_create demo_mqtt_recv_thread failed: %d\n", res);
        return -1;
    }

    /* 主循环进入休眠 */
    while (1) {
        sleep(1);
    }

    /* 断开MQTT连接, 一般不会运行到这里 */
    g_mqtt_process_thread_running = 0;
    g_mqtt_recv_thread_running = 0;
    sleep(1);
    pthread_join(g_mqtt_process_thread, NULL);
    pthread_join(g_mqtt_recv_thread, NULL);

    res = aiot_mqtt_disconnect(mqtt_handle);
    if (res < STATE_SUCCESS) {
        aiot_mqtt_deinit(&mqtt_handle);
        printf("aiot_mqtt_disconnect failed: -0x%04X\n", -res);
        return -1;
    }

    /* 销毁MQTT实例, 一般不会运行到这里 */
    res = aiot_mqtt_deinit(&mqtt_handle);
    if (res < STATE_SUCCESS) {
        printf("aiot_mqtt_deinit failed: -0x%04X\n", -res);
        return -1;
    }

    return 0;
}

