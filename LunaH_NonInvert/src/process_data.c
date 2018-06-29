/*
 * process_data.c
 *
 *  Created on: May 9, 2018
 *      Author: gstoddard
 */

#include "process_data.h"

///// Structure Definitions ////
struct event_raw {			// Structure is 8+4+8+8+8+8= 44 bytes long
	double time;
	long long total_events;
	long long event_num;
	double bl;
	double si;
	double li;
	double fi;
	double psd;
	double energy;
};

struct cps_data {
	unsigned short n_psd;
	unsigned short counts;
	unsigned short n_no_psd;
	unsigned short n_wide_cut;
	unsigned int time;
	unsigned char temp;
};

struct event_by_event {
	unsigned short u_EplusPSD;
	unsigned int ui_localTime;
	unsigned int ui_nEvents_temp_ID;
};

struct counts_per_second {
	unsigned char uTemp;
	unsigned int ui_nPSD_CNTsOverThreshold;
	unsigned int ui_nNoPSD_nWideCuts;
	unsigned int ui_localTime;
};

int process_data(unsigned int * data_array, FIL data_file)
{
	//have the data we need to process in data_array, there are 512*4 events
	//get access to buffers we will use to sort/process the data into
	//will need to create the buffers and pass in references to them (pointers)
	int bl1 = 0; int bl2 = 0; int bl3 = 0; int bl4 = 0; int bl_avg = 0;

	return 0;
}

