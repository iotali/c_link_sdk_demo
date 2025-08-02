/*
 * 这个例程适用于`Linux`这类支持pthread的POSIX设备, 它演示了用SDK配置MQTT参数并建立连接, 之后创建2个线程
 * 使用1883端口，不需要TLS证书的TCP连接
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
#include <stdlib.h>

#include "aiot_state_api.h"
#include "aiot_sysdep_api.h"
#include "aiot_mqtt_api.h"

/* 配置参数（将从配置文件读取） */
char product_key[64] = "QLTMkOfW";        // 默认值
char device_name[64] = "THYYENG5wd";      // 默认值  
char device_secret[128] = "1kaFD2aghSTw4aKV"; // 默认值
char mqtt_host[256] = "121.40.253.224";   // 默认值
uint16_t port = 1883;                     // 默认值
char security_mode[8] = "3";              // 默认值
uint16_t keepalive_sec = 60;              // 默认值

/* 位于portfiles/aiot_port文件夹下的系统适配函数集合 */
extern aiot_sysdep_portfile_t g_aiot_sysdep_portfile;

static pthread_t g_mqtt_process_thread;
static pthread_t g_mqtt_recv_thread;
/* static pthread_t g_mqtt_send_thread; */ // 已停用发送线程
static uint8_t g_mqtt_process_thread_running = 0;
static uint8_t g_mqtt_recv_thread_running = 0;
/* static uint8_t g_mqtt_send_thread_running = 0; */ // 已停用发送线程

/* TODO: 如果要关闭日志, 就把这个函数实现为空, 如果要减少日志, 可根据code选择不打印
 *
 * 例如: [1577589489.033][LK-0317] mqtt_basic_demo&gb80sFmX7yX
 *
 * 上面这条日志的code就是0317(十六进制), code值的定义见core/aiot_state_api.h
 *
 */

/* 配置文件读取函数 */
int load_config(const char *config_file)
{
    FILE *file = fopen(config_file, "r");
    if (file == NULL) {
        printf("无法打开配置文件 %s，使用默认配置\n", config_file);
        return -1;
    }

    char line[512];
    while (fgets(line, sizeof(line), file)) {
        // 跳过注释和空行
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') {
            continue;
        }

        // 移除行尾的换行符
        line[strcspn(line, "\r\n")] = 0;

        // 查找等号分隔符
        char *delimiter = strchr(line, '=');
        if (delimiter == NULL) {
            continue;
        }

        // 分离键和值
        *delimiter = '\0';
        char *key = line;
        char *value = delimiter + 1;

        // 匹配配置项
        if (strcmp(key, "mqtt_host") == 0) {
            strncpy(mqtt_host, value, sizeof(mqtt_host) - 1);
            mqtt_host[sizeof(mqtt_host) - 1] = '\0';
        } else if (strcmp(key, "mqtt_port") == 0) {
            port = (uint16_t)atoi(value);
        } else if (strcmp(key, "product_key") == 0) {
            strncpy(product_key, value, sizeof(product_key) - 1);
            product_key[sizeof(product_key) - 1] = '\0';
        } else if (strcmp(key, "device_name") == 0) {
            strncpy(device_name, value, sizeof(device_name) - 1);
            device_name[sizeof(device_name) - 1] = '\0';
        } else if (strcmp(key, "device_secret") == 0) {
            strncpy(device_secret, value, sizeof(device_secret) - 1);
            device_secret[sizeof(device_secret) - 1] = '\0';
        } else if (strcmp(key, "security_mode") == 0) {
            strncpy(security_mode, value, sizeof(security_mode) - 1);
            security_mode[sizeof(security_mode) - 1] = '\0';
        } else if (strcmp(key, "keepalive_sec") == 0) {
            keepalive_sec = (uint16_t)atoi(value);
            // 确保keepalive在有效范围内
            if (keepalive_sec < 30) keepalive_sec = 30;
            if (keepalive_sec > 1200) keepalive_sec = 1200;
        }
    }

    fclose(file);
    printf("配置文件 %s 加载成功\n", config_file);
    return 0;
}

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
            printf("AIOT_MQTTEVT_CONNECT - MQTT连接成功!\n");
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
            /* TODO: 处理服务器下发的业务报文 */
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

