/*
 * interface.h
 *
 *  Created on: 5 Mar 2013
 *      Author: swan
 */

#ifndef INTERFACE_H_
#define INTERFACE_H_

/*------------------------------Functions-------------------------------------*/

/*
 * check if NIC status is up.
 * return 1 for up, 0 for down.
 */
int check_nic_up(const char * nic);

/*
 * Check if the numbers of NIC > 2
 */
int check_nic(const char * path);

/*
 * check if nic name is correct and it's up
 * return 1 for correct; 0 for wrong name.
 */
int check_nic_name(const char * nic);

#endif /* INTERFACE_H_ */
