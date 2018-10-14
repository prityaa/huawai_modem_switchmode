
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include "huawie_communication.h"

#define DBG
//#undef DBG
#ifdef DBG
#define dbg     printf
#else
#define dbg(...)
#endif

#define LOG
//#undef LOG
#ifdef LOG
#define log(...) \
        fprintf(stderr, "%s:%s:%d: ", __FILE__, __func__, __LINE__); \
        fprintf(stderr, __VA_ARGS__)
#else
#define log(...)
#endif

#define BULK_EP_OUT_ADDR	0x1
#define BULK_EP_IN_ADDR	0x81
#define BULK_PID_1175_IFACE_NO	0
#define BULK_OUT_XFER	0
#define BULK_IN_XFER	0x80
#define BULK_XFER_TYPE	0x3

char MessageContent[SZ_1KB];
char MessageContent2[SZ_1KB];
char MessageContent3[SZ_1KB];
char ByteString[SZ_1KB/2];
int errno;

static int bulk_xfer(struct huawei_data *data, unsigned char ep, char *buf, int len, int timeout);

inline int bulk_read(struct huawei_data *data, char *message, int length)
{
	return bulk_xfer(data, BULK_EP_IN_ADDR, message, length, 3000);
}

inline int bulk_write(struct huawei_data *data, char *message, int length)
{
	dbg("%s : length = %d and buffer = %s\n", __func__, length, message);
	return bulk_xfer(data, BULK_EP_OUT_ADDR, message, length, 3000);
}

int open_device(struct huawei_data *data)
{
	int fd = -ENODEV;

	if ((fd = open(USBFS_DEV_FILE, O_RDWR)) < 0) {
		perror("GETDRIVER");
		return -errno;
	}

	data->fd = fd;
	data->ep_out_addr = BULK_EP_OUT_ADDR;
	data->ep_in_addr = BULK_EP_IN_ADDR;
	return 0;
}

/**
  * find the active driver and detach it
  * om the system
  */

int detach_storage_driver(struct huawei_data *data)
{
	struct usbfs_getdriver getdrv;
	struct usbfs_ioctl command;
	int ret;
	int fd = data->fd;


	log("fd = %d\n", fd);
	getdrv.interface = BULK_PID_1175_IFACE_NO;
	ret = ioctl(fd, IOCTL_USBFS_GETDRIVER, &getdrv);
	log("ret(GETDRIVER) = %d\n", ret);
	if (ret) {
		perror("GETDRIVER");
		return -errno;
	}

	log("get driver active driver = %s, errno = %d, ret = %d\n",
		getdrv.driver, errno, ret);
	printf("detaching %s driver\n", getdrv.driver);

	command.ifno = BULK_PID_1175_IFACE_NO;
	command.ioctl_code = IOCTL_USBFS_DISCONNECT;
	command.data = NULL;

	ret = ioctl(fd, IOCTL_USBFS_IOCTL, &command);
	log("detaching  %s, errno = %d, ret = %d\n",
		getdrv.driver, errno, ret);
	if (ret) {
		return -errno;
	}

	return 0;
}

static int ep_clear_halt(struct huawei_data *data, unsigned int _endpoint)
{
	int fd = data->fd;
	int ret;

	ret = ioctl(fd, IOCTL_USBFS_CLEAR_HALT, &_endpoint);
	log("clear halt ret %d errno %d\n", ret, errno);
	if (ret) {
		printf("Failed clear_halt error\n");
		return -errno;
	}

	return 0;
}

static int claim_interface(struct huawei_data *data, int iface)
{
	int fd = data->fd;
	int ret;

	ret = ioctl(fd, IOCTL_USBFS_CLAIMINTF, &iface);
	log("fd = %d, ret %d errno %d\n", fd, ret, errno);
	if (ret) {
		printf("Failed claim interface\n");
		return -errno;
	}

	return 0;
}

int set_up_interface(struct huawei_data *data)
{
	int  ret;

	if ((claim_interface(data, BULK_PID_1175_IFACE_NO)) &&
 		(ep_clear_halt(data, data->ep_out_addr))) {
		printf("Failed to claim and halt iface\n");
		return ret;
	}

	return 0;
}

