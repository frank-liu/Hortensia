#include <stdio.h>
#include <ctype.h>
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

#define G (1e9)
#define M (1e6)
#define K (1e3)

inline void usage(char *name)
{
	printf("usage: %s <interface>\n", name);
	printf("\te.g.: sudo ./%s wlan0\n", name);
	printf("\tFrank LIU, <frank dot kingdom at gmail dot com>\n");
}
/*
 * modify file contents using a shell script.
 * -index is channel index between [1,13].
 */
void modify_hostapd_conf(int index)
{
	FILE * fp;
	char buffer[80];
	char cmd_sh[80] = "sh ~/workspace/wifi/myconfig_channel\t", ch[3];

	//printf("change channel to %d\n",index);
	sprintf(ch, "%d", index); //convert int to char.
	strcat(cmd_sh, ch); //connect two strings, and stores the result in first arg.
	//printf("%s\n",cmd_sh);

	//fp = popen("sh ~/workspace/wifi/myconfig_channel 5", "w");
	fp = popen(cmd_sh, "w"); //call a shell script:myconfig_channel
	fgets(buffer, sizeof(buffer), fp);
	pclose(fp);

}
/*
 * are all of channels free in radius step?
 */
int all_free_ch(int i, int step, int channel[])
{
	int a, b, j;
	a = (i - step) <= 0 ? 1 : (i - step);
	b = (i + step) > MAX_CH ? MAX_CH : (i + step);
	for (j = a; j <= b; j++)
	{
		if (channel[j] == 1)
			break;
		if (j == b) //no used channel till channel[b], i.e.: all channels around i are free
			return i;
	}
	return 0;
}
/*
 * is there a free channel at least? return the index.
 */
int any_free_ch(int a, int b, int step, int channel[])
{
	int i;
	if (a >= b)
		return 0;
	if (b < 0)
		b = 1;
	if (a > MAX_CH)
		a = 13;
	for (i = a; i <= b; i++)
	{
		if (channel[i] == 0)
		{
			if (all_free_ch(i, step, channel) > 0)
			{
				break;
			}
		}
		if (i == b)
			return 0;
	}
	return i; //not found
}
/*
 * seek the best channel: no interference with other channels
 * 0 for failure,
 */
int seek_idealch(int channel[])
{
	int ch, i, step = 5;
	do
	{
		for (i = 1; i <= MAX_CH; i++)
		{
			if (channel[i] == 0)
			{
				if ((ch = any_free_ch(1, i - step, step, channel)) != 0)
				{
					//flag = 1;
					printf("suitable Channel:%d, step=%d\n", ch, step);
					return ch;
				}
				else if ((ch = any_free_ch(i + step, MAX_CH, step, channel))
						!= 0)
				{
					//flag = 1;
					printf("suitable Channel:%d, step=%d\n", ch, step);
					return ch;
				}
				else
				{
					printf("NO suitable Channel, step=%d\n", step);
					//step--;
				}
			}
		}
	} while (step--);
	return 0; //not found
}

/*
 * My function just seeking a valid channel.
 */
int seek_Channel(int channel[])
{
	int i, index = 0; //default not found.
	for (i = 1; i < 14; i++)
	{
		if (channel[i] == 0)
		{
			printf("find a free channel:  %d\n", i);
			index = i;
			break;
		}
	}
	index = seek_idealch(channel);
	if (index == 0)
	{
		// dont find a free channel.
		printf("Dont find a suitable channel.\n");
	}
	return index; // find a free channel
}

/*
 * My function : Set Channel
 */
int set_channel(int sockfd, int ch_index, const char * iface)
{
	struct iwreq wreq;
	memset(&wreq, 0, sizeof(struct iwreq));
	sprintf(wreq.ifr_name, iface); //?

	/*Assign a specific channel*/
	wreq.u.freq.flags = IW_FREQ_FIXED; /* Force a specific value */
	wreq.u.freq.e = 0;
	wreq.u.freq.m = ch_index;

	/*Set Frequency info from ioctl*/
	if (ioctl(sockfd, SIOCSIWFREQ, &wreq) == -1)
	{
		perror("IOCTL SIOCSIWFREQ Failed,error");
		exit(2);
	}

	printf("\nChannel is set to: %d\n", wreq.u.freq.m);
	return 0;
}

