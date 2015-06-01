#ifndef USB_H
#define USB_H

#include <stdint.h>

/* descriptor definitions from
 * USB 2.0 spec document
 */

typedef enum {
    usb_desc_device = 1,
    usb_desc_config = 2,
    usb_desc_string = 3,
    usb_desc_iface  = 4,
    usb_desc_EP     = 5,
    usb_desc_last_X
} usb_desc;

/* From table 9-8 (page 262) */
typedef struct
{
    uint8_t bLength, bDescType;
    uint16_t bcdUSB; /* eg. USB 1.1 0x0110 */
    uint8_t bDevClass, bDevSubClass, bDevProto;
    uint8_t bMaxPacketSize;
    uint16_t idVendor, idProd, bcdDevice;
    uint8_t iManu, iProd, iSerial;
    uint8_t bNumConfig;
} __attribute__((packed)) usb_std_device_desc;

/* From table 9-10 (page 265) */
typedef struct
{
    uint8_t bLength, bDescType;
    uint16_t bTotalLengh;
    uint8_t bNumIFaces, bConfValue, iConfig;
    uint8_t bmAttribs, bMaxPower;
} __attribute__((packed)) usb_std_config_desc;

/* From table 9-12 (page 268) */
typedef struct
{
    uint8_t bLength, bDescType;
    uint8_t bNumIFace, bAltSetting, bNumEP;
    uint8_t bIfaceClass, bIfaceSubClass, bIfaceProto;
    uint8_t iIface;
} __attribute__((packed)) usb_std_iface_desc;

/* From table 9-13 (page 269) */
typedef struct
{
    uint8_t bLength, bDescType;
    uint8_t bEPAddr;
    uint8_t bmAttribs;
    uint16_t bMaxPacketSize;
    uint8_t bInterval;
} __attribute__((packed)) usb_std_EP_desc;

typedef struct
{
    uint8_t bLength, bDescType;
    wchar_t bString[];
} __attribute__((packed)) usb_std_string_desc;

/* bmRequestType bitmap.  Table 9-2 page 248 */
#define ReqType_DirD2H   0b10000000
#define ReqType_TypeMask 0b01100000
#define ReqType_TypeStd  0
#define ReqType_TypeCls  0b00100000
#define ReqType_TypeVnd  0b01000000
#define ReqType_RecpMask 0b00011111
#define ReqType_RecpDev  0
#define ReqType_RecpIfc  1
#define ReqType_RecpEP   2
#define ReqType_RecpOtr  3

typedef enum {
    usb_req_get_status = 0,
    usb_req_clear_feature = 1,
    usb_req_set_feature = 3,
    usb_req_set_address = 5,
    usb_req_get_desc = 6,
    usb_req_set_desc = 7,
    usb_req_get_config = 8,
    usb_req_set_config = 9,
    usb_req_get_iface = 10,
    usb_req_set_iface = 11,
    usb_req_synch_frame = 12
} usb_req;

typedef struct
{
    uint8_t bmReqType, bReq;
    uint16_t wValue, wIndex, wLength;
} __attribute__((packed)) usb_header;

#endif // USB_H