static int submit_urb(struct huawei_data *data, struct usbfs_urb **urb)
{
	int ret;

	ret = ioctl(data->fd, IOCTL_USBFS_SUBMITURB, *urb);
	dbg("%s : submiturb ret = %d errno = %d\n", __func__, ret, errno);
	if (ret < 0) {
		if (errno != ENODEV) {
			dbg("%s : submiturb failed error %d errno = %d\n", __func__, ret, errno);
			ret = -EIO;
		}

		return ret;
	}

	return 0;
}

typedef void (*transfer_cb_fn)(struct huawei_data *data);

static void sync_transfer_cb(struct huawei_data *data)
{
	int *completed = &data->user_data;
	*completed = 1;
	log("actual_length=%d", data->actual_length);
	/* caller interprets result and frees transfer */
}


static void fill_bulk_xfer_urb(struct huawei_data *data, struct usbfs_urb *urb,
		unsigned char endpoint, unsigned char *buffer, int length,
		                int *transferred, unsigned int timeout)
{
	urb->type = BULK_XFER_TYPE;
	urb->endpoint = endpoint & 0xff;
	urb->buffer = buffer;
	urb->buffer_length = length;
	urb->usercontext = data;
	urb->flags = 0;
	//urb->timeout = timeout;
	//urb->callback = callback;
}

static struct usbfs_urb * alloc_urb(int nr_urbs)
{
	struct usbfs_urb *urbs;

	urbs = calloc(nr_urbs, sizeof(struct usbfs_urb));
	if (!urbs)
		return NULL;

	return urbs;
}

static free_urbs(struct usbfs_urb *urbs)
{
	free(urbs);
}

static int __bulk_xfer(struct huawei_data *data,
		unsigned char endpoint, unsigned char *buffer, int length,
		int *transferred, unsigned int timeout)
{
	int completed = 0;
	int r = 0, i = 0, t_length;
	struct usbfs_urb *urbs;
	int nr_urbs = length / 512;

	if (length == 0) {
		nr_urbs = 1;
	} else if ((length % 512) > 0) {
		nr_urbs++;
	}

	urbs = alloc_urb(nr_urbs);
	dbg("%s : urbs = %p, nr_urbs = %d\n", __func__, urbs, nr_urbs);
	if (urbs == NULL)
		return -ENOMEM;

	for (; i < nr_urbs && !r; i++) {
		struct usbfs_urb *urb = &urbs[i];

		t_length = (length > 512) ? 512 : length;

		fill_bulk_xfer_urb(data, urb, endpoint, buffer+(i*512), t_length,
				&completed, timeout);

		dbg("%s : buffer = %s, %p\n", __func__, buffer, buffer);
#define DEBUG_URB
#ifdef DEBUG_URB
		dbg("%s : urb_buffer ptr = %p,  buffer = %s, urb_len = %d, ep = %x\n",
			__func__, urb->buffer, (char *)urb->buffer, urb->buffer_length, urb->endpoint);
#endif
		r = submit_urb(data, &urb);
		if (r < 0) {
			free_urbs(urbs);
			return r;
		}

		usleep(3000);

		if (transferred)
			*transferred = data->actual_length;

		dbg("%s : transfer->status = %d\n", __func__, data->status);
		dbg("%s : urb->status = %d actual_length = %d\n", __func__, urb->status, urb->actual_length);
		switch (data->status) {
			case USB_TRANSFER_COMPLETED:
				r = 0;
				break;
			case USB_TRANSFER_TIMED_OUT:
				r = -ETIMEDOUT;
				break;
			case USB_TRANSFER_STALL:
				r = -EPIPE;
				break;
			case USB_TRANSFER_OVERFLOW:
				r = -EOVERFLOW;
				break;
			case USB_TRANSFER_NO_DEVICE:
				r = -ENODEV;
				break;
			case USB_TRANSFER_ERROR:
			case USB_TRANSFER_CANCELLED:
				r = -EIO;
				break;
			default:
				r = -EEXIST;
		}

		length  -= 512;
		//dbg("%s : r = %d, length = %d\n", __func__, r, length);
	}

	free_urbs(urbs);
	return r;
}

static int bulk_xfer(struct huawei_data *data, unsigned char ep, char *buf, int len, int timeout)
{
	int actual_length, ret;

	ret = __bulk_xfer(data,
			ep, (unsigned char *)buf, len,
			&actual_length, timeout);

	if (ret == 0 || (ret == -7 && actual_length > 0))
		return actual_length;

	return ret;
}

