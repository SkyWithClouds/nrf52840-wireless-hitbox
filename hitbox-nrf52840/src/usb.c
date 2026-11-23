/* SPDX-License-Identifier: MIT */
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/usb/usb_device.h>
#include <os_desc.h>
#include <usb_descriptor.h>

#include "hardware.h"
enum { EP_IN = 0x81, EP_OUT = 0x02, MPS = 32, MS_REQUEST = 0x20 };

struct xinput_descriptors {
	struct usb_if_descriptor iface;
	uint8_t capabilities[17];
	struct usb_ep_descriptor in;
	struct usb_ep_descriptor out;
} __packed;

USBD_CLASS_DESCR_DEFINE(primary, 0) struct xinput_descriptors descriptors = {
	.iface = {
		.bLength = sizeof(struct usb_if_descriptor),
		.bDescriptorType = USB_DESC_INTERFACE,
		.bNumEndpoints = 2,
		.bInterfaceClass = 0xff, .bInterfaceSubClass = 0x5d, .bInterfaceProtocol = 1,
	},
	.capabilities = {
		17, 0x21, 0x00, 0x01, 0x01, 0x25, EP_IN, 20,
		0, 0, 0, 0, 0x13, EP_OUT, 8, 0, 0
	},
	.in = {
		.bLength = sizeof(struct usb_ep_descriptor), .bDescriptorType = USB_DESC_ENDPOINT,
		.bEndpointAddress = EP_IN, .bmAttributes = USB_DC_EP_INTERRUPT,
		.wMaxPacketSize = sys_cpu_to_le16(MPS), .bInterval = 1,
	},
	.out = {
		.bLength = sizeof(struct usb_ep_descriptor), .bDescriptorType = USB_DESC_ENDPOINT,
		.bEndpointAddress = EP_OUT, .bmAttributes = USB_DC_EP_INTERRUPT,
		.wMaxPacketSize = sys_cpu_to_le16(MPS), .bInterval = 8,
	},
};

static uint8_t ms_string[] = {
	18, USB_DESC_STRING, 'M', 0, 'S', 0, 'F', 0, 'T', 0, '1', 0, '0', 0, '0', 0,
	MS_REQUEST, 0,
};
static uint8_t compatible_id[40] = {
	40, 0, 0, 0, 0, 1, 4, 0, 1, /* length, version 1.00, index 4, one function */
	[16] = 0, 1, 'X', 'U', 'S', 'B', '1', '0', /* interface 0, Reserved1, ID */
};
static struct usb_os_descriptor os = {
	.string = ms_string, .string_len = sizeof(ms_string), .vendor_code = MS_REQUEST,
	.compat_id = compatible_id, .compat_id_len = sizeof(compatible_id),
};

static atomic_t configured, suspended, epoch, game_busy, hid_busy, lamp;
static const uint8_t hid_report[] = {
    0x06,0x60,0xff, 0x09,0x01, 0xa1,0x01, /* vendor page FF60, application */
    0x15,0x00, 0x26,0xff,0x00, 0x75,0x08, 0x95,HID_BYTES,
    0x09,0x02, 0x81,0x02, 0x09,0x03, 0xb1,0x02, 0xc0
};
struct hid_descriptors {
    struct usb_if_descriptor iface;
    uint8_t hid[9];
    struct usb_ep_descriptor in;
} __packed;
USBD_CLASS_DESCR_DEFINE(primary, 1) struct hid_descriptors web = {
    .iface={.bLength=9,.bDescriptorType=USB_DESC_INTERFACE,.bNumEndpoints=1,.bInterfaceClass=3},
    .hid={9,0x21,0x11,0x01,0,1,0x22,sizeof(hid_report),0},
    .in={.bLength=7,.bDescriptorType=USB_DESC_ENDPOINT,.bEndpointAddress=0x83,
         .bmAttributes=USB_DC_EP_INTERRUPT,.wMaxPacketSize=sys_cpu_to_le16(HID_BYTES),.bInterval=8}
};
static struct k_spinlock lock;
static uint8_t last_feature[HID_BYTES], control_copy[HID_BYTES], idle_rate;
struct queued_command { atomic_val_t epoch; uint8_t data[HID_BYTES]; };
K_MSGQ_DEFINE(commands,sizeof(struct queued_command),4,4);

