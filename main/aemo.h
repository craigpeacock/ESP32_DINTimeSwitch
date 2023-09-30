#ifndef MAIN_AEMO_H_
#define MAIN_AEMO_H_

#include <time.h>

#define WEB_SERVER "aemo.com.au"
#define WEB_PORT "443"
#define WEB_URL "https://aemo.com.au/aemo/apps/api/report/ELEC_NEM_SUMMARY"

struct aemo {
	char *region;
	bool valid;
	struct tm settlement;
	double price;
	double totaldemand;
	double netinterchange;
	double scheduledgeneration;
	double semischeduledgeneration;
	double renewables;
};

extern struct aemo aemo_data;

void parse_aemo_json(char *ptr, struct aemo *aemo_data);
void aemo_get_price(struct aemo *aemo_data);

#endif
