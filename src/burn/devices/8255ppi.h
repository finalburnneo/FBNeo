typedef UINT8 (*PPIPortRead)();
typedef void (*PPIPortWrite)(UINT8 data);

void ppi8255_init(INT32 num);
void ppi8255_exit();
void ppi8255_reset();
void ppi8255_scan();
UINT8 ppi8255_r(INT32 which, INT32 offset);
void ppi8255_w(INT32 which, INT32 offset, UINT8 data);
void ppi8255_set_portC( INT32 which, UINT8 data );
void ppi8255_set_read_ports(INT32 which, PPIPortRead a, PPIPortRead b, PPIPortRead c);
void ppi8255_set_read_port(INT32 which, INT32 port, PPIPortRead pr);
void ppi8255_set_write_ports(INT32 which, PPIPortWrite a, PPIPortWrite b, PPIPortWrite c);
void ppi8255_set_write_port(INT32 which, INT32 port, PPIPortWrite pw);

