#ifndef MAIN_AEMO_H_
#define MAIN_AEMO_H_

struct aemo {
	char *region;
	char *settlement;
	double price;
	double totaldemand;
	double netinterchange;
	double scheduledgeneration;
	double semischeduledgeneration;
};

void parse_aemo_json(char *ptr, struct aemo *aemo_data);
void aemo_get_price(struct aemo *aemo_data);

#endif
