/*
 * 这个例程演示了如何主动发送消息获取OTA升级信息
 * 它演示了用SDK配置MQTT参数并建立连接, 并主动发送消息查询OTA升级信息的过程
 *
 * 需要用户关注或修改的部分, 已用 `TODO` 在注释中标明
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include "aiot_state_api.h"
#include "aiot_sysdep_api.h"
#include "aiot_mqtt_api.h"
#include "aiot_ota_api.h"

/* TODO: 替换为自己设备的三元组 */
const char *product_key       = "EkkpqOZe";
const char *device_name       = "ctest001";
const char *device_secret     = "MwJqVSXG38***";

const char  *mqtt_host = "60.173.17.244";
const uint16_t port = 8883;

/* 位于external/ali_ca_cert.c中的服务器证书 */
extern const char *ali_ca_cert;

/* 位于external/my_ca_cert.c中的自定义证书 */
extern const char *new_custom_cert;

void *g_ota_handle = NULL;
void *g_dl_handle = NULL;
uint32_t g_firmware_size = 0;

/* 位于portfiles/aiot_port文件夹下的系统适配函数集合 */
extern aiot_sysdep_portfile_t g_aiot_sysdep_portfile;

/* 日志回调函数, SDK的日志会从这里输出 */
int32_t demo_state_logcb(int32_t code, char *message)
{
    /* 下载固件的时候会有大量的HTTP收包日志, 通过code筛选出来关闭 */
    if (STATE_HTTP_LOG_RECV_CONTENT != code) {
        printf("%s", message);
    }
    return 0;
}

/* MQTT事件回调函数, 当网络连接/重连/断开时被触发, 事件定义见core/aiot_mqtt_api.h */
void demo_mqtt_event_handler(void *handle, const aiot_mqtt_event_t *const event, void *userdata)
{
    switch (event->type) {
    case AIOT_MQTTEVT_CONNECT: {
        printf("AIOT_MQTTEVT_CONNECT\r\n");
    }
    break;

    case AIOT_MQTTEVT_RECONNECT: {
        printf("AIOT_MQTTEVT_RECONNECT\r\n");
    }
    break;

    case AIOT_MQTTEVT_DISCONNECT: {
        char *cause = (event->data.disconnect == AIOT_MQTTDISCONNEVT_NETWORK_DISCONNECT) ? ("network disconnect") :
                      ("heartbeat disconnect");
        printf("AIOT_MQTTEVT_DISCONNECT: %s\r\n", cause);
    }
    break;
    default: {

    }
    }
}

/* 下载收包回调, 用户调用 aiot_download_recv() 后, SDK收到数据会进入这个函数, 把下载到的数据交给用户 */
void user_download_recv_handler(void *handle, const aiot_download_recv_t *packet, void *userdata)
{
    int32_t percent = 0;
    int32_t last_percent = 0;
    uint32_t data_buffer_len = 0;

    if (!packet || AIOT_DLRECV_HTTPBODY != packet->type) {
        return;
    }
    percent = packet->data.percent;

    if (percent < 0) {
        printf("exception happend, percent is %d\r\n", percent);
        if (userdata) {
            free(userdata);
        }
        return;
    }

    if (userdata) {
        last_percent = *((uint32_t *)(userdata));
    }
    data_buffer_len = packet->data.len;

    if (percent == 100) {
        /* TODO: 这里可以执行固件校验、保存新版本号等操作 */
        printf("Download completed\r\n");
    }

    if (percent - last_percent >= 5 || percent == 100) {
        printf("download %03d%% done, +%d bytes\r\n", percent, data_buffer_len);
        aiot_download_report_progress(handle, percent);

        if (userdata) {
            *((uint32_t *)(userdata)) = percent;
        }
        if (percent == 100 && userdata) {
            free(userdata);
        }
    }
}

/* 用户通过 aiot_ota_setopt() 注册的OTA消息处理回调 */
void user_ota_recv_handler(void *ota_handle, aiot_ota_recv_t *ota_msg, void *userdata)
{
    switch (ota_msg->type) {
    case AIOT_OTARECV_FOTA: {
        uint16_t port = 443;
        uint32_t max_buffer_len = 2048;
        aiot_sysdep_network_cred_t cred;
        void *dl_handle = NULL;
        void *last_percent = NULL;

        if (NULL == ota_msg->task_desc || ota_msg->task_desc->protocol_type != AIOT_OTA_PROTOCOL_HTTPS) {
            printf("No OTA task available or protocol not supported\r\n");
            break;
        }

        printf("OTA task received:\r\n");
        printf("    Version: %s\r\n", ota_msg->task_desc->version);
        printf("    Size: %u Bytes\r\n", ota_msg->task_desc->size_total);

        dl_handle = aiot_download_init();
        if (NULL == dl_handle) {
            printf("Failed to initialize download handle\r\n");
            break;
        }

        last_percent = malloc(sizeof(uint32_t));
        if (NULL == last_percent) {
            aiot_download_deinit(&dl_handle);
            break;
        }
        memset(last_percent, 0, sizeof(uint32_t));

        g_firmware_size = ota_msg->task_desc->size_total;

        memset(&cred, 0, sizeof(aiot_sysdep_network_cred_t));
        cred.option = AIOT_SYSDEP_NETWORK_CRED_SVRCERT_CA;
        cred.max_tls_fragment = 16384;
        cred.x509_server_cert = ali_ca_cert;
        cred.x509_server_cert_len = strlen(ali_ca_cert);

        if ((STATE_SUCCESS != aiot_download_setopt(dl_handle, AIOT_DLOPT_NETWORK_CRED, (void *)(&cred)))
                || (STATE_SUCCESS != aiot_download_setopt(dl_handle, AIOT_DLOPT_NETWORK_PORT, (void *)(&port)))
                || (STATE_SUCCESS != aiot_download_setopt(dl_handle, AIOT_DLOPT_TASK_DESC, (void *)(ota_msg->task_desc)))
                || (STATE_SUCCESS != aiot_download_setopt(dl_handle, AIOT_DLOPT_RECV_HANDLER, (void *)(user_download_recv_handler)))
                || (STATE_SUCCESS != aiot_download_setopt(dl_handle, AIOT_DLOPT_BODY_BUFFER_MAX_LEN, (void *)(&max_buffer_len)))
                || (STATE_SUCCESS != aiot_download_setopt(dl_handle, AIOT_DLOPT_USERDATA, (void *)last_percent))
                || (STATE_SUCCESS != aiot_download_send_request(dl_handle))) {
            aiot_download_deinit(&dl_handle);
            free(last_percent);
            printf("Failed to setup download parameters\r\n");
            break;
        }
        g_dl_handle = dl_handle;
        break;
    }
    default:
        break;
    }
}


