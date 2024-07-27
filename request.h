#ifndef __REQUEST_H__

void requestHandle(int fd);
void* pick_event_to_run(void* q_ptr);
#endif
