#ifndef NSV_H
#define NSV_H

const struct nsv_version_info
{
	int major;
	int minor;
	int fix;
	char build_time[20];
	char build_date[20];
} nsv_version = { 1,5,0,__TIME__,__DATE__};

#endif //NSV_H
