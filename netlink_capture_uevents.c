#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <unistd.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <linux/netlink.h>

#define NETLINK_TEST    17
#define MYGROUP         1

#define MAX_PAYLOAD 1024  /* maximum payload size*/
struct sockaddr_nl src_addr, dest_addr;
union {
	struct nlmsghdr nlh;
	char raw[8192];
} buf;
struct iovec iov;
int sock_fd;
struct msghdr msg;

int main()
{
	int ret, fd, i;
	char *event_buf;
	char action[50];
#undef USE_USB_PORT
#ifdef USE_USB_PORT

	fd = open("/dev/bus/usb/001/020", O_RDWR);
	if (fd < 0) {
		printf("Unable to open port\n");
		return -1;
	}
#endif
	printf("Creating socket\n");
	sock_fd=socket(PF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT);
	if(sock_fd<0) {
		printf("Socket creating failed\n");
		return -1;
	}

	memset(&src_addr, 0, sizeof(src_addr));
	src_addr.nl_family = AF_NETLINK;
	src_addr.nl_pid = getpid();  /* self pid */
	src_addr.nl_groups = MYGROUP;
	/* interested in group 1<<0 */
	ret = bind(sock_fd, (struct sockaddr*)&src_addr,
			sizeof(src_addr));
	if (ret < 0) {
		printf("Bind Failed\n");
		perror("bind:");
		return -1;
	}

	memset(&dest_addr, 0, sizeof(dest_addr));
	dest_addr.nl_family = AF_NETLINK;
	dest_addr.nl_pid = 0;   /* For Linux Kernel */
	dest_addr.nl_groups = 0; /* unicast */

	memset(&buf, 0, sizeof(buf));
	iov.iov_base = &buf;
	iov.iov_len = sizeof(buf);
	msg.msg_name = (void *)&dest_addr;
	msg.msg_namelen = sizeof(dest_addr);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
#if 1
	/* Read message from kernel */
	printf("%s :Reading the events\n", __func__);
	while (1) {
		recvmsg(sock_fd, &msg, 0);
		printf("msg = %s\n", buf.raw);
		if (strstr(buf.raw, "usb") != NULL) {
			event_buf = buf.raw;
			for (i = 0; event_buf[i] != '@'; i++)
				action[i] = event_buf[i];
			action[i] = '\0';
			printf("ACTION = %s\n", action);
#ifdef USE_USB_PORT
			if ((strcmp(action, "remove")) == 0) {
				close(fd);
				printf("USB port closed\n");
			}
#endif
		}
	}
end:
	close(sock_fd);
#endif
	return 0;
}