int main(int argc, char *argv[])
{
    int32_t res = STATE_SUCCESS;
    void *mqtt_handle = NULL;
    aiot_sysdep_network_cred_t cred;
    char *cur_version = "1.0.0";  /* TODO: 替换为实际的当前版本号 */
    void *ota_handle = NULL;

    /* 配置SDK的底层依赖 */
    aiot_sysdep_set_portfile(&g_aiot_sysdep_portfile);
    /* 配置SDK的日志输出 */
    aiot_state_set_logcb(demo_state_logcb);

    /* 创建SDK的安全凭据 */
    memset(&cred, 0, sizeof(aiot_sysdep_network_cred_t));
    cred.option = AIOT_SYSDEP_NETWORK_CRED_SVRCERT_CA;
    cred.max_tls_fragment = 16384;
    cred.sni_enabled = 1;
    cred.x509_server_cert = new_custom_cert;            /* 使用自定义证书替代原来的证书 */
    cred.x509_server_cert_len = strlen(new_custom_cert); /* 自定义证书的长度 */

    /* 创建MQTT客户端 */
    mqtt_handle = aiot_mqtt_init();
    if (mqtt_handle == NULL) {
        printf("Failed to initialize MQTT handle\r\n");
        return -1;
    }

    /* 配置MQTT参数 */
    aiot_mqtt_setopt(mqtt_handle, AIOT_MQTTOPT_HOST, (void *)mqtt_host);
    aiot_mqtt_setopt(mqtt_handle, AIOT_MQTTOPT_PORT, (void *)&port);
    aiot_mqtt_setopt(mqtt_handle, AIOT_MQTTOPT_PRODUCT_KEY, (void *)product_key);
    aiot_mqtt_setopt(mqtt_handle, AIOT_MQTTOPT_DEVICE_NAME, (void *)device_name);
    aiot_mqtt_setopt(mqtt_handle, AIOT_MQTTOPT_DEVICE_SECRET, (void *)device_secret);
    aiot_mqtt_setopt(mqtt_handle, AIOT_MQTTOPT_NETWORK_CRED, (void *)&cred);
    aiot_mqtt_setopt(mqtt_handle, AIOT_MQTTOPT_EVENT_HANDLER, (void *)demo_mqtt_event_handler);

    /* 创建OTA会话实例 */
    ota_handle = aiot_ota_init();
    if (NULL == ota_handle) {
        printf("Failed to initialize OTA handle\r\n");
        goto exit;
    }

    /* 配置OTA会话参数 */
    aiot_ota_setopt(ota_handle, AIOT_OTAOPT_MQTT_HANDLE, mqtt_handle);
    aiot_ota_setopt(ota_handle, AIOT_OTAOPT_RECV_HANDLER, user_ota_recv_handler);
    g_ota_handle = ota_handle;

    /* 建立MQTT连接 */
    res = aiot_mqtt_connect(mqtt_handle);
    if (res < STATE_SUCCESS) {
        printf("Failed to connect MQTT: -0x%04X\r\n", -res);
        goto exit;
    }

    /* 上报当前版本号 */
    res = aiot_ota_report_version(ota_handle, cur_version);
    if (res < STATE_SUCCESS) {
        printf("Failed to report version: -0x%04X\r\n", -res);
    }

    // TODO: 替换为实际的模块名
    aiot_ota_setopt(ota_handle, AIOT_OTAOPT_MODULE, (void *)"模块名");

    /* 发送OTA查询请求 */
    res = aiot_ota_query_firmware(ota_handle);
    if (res < STATE_SUCCESS) {
        printf("Failed to send OTA query: -0x%04X\r\n", -res);
    } else {
        printf("OTA query sent successfully\r\n");
    }

    /* 持续处理收到的消息 */
    while (1) {
        aiot_mqtt_process(mqtt_handle);
        res = aiot_mqtt_recv(mqtt_handle);

        if (res == STATE_SYS_DEPEND_NWK_CLOSED) {
            sleep(1);
            continue;
        }

        if (NULL != g_dl_handle) {
            res = aiot_download_recv(g_dl_handle);
            
            if (res == STATE_DOWNLOAD_FINISHED) {
                printf("Download completed successfully\r\n");
                aiot_download_deinit(&g_dl_handle);
                continue;
            }

            if (res < STATE_SUCCESS) {
                printf("Download failed with error: -0x%04X\r\n", -res);
                aiot_download_deinit(&g_dl_handle);
                continue;
            }
        }
    }

exit:
    /* 销毁资源 */
    if (mqtt_handle != NULL) {
        aiot_mqtt_disconnect(mqtt_handle);
        aiot_mqtt_deinit(&mqtt_handle);
    }
    if (ota_handle != NULL) {
        aiot_ota_deinit(&ota_handle);
    }

    return 0;
}
