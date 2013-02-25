#include <stdio.h>
#include <libgen.h>
#include <unistd.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/wireless.h>

#include "util.h"
#include "event.h"

#define MAX_LENGTH  10240
#define TMP_LENGTH	256
#define MAX_COUNT  	5

#define ERR_SUCCESS	0x00
#define ERR_USAGE	0x01
#define ERR_SOCKET	0x02
#define ERR_SCAN	0x03
#define ERR_READ	0x04

void wifi_show(struct iw_event *iwe, int channel[])
{
	char tmp[TMP_LENGTH];
	memset(tmp, 0, TMP_LENGTH);

	switch (iwe->cmd)
	{
	case SIOCGIWAP:
	{
		printf("=======================================\n");
		mac_addr(&iwe->u.ap_addr, tmp);
		printf("MAC: %s\n", tmp);
	}
		break;

		/* This is the point.*/
	case SIOCGIWFREQ:
	{
		double freq = freq2double(&iwe->u.freq);
		uint f, channel_index;
		f = (uint) freq / 1e6; // frequency in Mhz
		channel_index = (f - 2407) / 5; //calculate channel index

#ifdef MYDEBUG
		printf(" %d %d  ", f, channel_index);
#endif
		if (freq <= 1000) /* It's Channel */
		{
			printf("Channel: %d\n", (int) freq);
		}
		else /* It's freq */
		{
			double2string(freq, tmp);
			printf("Frequency: %sHz\n", tmp);
			channel[channel_index] = 1;
		}

		/*Print out channel usage.*/
#ifdef MYDEBUG
		printf("Channel     :0\t1\t2\t3\t4\t5\t6\t7\t8\t9\t10\t11\t12\t13\n");
		printf("Channel[14]= ");
		for (f = 0; f <= 13; f++)
		{
			printf("%d\t", channel[f]);
		}
		printf("\n");
#endif
	}
		break;
	case SIOCGIWENCODE:
	{
		char key[IW_ENCODING_TOKEN_MAX];
		memset(key, 0, sizeof(key));
		memcpy((void*) key, iwe->u.encoding.pointer, iwe->u.encoding.length);
		printf("Encryption key: ");

		if (iwe->u.encoding.flags & IW_ENCODE_DISABLED)
			printf("off\n");
		else
		{
			encoding_key(tmp, TMP_LENGTH, (const unsigned char *) key,
					iwe->u.encoding.length, iwe->u.encoding.flags);
			printf("%s", tmp);

			if ((iwe->u.data.flags & IW_ENCODE_INDEX) > 1)
				printf("\t[%d]", iwe->u.data.flags & IW_ENCODE_INDEX);
			if (iwe->u.data.flags & IW_ENCODE_RESTRICTED)
				printf("\tSecurity mode:restricted");
			if (iwe->u.data.flags & IW_ENCODE_OPEN)
				printf("\tSecurity mode:open");

			printf("\n");
		}
	}
		break;
	case SIOCGIWESSID:
	{
		char essid[IW_ESSID_MAX_SIZE + 1];
		memset(essid, 0, sizeof(essid));

		if (iwe->u.essid.pointer && iwe->u.essid.length)
			memcpy(essid, iwe->u.essid.pointer, iwe->u.essid.length);

		if (iwe->u.essid.flags)
			printf("ESSID:'%s'\n", essid);
		else
			printf("ESSID:off/any/hidden\n");
	}
		break;
	case SIOCGIWRATE:
	{
		double2string(iwe->u.bitrate.value, tmp);
		printf("Bit Rate: %sb/s\n", tmp);
	}
		break;
	case SIOCGIWMODE:
	{
		const char *mode[] =
		{ "Auto", "Ad-Hoc", "Managed", "Master", "Repeater", "Secondary",
				"Monitor", "Unknown/bug" };

		printf("Mode: %s\n", mode[iwe->u.mode]);
	}
		break;
	case IWEVQUAL:
	{
		printf("Quality: %d  Level:%d  Noise: %d\n", iwe->u.qual.qual,
				iwe->u.qual.level, iwe->u.qual.noise);
	}
		break;
	case IWEVCUSTOM:
	{
		char custom[IW_CUSTOM_MAX + 1];

		if ((iwe->u.data.pointer) && (iwe->u.data.length))
			memcpy(custom, iwe->u.data.pointer, iwe->u.data.length);
		custom[iwe->u.data.length] = '\0';

		printf("Extra: %s\n", custom);
	}
		break;
	case IWEVGENIE:
	{
		printf("IWEVGENIE, get encryption method, we don't care about it.\n");
		//iw_print_gen_ie(iwe->u.data.pointer, iwe->u.data.length);
	}
		break;
	default:
	{
		printf("[DEBUG.IOCTL] cmd = 0x%X, len = %d\n", iwe->cmd, iwe->len);
		break;
	}
	}
}

