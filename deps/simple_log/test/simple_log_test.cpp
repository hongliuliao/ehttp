/*
 * simple_log_test.cpp
 *
 *  Created on: Oct 24, 2014
 *      Author: liao
 */
#include <stdio.h>
#include <sys/time.h>
#include <iostream>
#include "simple_log.h"

int main(int argc, char **argv) {
	// init_log_config("conf/simple_log.conf");

    int run_num = 100000;
	struct timeval start, end;
	gettimeofday(&start, NULL);

	for (unsigned i = 0; i < run_num; i++) {
		//LOG_ERROR("%s", "this is a ERROR log");
		//LOG_WARN("%s", "this is a WARN log");
		LOG_INFO("%s", "this is a info log");
		LOG_DEBUG("%s", "this is a DEBUG log");
		if(argc != 1) {
			sleep(1);
		}
	}

	gettimeofday(&end, NULL);
	int cost_time = (end.tv_sec-start.tv_sec)*1000 + (end.tv_usec-start.tv_usec)/1000;
	LOG_INFO("RUN TIME:%d, cost_time:%d ms", run_num, cost_time);
	return 0;
}
