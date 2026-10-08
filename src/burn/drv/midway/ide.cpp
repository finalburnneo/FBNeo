// Killer Instinct hd image:
// Tag='GDDD'  Index=0  Length=34 bytes
// CYLS:419,HEADS:13,SECS:47,BPS:512.
#include "ide.h"

#define DEBUG_ATA   0

#if DEBUG_ATA
# define ata_log(...)   printf("ata_device: " __VA_ARGS__); fflush(stdout)
#else
# define ata_log(...)
#endif

namespace ide
{


using namespace std;

enum ata_registers {
    REG_DATA = 0,
    REG_ERROR_RO = 1,
    REG_SECTOR_COUNT = 2,
    REG_SECTOR_NUMBER = 3,
    REG_CYLINDER_LOW = 4,
    REG_CYLINDER_HIGH = 5,
    REG_DRIVE_HEAD = 6,
    REG_STATUS_RO = 7,
    REG_FEATURES_WO = 1,
    REG_COMMAND_WO = 7
};

enum ata_alt_registers {
    REG_ALT_STATUS_RO = 6,
    REG_ALT_DRIVE_ADDRESS_RO = 7,
    REG_ALT_DEV_CONTROL_WO = 6
};

enum ata_commands {
    CMD_EXEC_DRIVE_DIAG = 0x90,
    CMD_FORMAT_TRACK = 0x50,
    CMD_INIT_DRIVE_PARAM = 0x91,
    CMD_READ_LONG = 0x22,
    CMD_READ_LONG_NO_RETRY = 0x23,
    CMD_READ_SECTOR = 0x20,
    CMD_READ_SECTOR_NO_RETRY = 0x21,
    CMD_VERIFY_SECTOR = 0x40,
    CMD_VERIFY_SECTOR_NO_RETRY = 0x41,
    CMD_RECALIBRATE = 0x10,
    CMD_WRITE_LONG = 0x32,
    CMD_WRITE_LONG_NO_RETRY = 0x33,
    CMD_WRITE_SECTOR = 0x30,
    CMD_WRITE_SECTOR_NO_RETRY = 0x31,
    CMD_IDENTIFY_DRIVE = 0xEC
};

enum transfer_operations {
    TRF_NONE = 0,
    TRF_SECTOR_READ,
    TRF_SECTOR_WRITE,
    TRF_IDENTIFY
};

enum ata_status_flags {
    ST_ERR = 1,
    ST_IDX = 2,
    ST_CORR = 4,
    ST_DRQ = 8,
    ST_DSC = 16,
    ST_DF = 32,
    ST_DRDY = 64,
    ST_BSY = 128
};

enum ata_error_flags {
    ERR_AMNF = 1,
    ERR_TKNONF = 2,
    ERR_ABRT = 4,
    ERR_MCR = 8,
    ERR_IDNF = 16,
    ERR_MC = 32,
    ERR_UNC = 64
};

#define SECTOR_SIZE		512

ide_disk::ide_disk()
{
    m_disk = NULL;
    m_irq_callback = NULL;
    m_buffer = new unsigned short[256];
    memset(m_buffer, 0, 256 * sizeof(unsigned short));

    // Killer Instinct geometry until an image is loaded
    m_num_cylinders = 419;
    m_default_heads = 13;
    m_default_sectors = 47;
    m_num_bytes_per_sector = SECTOR_SIZE;

    reset();
}

ide_disk::~ide_disk()
{
	BurnHDDClose(m_disk);
	m_disk = NULL;

	delete[] m_buffer;
}

void ide_disk::set_irq_callback(void (*irq)(int))
{
    m_irq_callback = irq;
}

void ide_disk::reset()
{
    // INITIALIZE DRIVE PARAMETERS may have changed these
    m_num_heads = m_default_heads;
    m_num_sectors = m_default_sectors;

    m_status = 0;
    m_error = 0;
    m_device_control = 0;
    m_features = 0;
    m_command = 0;
    m_sector_count = 0;
    m_sector_number = 0;
    m_cylinder_low = 0;
    m_cylinder_high = 0;
    m_drive_head = 0;
    m_transfer_operation = TRF_NONE;
    m_transfer_count = 0;
    m_buffer_pos = 0;
    m_last_buffer_lba = 0;
    build_identify_buffer();
}

void ide_disk::scan(INT32 nAction)
{
    if (nAction & ACB_DRIVER_DATA) {
        ScanVar(m_buffer, 256 * sizeof(unsigned short), "IDE Buffer");
        SCAN_VAR(m_buffer_pos);
        SCAN_VAR(m_last_buffer_lba);
        SCAN_VAR(m_transfer_count);
        SCAN_VAR(m_transfer_operation);
        SCAN_VAR(m_num_heads);
        SCAN_VAR(m_num_sectors);
        SCAN_VAR(m_device_control);
        SCAN_VAR(m_error);
        SCAN_VAR(m_sector_count);
        SCAN_VAR(m_sector_number);
        SCAN_VAR(m_cylinder_low);
        SCAN_VAR(m_cylinder_high);
        SCAN_VAR(m_drive_head);
        SCAN_VAR(m_status);
        SCAN_VAR(m_features);
        SCAN_VAR(m_command);
    }
}

void ide_disk::execute()
{
    switch (m_command) {
    case CMD_EXEC_DRIVE_DIAG: cmd_exec_drive_diag(); break;
    case CMD_INIT_DRIVE_PARAM: cmd_init_drive_params(); break;
    case CMD_READ_LONG: cmd_read_long(); break;
    case CMD_READ_LONG_NO_RETRY: cmd_read_long_wor(); break;
    case CMD_READ_SECTOR: cmd_read_sector(); break;
    case CMD_READ_SECTOR_NO_RETRY: cmd_read_sector_wor(); break;
    case CMD_WRITE_LONG: cmd_write_long(); break;
    case CMD_WRITE_LONG_NO_RETRY: cmd_write_long_wor(); break;
    case CMD_WRITE_SECTOR: cmd_write_sector(); break;
    case CMD_WRITE_SECTOR_NO_RETRY: cmd_write_sector_wor(); break;
    case CMD_IDENTIFY_DRIVE: cmd_indentify_drive(); break;
    default:
        ata_log("unimplemented command %x\n", m_command);
        break;
    }
}

void ide_disk::build_identify_buffer()
{
    memset(m_identify_buffer, 0, sizeof(m_identify_buffer));

    m_identify_buffer[0] = 0x0040;		// fixed drive
    m_identify_buffer[1] = m_num_cylinders;
    m_identify_buffer[3] = m_num_heads;
    m_identify_buffer[4] = m_num_sectors * SECTOR_SIZE;
    m_identify_buffer[5] = SECTOR_SIZE;
    m_identify_buffer[6] = m_num_sectors;

    // serial number and model, set up as MAME does: KI checks the model at word 27, KI2 at word 11
    for (int i = 10; i < 20; i++)
        m_identify_buffer[i] = ('0' << 8) | '0';
    for (int i = 27; i < 47; i++)
        m_identify_buffer[i] = (' ' << 8) | ' ';

    static const char model[9] = "ST9150AG";
    for (int i = 0; i < 4; i++) {
        m_identify_buffer[11 + i] = (model[i * 2] << 8) | model[i * 2 + 1];
        m_identify_buffer[27 + i] = (model[i * 2] << 8) | model[i * 2 + 1];
    }
}

unsigned ide_disk::chs_to_lba(int cylinder, int head, int sector)
{
    return ((cylinder * m_num_heads + head) * m_num_sectors) + sector - 1;
}

void ide_disk::chs_next_sector()
{
    if (m_drive_head & 0x40) {
        // LBA addressing: the task file holds a 28-bit sector number
        unsigned lba = lba_from_regs() + 1;
        m_sector_number = lba & 0xff;
        m_cylinder_low = (lba >> 8) & 0xff;
        m_cylinder_high = (lba >> 16) & 0xff;
        m_drive_head = (m_drive_head & 0xf0) | ((lba >> 24) & 0x0f);
        return;
    }

    // sectors count from 1, heads and cylinders from 0
    if (++m_sector_number > m_num_sectors) {
        m_sector_number = 1;
        int head = (m_drive_head & 0x0f) + 1;
        if (head >= m_num_heads) {
            head = 0;
            if (++m_cylinder_low >= 256) {
                m_cylinder_low = 0;
                m_cylinder_high = (m_cylinder_high + 1) & 0xff;
            }
        }
        m_drive_head = (m_drive_head & 0xf0) | head;
    }
}

unsigned ide_disk::lba_from_regs()
{
    if (m_drive_head & 0x40)
        return ((m_drive_head & 0x0f) << 24) | (m_cylinder_high << 16) | (m_cylinder_low << 8) | m_sector_number;

    return chs_to_lba(m_cylinder_low | (m_cylinder_high << 8), m_drive_head & 0x0f, m_sector_number);
}

inline bool ide_disk::is_drive_ready()
{
    if (m_status & ST_DRDY)
        return true;

    m_error |= ERR_ABRT;
    m_status |= ST_ERR;
    m_status &= ~ST_BSY;
    return false;
}

void ide_disk::raise_interrupt()
{
    if (~m_device_control & 2)
        if (m_irq_callback)
            m_irq_callback(1);
}

void ide_disk::clear_interrupt()
{
    if (m_irq_callback)
        m_irq_callback(0);
}


void ide_disk::write(unsigned offset, unsigned value)
{
    switch (offset) {
    case REG_COMMAND_WO:

        m_command = value;
        execute();

        break;

    case REG_DATA:
        if (m_status & ST_DRQ) {
            if (m_transfer_operation == TRF_SECTOR_WRITE) {
                m_buffer[m_buffer_pos++] = BURN_ENDIAN_SWAP_INT16(value);
                if (m_buffer_pos >= m_num_bytes_per_sector / 2)
                    update_transfer();
            }
        }
        break;

    case REG_FEATURES_WO:
        m_features = value;
        break;

    case REG_SECTOR_COUNT:
        m_sector_count = value & 0xff;
        break;

    case REG_SECTOR_NUMBER:
        m_sector_number = value & 0xff;
        break;

    case REG_CYLINDER_LOW:
        m_cylinder_low = value & 0xff;
        break;

    case REG_CYLINDER_HIGH:
        m_cylinder_high = value & 0xff;
        break;

    case REG_DRIVE_HEAD:
        m_drive_head = value & 0xff;
        break;
    }
}

void ide_disk::write_alternate(unsigned offset, unsigned value)
{
    ata_log("write_alt: %x = %x\n", offset, value);
    m_device_control = value;
}

unsigned ide_disk::read(unsigned offset)
{
    switch (offset) {
    case REG_STATUS_RO:
        clear_interrupt();
        return m_status;

    case REG_ERROR_RO:
        return m_error;

    case REG_DATA:
        if (m_status & ST_DRQ) {
            if ((m_transfer_operation == TRF_SECTOR_READ) ||
                (m_transfer_operation == TRF_IDENTIFY)) {
                unsigned data = BURN_ENDIAN_SWAP_INT16(m_buffer[m_buffer_pos]);
                m_buffer_pos++;

                if (m_buffer_pos >= m_num_bytes_per_sector / 2)
                    update_transfer();
                return data;
            }
        }
        return 0;
    case REG_SECTOR_COUNT:
        return m_sector_count;
    case REG_SECTOR_NUMBER:
        return m_sector_number;
    case REG_CYLINDER_LOW:
        return m_cylinder_low;
    case REG_CYLINDER_HIGH:
        return m_cylinder_high;
    case REG_DRIVE_HEAD:
        return m_drive_head;
    }

	// shouldn't happen
	return 0;
}

unsigned ide_disk::read_alternate(unsigned offset)
{
    switch (offset) {
    case REG_ALT_DRIVE_ADDRESS_RO:
    case REG_ALT_STATUS_RO:
        return m_status | 0x40 /* hack? */;
    }
    return 0;
}

// ---------------------------------------------------------------------------
// disk image

int ide_disk::load_hdd_image(int idx)
{
	BurnHDDClose(m_disk);
	m_disk = BurnHDDOpen(idx);
	if (m_disk == NULL)
		return 1;

	if (BurnHDDGetSectorSize(m_disk) != SECTOR_SIZE) {
		bprintf(PRINT_ERROR, _T("IDE: only 512 byte sectors are supported\n"));
		BurnHDDClose(m_disk);
		m_disk = NULL;
		return 1;
	}

	int cyls, heads, secs;
	if (BurnHDDGetGeometry(m_disk, &cyls, &heads, &secs) == 0) {
		m_default_heads = heads;
		m_default_sectors = secs;
		m_num_cylinders = cyls;
	} else {
		// raw dump: Killer Instinct geometry
		m_default_heads = 13;
		m_default_sectors = 47;
		m_num_cylinders = BurnHDDGetSectorCount(m_disk) / (13 * 47);
	}

	m_num_heads = m_default_heads;
	m_num_sectors = m_default_sectors;
	build_identify_buffer();

	return 0;
}

// ---------------------------------------------------------------------------
// PIO transfers

void ide_disk::setup_transfer(int mode)
{
    m_transfer_operation = mode;

    if (m_sector_count == 0)
        m_sector_count = 256;

    if (mode == TRF_IDENTIFY)
        m_sector_count = 1;

    m_status &= ~ST_ERR;
    start_sector(true);
}

// get the next sector of the transfer ready for the host
void ide_disk::start_sector(bool first)
{
    switch (m_transfer_operation) {
    case TRF_IDENTIFY:
#ifdef LSB_FIRST
        memcpy(m_buffer, m_identify_buffer, sizeof(m_identify_buffer));
#else
        for (int i = 0; i < 256; i++)
            m_buffer[i] = BURN_ENDIAN_SWAP_INT16(m_identify_buffer[i]);
#endif
        break;

    case TRF_SECTOR_READ:
        m_last_buffer_lba = lba_from_regs();
        BurnHDDRead(m_disk, m_last_buffer_lba, 1, m_buffer);
        chs_next_sector();
        break;

    case TRF_SECTOR_WRITE:
        m_last_buffer_lba = lba_from_regs();
        chs_next_sector();
        break;
    }

    m_buffer_pos = 0;
    m_status |= ST_DRQ;

    // no interrupt before the first sector of a write
    if (!(first && m_transfer_operation == TRF_SECTOR_WRITE))
        raise_interrupt();
}

// the host has read or written a whole sector
void ide_disk::update_transfer()
{
    if (m_transfer_operation == TRF_NONE)
        return;

    if (m_transfer_operation == TRF_SECTOR_WRITE)
        BurnHDDWrite(m_disk, m_last_buffer_lba, 1, m_buffer);

    if (--m_sector_count > 0) {
        start_sector(false);
        return;
    }

    m_sector_count = 0;
    m_status &= ~ST_DRQ;

    // a write ends with an interrupt once the last sector is on the disk
    if (m_transfer_operation == TRF_SECTOR_WRITE)
        raise_interrupt();

    m_transfer_operation = TRF_NONE;
}

// ========================================================================== //
//                               ATA COMMANDS                                 //
// ========================================================================== //

void ide_disk::cmd_exec_drive_diag()
{
    ata_log("exec_drive_dialog()\n");
}

void ide_disk::cmd_init_drive_params()
{
    ata_log("init_drive_params(logsec_per_logtrack=%d, logheads=%d)\n",
            m_sector_count, m_drive_head - 1);

    m_num_sectors = m_sector_count;
    m_num_heads = (m_drive_head & 0xF) + 1;

    m_status |= ST_DRDY;
    m_status &= ~ST_BSY;
    raise_interrupt();
}

void ide_disk::cmd_read_long()
{
    ata_log("read_long(cyl_lo=%d, cyl_hi=%d, head=%d, sector=%d) [sec_count=%d]\n",
            m_cylinder_low, m_cylinder_high, m_drive_head,
            m_sector_number, m_sector_count);
}

void ide_disk::cmd_read_long_wor()
{
    ata_log("read_long_wor(cyl_lo=%d, cyl_hi=%d, head=%d, sector=%d) [sec_count=%d]\n",
            m_cylinder_low, m_cylinder_high, m_drive_head,
            m_sector_number, m_sector_count);
}

void ide_disk::cmd_read_sector()
{
    ata_log("read_sector(lba=%x, sec_count=%d)\n", lba_from_regs(), m_sector_count);

    setup_transfer(TRF_SECTOR_READ);
}

void ide_disk::cmd_read_sector_wor()
{
    // same as READ SECTOR, the retry flag only matters for a real drive
    setup_transfer(TRF_SECTOR_READ);
}

void ide_disk::cmd_write_long()
{
    ata_log("write_long(cyl_lo=%d, cyl_hi=%d, head=%d, sector=%d) [sec_count=%d]\n",
            m_cylinder_low, m_cylinder_high, m_drive_head,
            m_sector_number, m_sector_count);
}

void ide_disk::cmd_write_long_wor()
{
    ata_log("write_long_wor(cyl_lo=%d, cyl_hi=%d, head=%d, sector=%d) [sec_count=%d]\n",
            m_cylinder_low, m_cylinder_high, m_drive_head,
            m_sector_number, m_sector_count);
}

void ide_disk::cmd_write_sector()
{
    ata_log("write_sector(lba=%x, sec_count=%d)\n", lba_from_regs(), m_sector_count);

    setup_transfer(TRF_SECTOR_WRITE);
}

void ide_disk::cmd_write_sector_wor()
{
    setup_transfer(TRF_SECTOR_WRITE);
}

void ide_disk::cmd_indentify_drive()
{
    ata_log("identify_drive()\n");
    setup_transfer(TRF_IDENTIFY);
}



}