int hex2num(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}


int hex2byte(const char *hex)
{
	int a, b;
	a = hex2num(*hex++);
	if (a < 0)
		return -1;
	b = hex2num(*hex++);
	if (b < 0)
		return -1;
	return (a << 4) | b;
}

int hexstr2bin(const char *hex, char *buffer, int len)
{
	int i;
	int a;
	const char *ipos = hex;
	char *opos = buffer;

	for (i = 0; i < len; i++) {
		a = hex2byte(ipos);
		if (a < 0)
			return -1;
		*opos++ = a;
		ipos += 2;
	}
	return 0;
}

int send_eject_sequence(struct huawei_data *data)
{
	int ret, i;
	const char* cmdHead = "55534243";
	char *msg[3] = {MessageContent, MessageContent2, MessageContent3};

	msg[0] = MessageContent;
	msg[1] = MessageContent2;
	msg[2] = MessageContent3;

	dbg("%s : con = %p, msg[0] = %p \n", __func__, MessageContent, msg[0]);

	ret = set_up_interface(data);
	log("ret(set_up_interface) = %d\n", ret);
	if (ret < 0) {
		printf("Failed to setup interface\n");
		return ret;
	}

	for (i=0; i<3; i++) {
		if ( strlen(msg[i]) == 0)
			continue;

		if ( hexstr2bin(msg[i], ByteString, strlen(msg[i]) / 2) == -1) {
			fprintf(stderr, "Error: MessageContent %d %s\n is not a hex string. Skipping ...\n", i, MessageContent);
			return 1;
		}

		ret = bulk_write(data, msg[i], strlen(msg[i]));
		dbg("%s : ret (bulk_write) = %d\n", __func__, ret);
		if (ret)
			break;

		if ( strstr(msg[i], cmdHead) != NULL ) {
			// UFI command
			log("Read the response to message %d (CSW) ...\n", i+1);
			ret = bulk_read(data, ByteString, 13);
		} else {
			log("Read the response to message %d ...\n", i+1);
			ret = bulk_read(data, ByteString, strlen(msg[i])/2 );
		}

		log("ret(read_bulk) = %d\n", ret);
		if (ret < 0)
			break;
	}

	return ret;
}

static int release_interface(struct huawei_data *data, int iface)
{
	int fd = data->fd;
	int r = ioctl(fd, IOCTL_USBFS_RELEASEINTF, &iface);
	log("release interface ret = %d errno = %d\n", r, errno);
	if (r) {
		printf("release interface failed\n");
		return r;
	}
	return 0;
}

int main()
{
	struct huawei_data data;
	int ret;

	memset(&data, '\0', sizeof(struct huawei_data));

	ret = open_device(&data);
	if (ret) {
		printf("Failed to open device\n");
		return ret;
	}

	ret = detach_storage_driver(&data);
	log("ret = %d\n", ret);
	if (ret) {
		printf("Failed to dettach driver, it may already be detached\n");
		return ret;
	}

	memset(MessageContent, '\0', sizeof(MessageContent));
	strcpy(MessageContent,"5553424312345678000000000000061e000000000000000000000000000000");
	memset(MessageContent2, '\0', sizeof(MessageContent2));
	strcpy(MessageContent2, "5553424312345679000000000000061b000000020000000000000000000000");
	memset(MessageContent3, '\0', sizeof(MessageContent3));

	ret = send_eject_sequence(&data);
	if (ret < 0) {
		printf("Falied to send eject sequence\n");
		goto close_device;
	}

	usleep(50000);
#if 1
	printf("Reset message endpoint 0x%02x\n", data.ep_in_addr);
	ret = ep_clear_halt(&data, data.ep_in_addr);
	if (ret)
		printf("Could not reset endpoint = %x\n", data.ep_in_addr);

	printf("Reset response endpoint 0x%02x\n", data.ep_out_addr);
	ret = ep_clear_halt(&data, data.ep_out_addr);
	if (ret)
		printf("Could not reset endpoint = %x\n", data.ep_out_addr);
#endif
	usleep(50000);

	ret = release_interface(&data, BULK_PID_1175_IFACE_NO);
	if (ret < 0) {
		printf("Failed to release interface\n");
		goto close_device;
	}
	printf("Interface realeased\n");

close_device:
	if(data.fd)
		close(data.fd);

	return ret;
}
