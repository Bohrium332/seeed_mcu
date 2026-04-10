/*
 * ESP32-S3 USB Host 设备检测程序
 * 
 * 硬件配置：
 * - USB0 : 下载/调试口 
 * - USB1 : Host口 (需配置)
 * - GPIO42: USB Host使能引脚（需拉高）
 * 
 * Arduino IDE 设置：
 * - Board: ESP32S3 Dev Module
 * - USB Mode: USB-OTG (TinyUSB)
 * - USB CDC On Boot: Disabled (使用UART0调试)
 */

#include <Arduino.h>
#include "usb/usb_host.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

// USB Host使能引脚
#define USB_HOST_ENABLE_PIN 42

// 调试标签
static const char *TAG = "USB_HOST";

// 设备句柄
static usb_host_client_handle_t client_hdl = NULL;
static usb_device_handle_t dev_hdl = NULL;
static bool device_connected = false;
static uint8_t current_dev_addr = 0;

// 函数声明
void usb_host_task(void *arg);
void usb_client_task(void *arg);
void print_device_info(usb_device_handle_t dev_hdl);
void print_device_descriptor(const usb_device_desc_t *dev_desc);
void print_config_descriptor(const usb_config_desc_t *cfg_desc);
const char* get_class_string(uint8_t device_class);
const char* get_speed_string(usb_speed_t speed);

// USB Host 库事件回调
static void host_lib_callback(const usb_host_lib_info_t *event_msg, void *arg)
{
    // 处理库级别事件
}

// 客户端事件回调
static void client_event_callback(const usb_host_client_event_msg_t *event_msg, void *arg)
{
    switch (event_msg->event) {
        case USB_HOST_CLIENT_EVENT_NEW_DEV:
            Serial.printf("\n[事件] 新设备连接!  地址: %d\n", event_msg->new_dev.address);
            current_dev_addr = event_msg->new_dev.address;
            device_connected = true;
            break;
        case USB_HOST_CLIENT_EVENT_DEV_GONE:
            Serial. println("\n[事件] 设备已断开!");
            device_connected = false;
            if (dev_hdl != NULL) {
                usb_host_device_close(client_hdl, dev_hdl);
                dev_hdl = NULL;
            }
            current_dev_addr = 0;
            break;
        default:
            break;
    }
}

void setup() {
    // ========== 第一步：尽早拉高GPIO42 ==========
    // 使用寄存器直接操作，确保最快速度
    gpio_reset_pin((gpio_num_t)USB_HOST_ENABLE_PIN);
    gpio_set_direction((gpio_num_t)USB_HOST_ENABLE_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level((gpio_num_t)USB_HOST_ENABLE_PIN, 1);
    
    // ========== 初始化串口（使用UART0，非USB CDC）==========
    Serial.begin(115200);
    delay(1000);
    
    Serial.println();
    Serial.println("================================================");
    Serial.println("    ESP32-S3 USB Host 设备检测程序");
    Serial.println("================================================");
    Serial.printf("GPIO%d 已设置为高电平 (USB Host使能)\n", USB_HOST_ENABLE_PIN);
    Serial.println();
    
    // ========== 初始化 USB Host 库 ==========
    Serial.println("[初始化] 正在启动USB Host库...");
    
    const usb_host_config_t host_config = {
        . skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };
    
    esp_err_t err = usb_host_install(&host_config);
    if (err != ESP_OK) {
        Serial. printf("[错误] USB Host安装失败: %s\n", esp_err_to_name(err));
        while (1) { delay(1000); }
    }
    Serial.println("[成功] USB Host库已安装");
    
    // ========== 注册客户端 ==========
    Serial.println("[初始化] 正在注册USB客户端...");
    
    const usb_host_client_config_t client_config = {
        .is_synchronous = false,
        .max_num_event_msg = 5,
        .async = {
            .client_event_callback = client_event_callback,
            . callback_arg = NULL,
        },
    };
    
    err = usb_host_client_register(&client_config, &client_hdl);
    if (err != ESP_OK) {
        Serial. printf("[错误] 客户端注册失败: %s\n", esp_err_to_name(err));
        while (1) { delay(1000); }
    }
    Serial.println("[成功] USB客户端已注册");
    
    // ========== 创建USB处理任务 ==========
    xTaskCreatePinnedToCore(usb_host_task, "usb_host", 4096, NULL, 2, NULL, 0);
    
    Serial.println();
    Serial.println("================================================");
    Serial.println("        等待USB设备接入...");
    Serial.println("================================================");
    Serial.println();
}

// USB Host 库处理任务
void usb_host_task(void *arg)
{
    while (1) {
        // 处理USB Host库事件
        uint32_t event_flags;
        usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
        
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            // 没有客户端
        }
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_ALL_FREE) {
            // 所有设备已释放
        }
    }
}