static int hid_descriptor(struct usb_setup_packet *s,int32_t *len,uint8_t **data)
{
    if (s->bmRequestType!=0x81 || s->bRequest!=6 || s->wIndex!=1) return -EINVAL;
    unsigned size;
    if (s->wValue==0x2200) { *data=(uint8_t *)hid_report; size=sizeof(hid_report); }
    else if (s->wValue==0x2100) { *data=web.hid; size=sizeof(web.hid); }
    else return -EINVAL;
    *len=MIN(s->wLength,size);
    return 0;
}
static int hid_control(struct usb_setup_packet *s,int32_t *len,uint8_t **data)
{
    if (s->wIndex!=1) return -ENOTSUP;
    if (s->bmRequestType==0xa1 && s->bRequest==1 && s->wValue==0x0300) {
        k_spinlock_key_t key=k_spin_lock(&lock);
        memcpy(control_copy,last_feature,HID_BYTES);
        k_spin_unlock(&lock,key);
        *data=control_copy; *len=MIN(s->wLength,HID_BYTES);
        return 0;
    }
    if (s->bmRequestType==0x21 && s->bRequest==9 && s->wValue==0x0300 &&
        s->wLength==HID_BYTES && *len==HID_BYTES) {
        struct queued_command cmd={.epoch=atomic_get(&epoch)};
        memcpy(cmd.data,*data,HID_BYTES);
        return k_msgq_put(&commands,&cmd,K_NO_WAIT);
    }
    if (s->bmRequestType==0x21 && s->bRequest==10) { idle_rate=s->wValue>>8; return 0; }
    if (s->bmRequestType==0xa1 && s->bRequest==2) { *len=1; *data=&idle_rate; return 0; }
    return -ENOTSUP;
}
static void endpoint_in(uint8_t ep, enum usb_dc_ep_cb_status_code status)
{
    if (status==USB_DC_EP_DATA_IN) atomic_clear(ep==EP_IN ? &game_busy : &hid_busy);
}
static void endpoint_out(uint8_t ep, enum usb_dc_ep_cb_status_code status)
{
    uint8_t packet[MPS];
    uint32_t n;
    if (status==USB_DC_EP_DATA_OUT && !usb_read(ep,packet,sizeof(packet),&n) &&
        n>=3 && packet[0]==1 && packet[1]==3) atomic_set(&lamp,0x100|packet[2]);
}
static void usb_status(enum usb_dc_status_code status,const uint8_t *param)
{
    switch (status) {
    case USB_DC_CONNECTED: wake_input(); break;
    case USB_DC_DISCONNECTED:
    case USB_DC_RESET:
    case USB_DC_CONFIGURED:
        atomic_set(&configured,status==USB_DC_CONFIGURED && param && *param);
        atomic_clear(&suspended); atomic_clear(&game_busy); atomic_clear(&hid_busy);
        atomic_clear(&lamp); atomic_inc(&epoch); wake_input(); break;
    case USB_DC_SUSPEND: atomic_set(&suspended,1); break;
    case USB_DC_RESUME: atomic_clear(&suspended); break;
    default: break;
    }
}
static struct usb_ep_cfg_data game_ep[]={{.ep_cb=endpoint_in,.ep_addr=EP_IN},
                                          {.ep_cb=endpoint_out,.ep_addr=EP_OUT}};
static struct usb_ep_cfg_data hid_ep[]={{.ep_cb=endpoint_in,.ep_addr=0x83}};
static void number_interface(struct usb_desc_header *head,uint8_t n)
{
    ((struct usb_if_descriptor *)head)->bInterfaceNumber=n;
}
USBD_DEFINE_CFG_DATA(xinput) = {.interface_descriptor=&descriptors.iface,.interface_config=number_interface,
    .num_endpoints=ARRAY_SIZE(game_ep),.endpoint=game_ep};
USBD_DEFINE_CFG_DATA(webhid) = {.interface_descriptor=&web.iface,.interface_config=number_interface,
    .interface={.custom_handler=hid_descriptor,.class_handler=hid_control},
    .num_endpoints=ARRAY_SIZE(hid_ep),.endpoint=hid_ep};
