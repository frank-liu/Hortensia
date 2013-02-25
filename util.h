#ifndef _WIFI_UTIL_
#define _WIFI_UTIL_

#define MYDEBUG 1 // for outputing debug info.
#define MAX_CH  13

/*----------------My Data structure---------------------*/
struct ap_class
{
	int cnt;			//how many APs in this AP cata
	int rssi_avg;		//average RSSI among APs
	int quality_avg;	//average signal quality among APs
};
struct ap_info
{
	struct ap_class strong;	//class for strong signal strength APs
	struct ap_class fair;	//class for fair signal strength APs
	struct ap_class weak;	//class for weak signal strength APs
};
struct channel_info
{
	struct ap_info ap;	//gathering an AP's infomation on a channel
	int free_flag;		//free flag for a channel
};

/*----------------Functions--------------------------*/
inline void usage(char *name);

void mac_addr(struct sockaddr *addr, char *buffer);

inline double freq2double(struct iw_freq *freq);
void double2string(double num, char *buffer);

void iw_essid_escape(char *dest, const char *src, const int slen);
void encoding_key(char *buffer, int buflen, const unsigned char *key, int key_size, int key_flags);

inline void iw_print_gen_ie(unsigned char *buffer, int buflen);

/*My function frank*/
int seek_Channel(int channel[]);
int set_channel(int sockfd, int ch_index, const char * iface);
void modify_hostapd_conf(int index);
int all_free_ch(int i, int step, int channel[]);
int any_free_ch(int a, int b, int step, int channel[]);
int seek_idealch(int channel[]);
#endif