void loop() {
    // 处理客户端事件
    usb_host_client_handle_events(client_hdl, pdMS_TO_TICKS(50));
    
    // 检测到新设备时打印信息
    static bool info_printed = false;
    
    if (device_connected && current_dev_addr != 0 && !info_printed) {
        delay(100); // 等待设备稳定
        
        // 打开设备
        esp_err_t err = usb_host_device_open(client_hdl, current_dev_addr, &dev_hdl);
        if (err == ESP_OK) {
            print_device_info(dev_hdl);
            info_printed = true;
        } else {
            Serial.printf("[错误] 无法打开设备: %s\n", esp_err_to_name(err));
        }
    }
    
    if (! device_connected) {
        info_printed = false;
    }
    
    delay(10);
}

// 打印设备完整信息
void print_device_info(usb_device_handle_t dev_hdl)
{
    Serial.println();
    Serial.println("╔══════════════════════════════════════════════╗");
    Serial.println("║           USB 设备信息                       ║");
    Serial.println("╚══════════════════════════════════════════════╝");
    
    // 获取设备信息
    usb_device_info_t dev_info;
    esp_err_t err = usb_host_device_info(dev_hdl, &dev_info);
    if (err == ESP_OK) {
        Serial.println("\n【基本信息】");
        Serial.printf("  设备地址: %d\n", dev_info.dev_addr);
        Serial.printf("  设备速度: %s\n", get_speed_string(dev_info.speed));
    }
    
    // 获取设备描述符
    const usb_device_desc_t *dev_desc;
    err = usb_host_get_device_descriptor(dev_hdl, &dev_desc);
    if (err == ESP_OK) {
        print_device_descriptor(dev_desc);
    }
    
    // 获取配置描述符
    const usb_config_desc_t *cfg_desc;
    err = usb_host_get_active_config_descriptor(dev_hdl, &cfg_desc);
    if (err == ESP_OK) {
        print_config_descriptor(cfg_desc);
    }
    
    // 获取字符串描述符
    Serial.println("\n【字符串描述符】");
    
    usb_device_info_t info;
    usb_host_device_info(dev_hdl, &info);
    
    // 制造商字符串
    if (info.str_desc_manufacturer != NULL) {
        Serial.print("  制造商: ");
        // UTF-16LE 转换为字符串打印
        const uint8_t *str = (const uint8_t *)info.str_desc_manufacturer;
        int len = str[0];
        for (int i = 2; i < len; i += 2) {
            if (str[i] < 128) {
                Serial.print((char)str[i]);
            }
        }
        Serial. println();
    } else {
        Serial.println("  制造商: (无)");
    }
    
    // 产品字符串
    if (info. str_desc_product != NULL) {
        Serial.print("  产品名:  ");
        const uint8_t *str = (const uint8_t *)info.str_desc_product;
        int len = str[0];
        for (int i = 2; i < len; i += 2) {
            if (str[i] < 128) {
                Serial.print((char)str[i]);
            }
        }
        Serial.println();
    } else {
        Serial.println("  产品名: (无)");
    }
    
    // 序列号字符串
    if (info.str_desc_serial_num != NULL) {
        Serial.print("  序列号:  ");
        const uint8_t *str = (const uint8_t *)info.str_desc_serial_num;
        int len = str[0];
        for (int i = 2; i < len; i += 2) {
            if (str[i] < 128) {
                Serial.print((char)str[i]);
            }
        }
        Serial.println();
    } else {
        Serial.println("  序列号:  (无)");
    }
    
    Serial.println();
    Serial.println("══════════════════════════════════════════════");
    Serial.println();
}

// 打印设备描述符
void print_device_descriptor(const usb_device_desc_t *dev_desc)
{
    Serial. println("\n【设备描述符】");
    Serial.printf("  USB 版本:      %d.%d%d\n", 
                  ((dev_desc->bcdUSB >> 8) & 0x0F),
                  ((dev_desc->bcdUSB >> 4) & 0x0F),
                  (dev_desc->bcdUSB & 0x0F));
    Serial.printf("  设备类:        0x%02X (%s)\n", 
                  dev_desc->bDeviceClass,
                  get_class_string(dev_desc->bDeviceClass));
    Serial.printf("  设备子类:     0x%02X\n", dev_desc->bDeviceSubClass);
    Serial.printf("  设备协议:     0x%02X\n", dev_desc->bDeviceProtocol);
    Serial.printf("  最大包大小:   %d 字节\n", dev_desc->bMaxPacketSize0);
    Serial.printf("  厂商ID (VID): 0x%04X\n", dev_desc->idVendor);
    Serial.printf("  产品ID (PID): 0x%04X\n", dev_desc->idProduct);
    Serial.printf("  设备版本:     %d.%d%d\n",
                  ((dev_desc->bcdDevice >> 8) & 0x0F),
                  ((dev_desc->bcdDevice >> 4) & 0x0F),
                  (dev_desc->bcdDevice & 0x0F));
    Serial.printf("  配置数量:     %d\n", dev_desc->bNumConfigurations);
}

