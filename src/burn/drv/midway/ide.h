#ifndef IDE
#define IDE

#include "burnint.h"
#include "burn_hdd.h"

namespace ide
{

using namespace std;

class ide_disk
{
public:
    void reset();
	void write(unsigned offset, unsigned value);
    void write_alternate(unsigned offset, unsigned value);
    unsigned read(unsigned offset);
    unsigned read_alternate(unsigned offset);
	int load_hdd_image(int idx);
    ide_disk();
    ~ide_disk();

    void set_irq_callback(void (*irq)(int state));
    void scan(INT32 nAction);


private:
    void execute();
    void build_identify_buffer();

    unsigned chs_to_lba(int cylinder, int head, int sector);
    void chs_next_sector();
    unsigned lba_from_regs();
    bool is_drive_ready();
    void raise_interrupt();
    void clear_interrupt();

    // commands
    void cmd_exec_drive_diag();
    void cmd_init_drive_params();
    void cmd_read_long();
    void cmd_read_long_wor();
    void cmd_read_sector();
    void cmd_read_sector_wor();
    void cmd_write_long();
    void cmd_write_long_wor();
    void cmd_write_sector();
    void cmd_write_sector_wor();
    void cmd_indentify_drive();

    void (*m_irq_callback)(int state);

    unsigned short *m_buffer;
    int m_buffer_pos;
    unsigned m_last_buffer_lba;

    void setup_transfer(int mode);
    void start_sector(bool first);
    void update_transfer();


    short m_identify_buffer[256];

    int m_transfer_count;
    int m_transfer_operation;

    int m_num_cylinders;
    int m_num_heads;
    int m_num_sectors;
    int m_num_bytes_per_sector;

    int m_device_control;
    int m_error;
    int m_sector_count;
    int m_sector_number;
    int m_cylinder_low;
    int m_cylinder_high;
    int m_drive_head;
    int m_status;
    int m_features;
    int m_command;

    int m_default_heads;
    int m_default_sectors;

    BurnHDD *m_disk;
};

}

#endif // IDE
