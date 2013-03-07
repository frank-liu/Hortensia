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
//#include <unistd.h> // for get current work directory
#include "util.h"
#include "event.h"
#include "interface.h"

#define MAX_LENGTH  10240
#define TMP_LENGTH	256
#define MAX_COUNT  	5

#define ERR_SUCCESS	0x00
#define ERR_USAGE	0x01
#define ERR_SOCKET	0x02
#define ERR_SCAN	0x03
#define ERR_READ	0x04
#define ERR_NIC		0x05

void wifi_show(struct iw_event *iwe, struct channel_info channel[])
{
	uint f, channel_index;
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
		//uint f, channel_index;
		f = (uint) freq / 1e6; // frequency in Mhz
		channel_index = (f - 2407) / 5; //calculate channel index

#ifdef MYDEBUG
		printf(" %d %d  ", f, channel_index);
#endif
		if (freq <= 1000) /* It's Channel index*/
		{
			printf("Channel: %d\n", (int) freq);

		}
		else /* It's freq */
		{
			double2string(freq, tmp);
			printf("Frequency: %sHz\n", tmp);
			channel[channel_index].used = 1; // 1 means this channel is occupied.
		}

		/*Print out channel usage.*/
#ifdef MYDEBUG
		printf("Channel     :0\t1\t2\t3\t4\t5\t6\t7\t8\t9\t10\t11\t12\t13\n");
		printf("Channel[14]= ");
		for (f = 0; f <= MAX_CH; f++)
		{
			printf("%d\t", channel[f].used);
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
		/*s for signal in db; n for noise in db; SNR for signal-noise ratio.*/
		int s, n, snr;

		/*convert signal level in db*/
		s = (iwe->u.qual.level > 63) ?
				iwe->u.qual.level - 0x100 : iwe->u.qual.level;
		/*convert noise in db*/
		n = (iwe->u.qual.noise > 63) ?
				iwe->u.qual.noise - 0x100 : iwe->u.qual.noise;
		snr = s - n; //signal noise ratio
		channel[channel_index].snr = snr;

#ifdef MYDEBUG
		printf("\nQuality: %d  Level:%d  Noise: %d\n", iwe->u.qual.qual,
				iwe->u.qual.level, iwe->u.qual.noise);
		printf("\nQuality: %d  Level:%d  Noise: %d  SNR: %u\n",
				iwe->u.qual.qual, s, n, (unsigned int) snr);
#endif

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
		//printf("IWEVGENIE, get encryption method, we don't care about it.\n");
		//iw_print_gen_ie(iwe->u.data.pointer, iwe->u.data.length);
	}
		break;
	case SIOCGIWSTATS:
	{
		;
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
	const char *wlan = NULL, *path = NULL;
	struct iwreq req;
	struct event_iter evi;
	struct iw_event iwe;
	int ret, i = 0, j;
	uint ch;
	//char * buf = NULL;

	struct channel_info ch_info[MAX_CH + 1]; //element ch_info[0] will never be used.
	init_channelinfo(ch_info);

	/* check param */
	if (argc < 2)
	{
		usage(basename(argv[0]));
		return ERR_USAGE;
	}

	path = (const char *) argv[0];
	wlan = (const char *) argv[1];

	/* open socket */
	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0)
		return ERR_USAGE;

	/*check if nic exist and is up*/
	if (check_nic_name(wlan) != 1)
	{
		return ERR_NIC;
	}

	/* check NIC numbers */
	if (check_nic(path) < 1) // if wirless NIC # is less than 1, exit.
	{
		printf(
				"\nOops...! Available Wireless interfaces inadequate.(minimum 2)\n\n");
		//printf("There is only %s Wireless interfaces .\n");
		return ERR_NIC;
	}

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
			/* argument should be struct array: ch_info*/
			wifi_show(&iwe, ch_info);

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

	if (0) // no longer used
	{
		c = seek_Channel(ch_info); // return a channel index.
	}

	c = seek_Channel2(ch_info); // return a channel index.

#ifdef MYDEBUG //display snr value in each channel
	printf("Channel     :0\t1\t2\t3\t4\t5\t6\t7\t8\t9\t10\t11\t12\t13\n");
	printf("snr[14]= ");
	for (j = 0; j <= MAX_CH; j++)
	{
		printf("%d\t", ch_info[j].snr);
	}
	printf("\n");
#endif

	if (c == 0)
	{
#ifdef MYDEBUG //search a channel by SNR
		printf("search a channel by SNR\n");
#endif
		channel_process(ch_info);
		c = seek_Channel2(ch_info); //search a channel by SNR
	}
	else if (c >= 11) //avoid channel 12 and 13.
	{
		c = 11;
	}
	/*set hostapd config file*/
	if (0 < c && c <= MAX_CH)
	{
		printf("\n Change channel to %d\n", c);
		modify_hostapd_conf(c, path);
	}

	/* close socket */
	close(sock);

	return 0;
}
