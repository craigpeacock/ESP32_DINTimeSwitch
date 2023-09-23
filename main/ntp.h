#ifndef MAIN_NTP_H_
#define MAIN_NTP_H_

void time_sync_notification_cb(struct timeval *tv);
void ntp_init(void);
void display_local_time(void);

#endif