/* 执行定时发送消息的线程 - 已停用 */
/*
void *demo_mqtt_send_thread(void *args)
{
    int32_t res = STATE_SUCCESS;
    char topic[128] = {0};
    const char *message = "{CN,OwZbOxPZxS,866940076295997,TP975H3_S20_WS_V250506_1,31,89861124201047889211,9531,112039684,ctnet.MNC011.MCC460.GPRS,8a7f}";
    
    // 构建发送的主题
    snprintf(topic, sizeof(topic), "/%s/%s/user/update", product_key, device_name);
    
    while (g_mqtt_send_thread_running) {
        res = aiot_mqtt_pub(args, topic, (uint8_t *)message, strlen(message), 0);
        if (res < 0) {
            printf("aiot_mqtt_pub failed, res: -0x%04X\n", -res);
        } else {
            printf("定时消息已发送到 %s (TCP连接，无TLS)\n", topic);
        }
        
        // 休眠5秒
        sleep(5);
    }
    return NULL;
}
*/

int main(int argc, char *argv[])
{
    int32_t     res = STATE_SUCCESS;
    void       *mqtt_handle = NULL;
    aiot_sysdep_network_cred_t cred; /* 安全凭据结构体，设置为不使用TLS */

    /* 加载配置文件 */
    const char *config_file = (argc > 1) ? argv[1] : "config.txt";
    load_config(config_file);
    
    printf("当前配置:\n");
    printf("  MQTT服务器: %s:%d\n", mqtt_host, port);
    printf("  Product Key: %s\n", product_key);
    printf("  Device Name: %s\n", device_name);
    printf("  Security Mode: %s\n", security_mode);
    printf("  Keepalive: %d 秒\n", keepalive_sec);
    printf("===================\n");

    /* 配置SDK的底层依赖 */
    aiot_sysdep_set_portfile(&g_aiot_sysdep_portfile);
    /* 配置SDK的日志输出 */
    aiot_state_set_logcb(demo_state_logcb);

    /* 创建1个MQTT客户端实例并内部初始化默认参数 */
    mqtt_handle = aiot_mqtt_init();
    if (mqtt_handle == NULL) {
        printf("aiot_mqtt_init failed\n");
        return -1;
    }

    /* 创建SDK的安全凭据，完全按照原始代码的TCP连接方式 */
    memset(&cred, 0, sizeof(aiot_sysdep_network_cred_t));
    cred.option = AIOT_SYSDEP_NETWORK_CRED_NONE;

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
    /* 配置安全模式 (从配置文件读取) */
    aiot_mqtt_setopt(mqtt_handle, AIOT_MQTTOPT_SECURITY_MODE, (void *)security_mode);
    /* 配置MQTT心跳间隔 (从配置文件读取) */
    aiot_mqtt_setopt(mqtt_handle, AIOT_MQTTOPT_KEEPALIVE_SEC, (void *)&keepalive_sec);
    /* 配置网络连接的安全凭据，设置为不使用TLS */
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

    printf("MQTT连接成功! 使用TCP连接，端口1883，无TLS加密\n");

    /* MQTT 订阅topic功能示例, 请根据自己的业务需求进行使用 */
    /* {
        char *sub_topic = "/sys/${YourProductKey}/${YourDeviceName}/thing/event/+/post_reply";

        res = aiot_mqtt_sub(mqtt_handle, sub_topic, NULL, 1, NULL);
        if (res < 0) {
            printf("aiot_mqtt_sub failed, res: -0x%04X\n", -res);
            return -1;
        }
    } */

    /* MQTT 发布消息功能示例, 请根据自己的业务需求进行使用 */
    /* {
        char *pub_topic = "/sys/${YourProductKey}/${YourDeviceName}/thing/event/property/post";
        char *pub_payload = "{\"id\":\"1\",\"version\":\"1.0\",\"params\":{\"LightSwitch\":0}}";

        res = aiot_mqtt_pub(mqtt_handle, pub_topic, (uint8_t *)pub_payload, (uint32_t)strlen(pub_payload), 0);
        if (res < 0) {
            printf("aiot_mqtt_sub failed, res: -0x%04X\n", -res);
            return -1;
        }
    } */

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

    /* 注释掉定时发送消息线程，只保留心跳功能 */
    /*
    g_mqtt_send_thread_running = 1; // 已停用的变量
    res = pthread_create(&g_mqtt_send_thread, NULL, demo_mqtt_send_thread, mqtt_handle); // 已停用的线程
    if (res < 0) {
        printf("pthread_create demo_mqtt_send_thread failed: %d\n", res);
        return -1;
    }
    */

    /* 主循环进入休眠 */
    while (1) {
        sleep(1);
    }

    /* 断开MQTT连接, 一般不会运行到这里 */
    g_mqtt_process_thread_running = 0;
    g_mqtt_recv_thread_running = 0;
    /* g_mqtt_send_thread_running = 0; */ // 已经停止创建此线程
    sleep(1);
    pthread_join(g_mqtt_process_thread, NULL);
    pthread_join(g_mqtt_recv_thread, NULL);
    /* pthread_join(g_mqtt_send_thread, NULL); */ // 已经停止创建此线程

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