void mac_addr(struct sockaddr *addr, char *buffer)
{
	sprintf(buffer, "%02X:%02X:%02X:%02X:%02X:%02X",
			(unsigned char) addr->sa_data[0], (unsigned char) addr->sa_data[1],
			(unsigned char) addr->sa_data[2], (unsigned char) addr->sa_data[3],
			(unsigned char) addr->sa_data[4], (unsigned char) addr->sa_data[5]);
}

inline double freq2double(struct iw_freq *freq)
{
	return ((double) freq->m) * pow(10, freq->e);
}

void double2string(double num, char *buffer)
{
	char scale = '\0';

	if (num >= G)
	{
		scale = 'G';
		num /= G;
	}
	else if (num >= M)
	{
		scale = 'M';
		num /= M;
	}
	else if (num >= K)
	{
		scale = 'K';
		num /= K;
	}

	sprintf(buffer, "%g %c", num, scale);
}

void iw_essid_escape(char * dest, const char * src, const int slen)
{
	const unsigned char * s = (const unsigned char *) src;
	const unsigned char * e = s + slen;
	char * d = dest;

	/* Look every character of the string */
	while (s < e)
	{
		int isescape;

		/* Escape the escape to avoid ambiguity.
		 * We do a fast path test for performance reason. Compiler will
		 * optimise all that ;-) */
		if (*s == '\\')
		{
			/* Check if we would confuse it with an escape sequence */
			if ((e - s) > 4 && (s[1] == 'x') && (isxdigit(s[2]))
					&& (isxdigit(s[3])))
			{
				isescape = 1;
			}
			else
				isescape = 0;
		}
		else
			isescape = 0;

		/* Is it a non-ASCII character ??? */
		if (isescape || !isascii(*s) || iscntrl(*s))
		{
			/* Escape */
			sprintf(d, "\\x%02X", *s);
			d += 4;
		}
		else
		{
			/* Plain ASCII, just copy */
			*d = *s;
			d++;
		}
		s++;
	}

	/* NUL terminate destination */
	*d = '\0';
}

void encoding_key(char * buffer, int buflen, const unsigned char * key, /* Must be unsigned */
int key_size, int key_flags)
{
	int i;

	/* Check buffer size -> 1 bytes => 2 digits + 1/2 separator */
	if ((key_size * 3) > buflen)
	{
		snprintf(buffer, buflen, "<too big>");
		return;
	}

	/* Is the key present ??? */
	if (key_flags & IW_ENCODE_NOKEY)
	{
		/* Nope : print on or dummy */
		if (key_size <= 0)
			strcpy(buffer, "on"); /* Size checked */
		else
		{
			strcpy(buffer, "**"); /* Size checked */
			buffer += 2;
			for (i = 1; i < key_size; i++)
			{
				if ((i & 0x1) == 0)
					strcpy(buffer++, "-"); /* Size checked */
				strcpy(buffer, "**"); /* Size checked */
				buffer += 2;
			}
		}
	}
	else
	{
		/* Yes : print the key */
		sprintf(buffer, "%.2X", key[0]); /* Size checked */
		buffer += 2;
		for (i = 1; i < key_size; i++)
		{
			if ((i & 0x1) == 0)
				strcpy(buffer++, "-"); /* Size checked */
			sprintf(buffer, "%.2X", key[i]); /* Size checked */
			buffer += 2;
		}
	}
}

static void iw_print_ie_unknown(unsigned char *iebuf, int buflen)
{
	int ielen = iebuf[1] + 2;
	int i;

	if (ielen > buflen)
		ielen = buflen;

	printf("Unknown: ");
	for (i = 0; i < ielen; i++)
		printf("%02X", iebuf[i]);
	printf("\n");
}

inline void iw_print_gen_ie(unsigned char *buffer, int buflen)
{
	int offset = 0;

	/* Loop on each IE, each IE is minimum 2 bytes */
	while (offset <= (buflen - 2))
	{
		printf("IE: ");

		/* Check IE type */
		switch (buffer[offset])
		{
		case 0xdd: /* WPA1 (and other) */
		case 0x30: /* WPA2 */
			//iw_print_ie_wpa(buffer + offset, buflen);
			//break;
		default:
			iw_print_ie_unknown(buffer + offset, buflen);
		}
		/* Skip over this IE to the next one in the list. */
		offset += buffer[offset + 1] + 2;
	}
}
