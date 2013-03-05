/*
 * interface.c
 *
 *  Created on: 5 Mar 2013
 *      Author: Frank
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <net/if_arp.h>
#include <string.h>
#include <unistd.h>
#include "interface.h"
#define MAXINTERFACES   16

/*
 * check if NIC's status is up
 * return 1 for up, 0 for down.
 */
int check_nic_up(const char * nic)
{
	int fd, inf_cnt, rtn = 0; // interface counter(inf_cnt), return value(rtn)
	struct ifreq buf[MAXINTERFACES];
	struct ifconf ifc;

	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) >= 0)
	{
		ifc.ifc_len = sizeof buf;
		ifc.ifc_buf = (caddr_t) buf;
		if (!ioctl(fd, SIOCGIFCONF, (char *) &ifc))
		{
			inf_cnt = ifc.ifc_len / sizeof(struct ifreq);
			printf("interface num is inf_cnt=%d\n", inf_cnt);
			while (inf_cnt-- > 0)
			{
				/*Go to next loop if it's not the nic we expect*/
				if (0 != strcmp(buf[inf_cnt].ifr_name, nic))
				{
					continue;
				}

				/*Must be here in order to get SIOCGIFFLAGS*/
				if (!(ioctl(fd, SIOCGIFFLAGS, (char *) &buf[inf_cnt])))
				{
					/*Jugde whether the net card status is up       */
					if (buf[inf_cnt].ifr_flags & IFF_UP)
					{
						printf("Interface %s is UP.\n",buf[inf_cnt].ifr_name);
						rtn = 1;
					}
					else
					{
						printf("Interface %s is DOWN. It needs to be set up.\n",nic);
					}
				}

			}
		}
		else
			perror("cpm: ioctl err\n");
	}
	else
		perror("cpm: socket err\n");

	close(fd);
	return rtn;
}


/*
 * Check if the numbers of NIC > 2
 */
int check_nic(const char * path)
{
	FILE * fp;
	char cnt[5];
	char cmd_sh[100] = "sh\t", conf_name[19] = "wireless_nic\t";
	char resolved_path[100];

	sprintf(resolved_path, "%s", path);
	resolved_path[strlen(resolved_path) - 6] = '\0'; //delete last 6 words(i.e., switCH) in path

	strcat(cmd_sh, resolved_path); //combine path in shell cmd.
	strcat(cmd_sh, conf_name); //combine configure file name into shell cmd.

	fp = popen(cmd_sh, "r"); //call a shell script: wireless_nic
	fgets(cnt, sizeof(cnt), fp);
	pclose(fp);

	/*File header in /proc/net/wireless ocuppies 2 lines,
	 *so here we subtract 2 to get the correct numbers of wireless NIC*/
	return (atoi(cnt) - 2);
}

/*
 * check if nic name is correct and it's up
 * return 1 for correct; 0 for wrong name.
 */
int check_nic_name(const char * nic)
{
	FILE * fp;
	char * line = NULL;
	const char *ifn;
	size_t len = 0;
	ssize_t read;
	char *delim = ":"; //分隔符
	int cnt = 1, found = 0;

	fp = fopen("/proc/net/wireless", "r");
	if (fp == NULL)
		return 0;
	while ((read = getline(&line, &len, fp)) != -1)
	{
		//printf("Retrieved line of length %zu :\n", read);
		//printf("%s", line);

		if (cnt <= 2)
		{
			cnt++;
			continue;
		}
		cnt++;
		ifn = strtok(line, delim); //interface name with blank-space at head.
		ifn = strtok(ifn, " "); // remove blank-space
		printf("wifi interface:%s\n", ifn);

		/*Check if nic matches in /proc/net/wireless, and the nic is up.*/
		if (strcmp(ifn, nic) == 0 && check_nic_up(nic) == 1)
		{
			found = 1; // Yes, we found it.
			printf("Correct interface name: %s\n", ifn); // print interface name
			break;
		}
	}

	if (line)
		free(line);

	return found;
}