int wired_open(void)
{
    if (!usb_get_device_descriptor() || descriptors.iface.bInterfaceNumber!=0 ||
        web.iface.bInterfaceNumber!=1) return -EINVAL;
    /* The legacy allocator renumbers endpoints. Keep XUSB's embedded capability
       record, endpoint descriptors and Zephyr callback table consistent. */
    descriptors.in.bEndpointAddress=game_ep[0].ep_addr=EP_IN;
    descriptors.out.bEndpointAddress=game_ep[1].ep_addr=EP_OUT;
    web.in.bEndpointAddress=hid_ep[0].ep_addr=0x83;
    usb_register_os_desc(&os);
    return usb_enable(usb_status);
}
static void report_game(uint8_t *buf,const struct gp_input *s)
{
    memset(buf,0,20); buf[1]=20;
    sys_put_le16(s->buttons,buf+2); buf[4]=s->lt; buf[5]=s->rt;
    sys_put_le16(s->lx,buf+6); sys_put_le16(s->ly,buf+8);
    sys_put_le16(s->rx,buf+10); sys_put_le16(s->ry,buf+12);
}
static bool submit(uint8_t ep, atomic_t *busy,uint8_t *buf,size_t size)
{
    uint32_t n;
    if (!atomic_cas(busy,0,1)) return false;
    /* Nordic USBD retains the caller's EasyDMA pointer until the IN callback. */
    static uint8_t game_dma[20] __aligned(4), live_dma[HID_BYTES] __aligned(4);
    uint8_t *dma = ep==EP_IN ? game_dma : live_dma;
    memcpy(dma,buf,size);
    if (usb_write(ep,dma,size,&n) || n!=size) { atomic_clear(busy); return false; }
    return true;
}
bool wired_tick(const struct gp_input *pad,const struct engine *e,struct configuration *cfg,
                const struct battery *bat,uint32_t ms)
{
    static atomic_val_t seen=-1;
    static struct editor edit;
    static uint8_t fifo[GP_QUEUE_SIZE][20], previous[20], count, head;
    static bool have_previous;
    static uint32_t last_live;
    atomic_val_t current=atomic_get(&epoch);
    if (seen!=current) {
        edit.editing=false; seen=current; count=head=0; have_previous=false;
        k_spinlock_key_t key=k_spin_lock(&lock);
        memset(last_feature,0,HID_BYTES);
        k_spin_unlock(&lock,key);
    }
    if (!atomic_get(&configured) || atomic_get(&suspended)) {
        count=0; have_previous=false; return false;
    }
    struct queued_command cmd;
    bool changed=false;
    if (!k_msgq_get(&commands,&cmd,K_NO_WAIT) && cmd.epoch==seen) {
        uint8_t reply[HID_BYTES];
        config_command(&edit,cfg,cmd.data,reply,store_save);
        changed = cmd.data[3]==CMD_COMMIT && reply[6]==0;
        k_spinlock_key_t key=k_spin_lock(&lock);
        memcpy(last_feature,reply,HID_BYTES);
        k_spin_unlock(&lock,key);
    }
    uint8_t game[20];
    report_game(game,pad);
    if (!have_previous || memcmp(previous,game,20)) {
        if (count==GP_QUEUE_SIZE) count=0; /* stalled host: retain newest state */
        memcpy(fifo[(head+count)%GP_QUEUE_SIZE],game,20); count++;
        memcpy(previous,game,20); have_previous=true;
    }
    if (count && submit(EP_IN,&game_busy,fifo[head],20)) { head=(head+1)%GP_QUEUE_SIZE; count--; }
    if (ms-last_live>=16 && !atomic_get(&hid_busy)) {
        uint8_t live[HID_BYTES]={'H','L',1,cfg->active};
        sys_put_le32(e->raw,live+4); sys_put_le32(e->stable,live+8);
        memcpy(live+12,game,20);
        sys_put_le16(bat->mv,live+32); live[34]=bat->source;
        sys_put_le16(bat->pin_mv,live+35); sys_put_le16(atomic_get(&lamp),live+37);
        if (submit(0x83,&hid_busy,live,sizeof(live))) last_live=ms;
    }
    return changed;
}