/* 1 = success, 0 = error */
int wifi_scan(int fd, const char *wlan, struct iwreq *req)
{
	if (fd < 0 || NULL == wlan || NULL == req)
		return 0;

	/* set interface */
	strcpy(req->ifr_ifrn.ifrn_name, wlan);

	/* set param for scanning */
	req->u.data.pointer = NULL;
	req->u.data.length = 0;
	req->u.data.flags = IW_SCAN_ALL_ESSID | IW_SCAN_ALL_FREQ | IW_SCAN_ALL_MODE
			| IW_SCAN_ALL_RATE;

	/* begin scanning  */
	if (ioctl(fd, SIOCSIWSCAN, req) == -1)
	{
		printf("%s\n", strerror(errno));
		return 0;
	}

	return 1;
}

/* 1 = success, 0 = error */
int wifi_read(int fd, const char *wlan, struct iwreq *req)
{
	int count;

	if (fd < 0 || NULL == wlan || NULL == req)
		return 0;

	/* set interface */
	strcpy(req->ifr_ifrn.ifrn_name, wlan);

	req->u.data.flags = 0;
	req->u.data.length = MAX_LENGTH;
	req->u.data.pointer = (unsigned char *) malloc(MAX_LENGTH);
	memset(req->u.data.pointer, 0, MAX_LENGTH);

	/* waiting for complete */
	for (count = 0; count < MAX_COUNT; count++)
	{
		sleep(1);

		/* try to get result */
		if (ioctl(fd, SIOCGIWSCAN, req) != -1)
			return 1;
#ifdef MYDEBUG
		else
			printf("[DEBUG] errno=%d, strerror=%s\n", errno, strerror(errno));
#endif
	}

	return 0;
}

int main(int argc, char **argv)
{
	int sock = 0, c;
	const char *wlan = NULL;
	struct iwreq req;
	struct event_iter evi;
	struct iw_event iwe;
	int ret, i = 0;
	uint ch;
	int channel[14] =
	{ 0 }; //1 for occupied, 0 for free.

	/* check param */
	if (argc < 2)
	{
		usage(basename(argv[0]));
		return ERR_USAGE;
	}

	wlan = (const char *) argv[1];

	/* open socket */
	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0)
		return ERR_USAGE;

	/* scan (enable scan)*/
	if (wifi_scan(sock, wlan, &req) == 0)
		return ERR_SCAN;

	/* get result */
	if (wifi_read(sock, wlan, &req) == 0)
		return ERR_READ;

	/* show result */
	event_iter_init(&evi, &req);
	while ((ret = event_iter_next(&evi, &iwe)) != 1)
	{
		if (0 == ret)
		{
#ifdef MYDEBUG
			printf("\n[%d]\n", ++i);
#endif
			/* show wifi result */
			wifi_show(&iwe, channel);

			/*判断在这里写*/
			if (&iwe.u.freq)
			{
				ch = ((uint) freq2double(&iwe.u.freq) / 1e6 - 2407) / 5;
				if (ch > 0 && ch < 14)
				{
					printf("Channel %d is used.\n", ch);
				}
			}
			else
			{
				printf("No channel available.\n");
			}
		}
	}
	event_iter_term(&evi);

	c = seek_Channel(channel); // return a channel index.
	//set_channel(sock,c,wlan);

	/*set hostapd config file*/
	if (c != 0)
	{
		printf("change channel to %d\n",c);
		modify_hostapd_conf(c);
	}

	/* close socket */
	close(sock);

	return 0;
}