// 打印配置描述符和接口/端点信息
void print_config_descriptor(const usb_config_desc_t *cfg_desc)
{
    Serial.println("\n【配置描述符】");
    Serial.printf("  配置值:     %d\n", cfg_desc->bConfigurationValue);
    Serial.printf("  接口数量:   %d\n", cfg_desc->bNumInterfaces);
    Serial.printf("  总长度:     %d 字节\n", cfg_desc->wTotalLength);
    Serial.printf("  供电属性:   0x%02X", cfg_desc->bmAttributes);
    if (cfg_desc->bmAttributes & 0x40) Serial.print(" [自供电]");
    if (cfg_desc->bmAttributes & 0x20) Serial.print(" [远程唤醒]");
    Serial.println();
    Serial.printf("  最大功耗:   %d mA\n", cfg_desc->bMaxPower * 2);
    
    // 解析接口和端点
    Serial.println("\n【接口和端点】");
    
    int offset = USB_CONFIG_DESC_SIZE;
    while (offset < cfg_desc->wTotalLength) {
        const uint8_t *p = (const uint8_t *)cfg_desc + offset;
        uint8_t bLength = p[0];
        uint8_t bDescriptorType = p[1];
        
        if (bLength == 0) break;
        
        // 接口描述符 (类型 = 4)
        if (bDescriptorType == USB_B_DESCRIPTOR_TYPE_INTERFACE) {
            const usb_intf_desc_t *intf = (const usb_intf_desc_t *)p;
            Serial.printf("\n  ┌─ 接口 #%d (备选设置:  %d)\n", 
                          intf->bInterfaceNumber,
                          intf->bAlternateSetting);
            Serial.printf("  │  端点数量: %d\n", intf->bNumEndpoints);
            Serial.printf("  │  接口类:    0x%02X (%s)\n", 
                          intf->bInterfaceClass,
                          get_class_string(intf->bInterfaceClass));
            Serial. printf("  │  子类:     0x%02X\n", intf->bInterfaceSubClass);
            Serial. printf("  │  协议:     0x%02X\n", intf->bInterfaceProtocol);
        }
        // 端点描述符 (类型 = 5)
        else if (bDescriptorType == USB_B_DESCRIPTOR_TYPE_ENDPOINT) {
            const usb_ep_desc_t *ep = (const usb_ep_desc_t *)p;
            
            uint8_t ep_num = ep->bEndpointAddress & 0x0F;
            const char *ep_dir = (ep->bEndpointAddress & 0x80) ? "IN " : "OUT";
            
            const char *ep_type;
            switch (ep->bmAttributes & 0x03) {
                case 0: ep_type = "控制"; break;
                case 1: ep_type = "等时"; break;
                case 2: ep_type = "批量"; break;
                case 3: ep_type = "中断"; break;
                default: ep_type = "未知"; break;
            }
            
            Serial. printf("  │  └─ 端点 %d %s: 类型=%s, 包大小=%d",
                          ep_num, ep_dir, ep_type, 
                          ep->wMaxPacketSize & 0x07FF);
            if ((ep->bmAttributes & 0x03) == 3) { // 中断端点
                Serial. printf(", 间隔=%dms", ep->bInterval);
            }
            Serial.println();
        }
        // HID描述符 (类型 = 33)
        else if (bDescriptorType == 0x21) {
            Serial.println("  │  [HID描述符]");
        }
        
        offset += bLength;
    }
}

// 获取速度字符串
const char* get_speed_string(usb_speed_t speed)
{
    switch (speed) {
        case USB_SPEED_LOW:  return "低速 (1.5 Mbps)";
        case USB_SPEED_FULL: return "全速 (12 Mbps)";
        default:  return "未知";
    }
}

// 获取设备类字符串
const char* get_class_string(uint8_t device_class)
{
    switch (device_class) {
        case 0x00: return "由接口定义";
        case 0x01: return "音频";
        case 0x02: return "CDC通信";
        case 0x03: return "HID人机接口";
        case 0x05: return "物理";
        case 0x06: return "图像";
        case 0x07: return "打印机";
        case 0x08: return "大容量存储";
        case 0x09: return "USB Hub";
        case 0x0A:  return "CDC数据";
        case 0x0B: return "智能卡";
        case 0x0D:  return "内容安全";
        case 0x0E: return "视频";
        case 0x0F: return "个人保健";
        case 0x10: return "音视频";
        case 0xDC: return "诊断";
        case 0xE0: return "无线控制器";
        case 0xEF: return "杂项";
        case 0xFE: return "特定应用";
        case 0xFF: return "厂商定义";
        default:    return "未知";
    }
}