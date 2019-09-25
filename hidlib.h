//
// HID functions.
//
int hid_init(int vid, int pid);
void hid_close(void);
void hid_send_recv(const unsigned char *data, unsigned nbytes, unsigned char *rdata, unsigned rlength);

extern int debug_level;
