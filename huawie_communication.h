
#ifndef __HUAWIE_MODEM_
#define __HUAWIE_MODEM_

#include <sys/ioctl.h>
#include <stdint.h>

#define USBFS_MAXDRIVERNAME 255
#define SZ_1KB	1024

struct usbfs_disconnect_claim {
	unsigned int interface;
	unsigned int flags;
	char driver[USBFS_MAXDRIVERNAME + 1];
};

struct usbfs_getdriver {
	unsigned int interface;
	char driver[USBFS_MAXDRIVERNAME + 1];
};

struct usbfs_ioctl {
	int ifno;       /* interface 0..N ; negative numbers reserved */
	int ioctl_code; /* MUST encode size + direction of data so the
			 * macros in <asm/ioctl.h> give correct values */
	void *data;     /* param buffer (in, or out) */
};

struct usbfs_urb {
	unsigned char type;
	unsigned char endpoint;
	int status;
	unsigned int flags;
	void *buffer;
	int buffer_length;
	int actual_length;
	int start_frame;
	union {
		int number_of_packets;  /* Only used for isoc urbs */
		unsigned int stream_id; /* Only used with bulk streams */
	};
	int error_count;
	unsigned int signr;
	void *usercontext;
};

enum USB_speed {
	USB_SPEED_UNKNOWN = 0,
	USB_SPEED_LOW = 1,
	USB_SPEED_FULL = 2,
	USB_SPEED_HIGH = 3,
	USB_SPEED_SUPER = 4,
	USB_SPEED_SUPER_PLUS = 5,
};

enum USB_transfer_status {
	/** Transfer completed without error. Note that this does not indicate
	 * that the entire amount of requested data was transferred. */
	USB_TRANSFER_COMPLETED,

	/** Transfer failed */
	USB_TRANSFER_ERROR,

	/** Transfer timed out */
	USB_TRANSFER_TIMED_OUT,

	/** Transfer was cancelled */
	USB_TRANSFER_CANCELLED,

	/** For bulk/interrupt endpoints: halt condition detected (endpoint
	 * stalled). For control endpoints: control request not supported. */
	USB_TRANSFER_STALL,

	/** Device was disconnected */
	USB_TRANSFER_NO_DEVICE,

	/** Device sent more data than requested */
	USB_TRANSFER_OVERFLOW,

	/* NB! Remember to update USB_error_name()
	   when adding new status codes here. */
};

struct huawei_data {
	int fd;
	int ep_out_addr;
	int ep_in_addr;
	uint8_t bus_number;
	uint8_t port_number;
	//struct USB_device* parent_dev;
	uint8_t device_address;
	uint8_t num_configurations;
	enum USB_speed speed;

	int user_data;
	int actual_length;
	int status;
};

#define USBFS_DEV_FILE	"/dev/bus/usb/001/074"

#define USBFS_URB_SHORT_NOT_OK          0x01
#define USBFS_URB_ISO_ASAP                      0x02
#define USBFS_URB_BULK_CONTINUATION     0x04
#define USBFS_URB_QUEUE_BULK            0x10
#define USBFS_URB_ZERO_PACKET           0x40

#define IOCTL_USBFS_CONTROL     _IOWR('U', 0, struct usbfs_ctrltransfer)
#define IOCTL_USBFS_BULK                _IOWR('U', 2, struct usbfs_bulktransfer)
#define IOCTL_USBFS_RESETEP     _IOR('U', 3, unsigned int)
#define IOCTL_USBFS_SETINTF     _IOR('U', 4, struct usbfs_setinterface)
#define IOCTL_USBFS_SETCONFIG   _IOR('U', 5, unsigned int)
#define IOCTL_USBFS_GETDRIVER   _IOW('U', 8, struct usbfs_getdriver)
#define IOCTL_USBFS_SUBMITURB   _IOR('U', 10, struct usbfs_urb)
#define IOCTL_USBFS_DISCARDURB  _IO('U', 11)
#define IOCTL_USBFS_REAPURB     _IOW('U', 12, void *)
#define IOCTL_USBFS_REAPURBNDELAY       _IOW('U', 13, void *)
#define IOCTL_USBFS_CLAIMINTF   _IOR('U', 15, unsigned int)
#define IOCTL_USBFS_RELEASEINTF _IOR('U', 16, unsigned int)
#define IOCTL_USBFS_CONNECTINFO _IOW('U', 17, struct usbfs_connectinfo)
#define IOCTL_USBFS_IOCTL         _IOWR('U', 18, struct usbfs_ioctl)
#define IOCTL_USBFS_HUB_PORTINFO        _IOR('U', 19, struct usbfs_hub_portinfo)
#define IOCTL_USBFS_RESET               _IO('U', 20)
#define IOCTL_USBFS_CLEAR_HALT  _IOR('U', 21, unsigned int)
#define IOCTL_USBFS_DISCONNECT  _IO('U', 22)
#define IOCTL_USBFS_CONNECT     _IO('U', 23)
#define IOCTL_USBFS_CLAIM_PORT  _IOR('U', 24, unsigned int)
#define IOCTL_USBFS_RELEASE_PORT        _IOR('U', 25, unsigned int)
#define IOCTL_USBFS_GET_CAPABILITIES    _IOR('U', 26, __u32)
#define IOCTL_USBFS_DISCONNECT_CLAIM    _IOR('U', 27, struct usbfs_disconnect_claim)
#define IOCTL_USBFS_ALLOC_STREAMS       _IOR('U', 28, struct usbfs_streams)
#define IOCTL_USBFS_FREE_STREAMS        _IOR('U', 29, struct usbfs_streams)

#endif
