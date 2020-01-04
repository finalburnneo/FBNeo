// Killer Instinct hd image:
// Tag='GDDD'  Index=0  Length=34 bytes
// CYLS:419,HEADS:13,SECS:47,BPS:512.
/*#include <string>
#include <fstream>
#include <cstdio>
#include <cstring>*/
#include "ide.h"

#define DEBUG_ATA   0

#if DEBUG_ATA
# define ata_log(...)   printf("ata_device: " __VA_ARGS__); fflush(stdout)
#else
# define ata_log(...)
#endif

char* TCHARToANSI(const TCHAR* pszInString, char* pszOutString, INT32 nOutSize);
#define _TtoA(a)	TCHARToANSI(a, NULL, 0)

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

ide_disk::ide_disk()
{
    reset();
    m_buffer_pos = 0;
    m_buffer = new unsigned short[256];
    m_irq_callback = NULL;
}

ide_disk::~ide_disk()
{
	delete[] m_buffer;

	if (m_disk_image) {
		fclose(m_disk_image);
		m_disk_image = NULL;
	}
}

void ide_disk::set_irq_callback(void (*irq)(int))
{
    m_irq_callback = irq;
}

void ide_disk::reset()
{
    // Killer Instinct geometry
    // CYLS:419,HEADS:13,SECS:47,BPS:512.
    m_num_cylinders = 419;
    m_num_heads = 13;
    m_num_sectors = 47;
    m_num_bytes_per_sector = 512;

    m_status = 0;
    build_identify_buffer();
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

    // Killer Instinct 1
    m_identify_buffer[0] = 0x5354;
    m_identify_buffer[1] = 0x3931;
    m_identify_buffer[2] = 0x3530;
    m_identify_buffer[3] = 0x4147;
    m_identify_buffer[4] = 0x2020;
    m_identify_buffer[6] = m_num_sectors;

    // serial number
    for (int i = 0; i < 10; i++)
        m_identify_buffer[10 + i] = '0';

    strncpy((char *) &m_identify_buffer[27], "Generic IDE HD", 27);


    // KI I
    m_identify_buffer[10] = ('0' << 8) | '0';
    m_identify_buffer[11] = ('S' << 8) | 'T';
    m_identify_buffer[12] = ('9' << 8) | '1';
    m_identify_buffer[13] = ('5' << 8) | '0';
    m_identify_buffer[14] = ('A' << 8) | 'G';

    // KI II
    m_identify_buffer[27] = ('S' << 8) | 'T';
    m_identify_buffer[28] = ('9' << 8) | '1';
    m_identify_buffer[29] = ('5' << 8) | '0';
    m_identify_buffer[30] = ('A' << 8) | 'G';
    m_identify_buffer[31] = (' ' << 8) | ' ';
}

unsigned ide_disk::chs_to_lba(int cylinder, int head, int sector)
{
    return ((cylinder * m_num_heads + head) * m_num_sectors) + sector - 1;
}

void ide_disk::chs_next_sector()
{
    m_sector_number++;
    if (m_sector_number >= m_num_sectors) {
        m_sector_number = 0;
        m_drive_head++;
        if (m_drive_head >= m_num_heads) {
            m_drive_head = 0;
            m_cylinder_low++;
            if (m_cylinder_low >= 256) {
                m_cylinder_low = 0;
                m_cylinder_high++;
            }
        }
    }
}

unsigned ide_disk::lba_from_regs()
{
    return chs_to_lba(m_cylinder_low | (m_cylinder_high << 8), m_drive_head, m_sector_number);
}

inline bool ide_disk::is_drive_ready()
{
    if (m_status & ST_DRDY)
        return true;

    m_error |= ERR_ABRT;
    m_status |= ST_ERR;
    m_status &= ~ST_BSY;
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
    //ata_log("[%x] write: %x = %x\n", mips::g_mips->m_state.pc, offset, value);

    switch (offset) {
    case REG_COMMAND_WO:

        m_command = value;
        execute();

        break;

    case REG_DATA:
        if (m_status & ST_DRQ) {
            if (m_transfer_operation == TRF_SECTOR_WRITE) {
                m_buffer[m_buffer_pos++] = value;
                //ata_log("ata_write_data: %02x\n", value);
                if (m_buffer_pos >= m_num_bytes_per_sector / 2)
                    update_transfer();
            }
        }
        break;

    case REG_FEATURES_WO:
        break;

    case REG_SECTOR_COUNT:
        m_sector_count = value;
        break;

    case REG_SECTOR_NUMBER:
        m_sector_number = value;
        break;

    case REG_CYLINDER_LOW:
        m_cylinder_low = value;
        break;

    case REG_CYLINDER_HIGH:
        m_cylinder_high = value;
        break;

    case REG_DRIVE_HEAD:
        m_drive_head = value;
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
    //ata_log("ata_read: %x\n", offset);

    switch (offset) {
    case REG_STATUS_RO:
        //ata_log("ata_read_status: %x\n", offset);
        clear_interrupt();
        return m_status;

    case REG_ERROR_RO:
        return m_error;

    case REG_DATA:
        if (m_status & ST_DRQ) {
            if ((m_transfer_operation == TRF_SECTOR_READ) ||
                (m_transfer_operation == TRF_IDENTIFY)) {
                unsigned data = m_buffer[m_buffer_pos++];

                if (m_transfer_operation == TRF_IDENTIFY) {
                ata_log("ata_read_data: %02x\n", data);
                }
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
    //ata_log("read_alt: %x\n", offset);
    switch (offset) {
    case REG_ALT_DRIVE_ADDRESS_RO:
    case REG_ALT_STATUS_RO:
        return m_status | 0x40 /* hack? */;
    }
    return 0;
}

bool ide_disk::load_disk_image(const char *filename)
{
	char szFilePath[MAX_PATH];

	sprintf(szFilePath, "%s%s", _TtoA(szAppHDDPath), filename);

	m_disk_image = fopen(szFilePath, "r+b");
	if (!m_disk_image) {
		ata_log("disk image not found!\n");
		return false;
	}

	return true;
}

int ide_disk::load_hdd_image(int idx)
{
	// get setname (use parent if applicable)
	char setname[128];
	
	if (BurnDrvGetTextA(DRV_PARENT)) {
		strcpy(setname, BurnDrvGetTextA(DRV_PARENT));
	} else {
		strcpy(setname, BurnDrvGetTextA(DRV_NAME));
	}
	
	// get hdd name
	char *szHDDNameTmp = NULL;
	BurnDrvGetHDDName(&szHDDNameTmp, idx, 0);
	
	// make the path
	char path[256];
	sprintf(path, "%s/%s", setname, szHDDNameTmp);
	
	// null terminate
	path[strlen(path)] = '\0';
	
	if (!load_disk_image(path)) {
		return 1;
	}

	return 0;
}

void ide_disk::setup_transfer(int mode)
{
    m_transfer_operation = mode;

    m_buffer_pos = 0;
    if (m_sector_count == 0)
        m_sector_count = 256;

    if (mode == TRF_IDENTIFY)
        m_sector_count = 1;

    m_transfer_write_first = true;
    update_transfer();
    m_transfer_write_first = false;
}

void ide_disk::update_transfer()
{
    if (m_transfer_operation == TRF_NONE)
        return;

    if (m_sector_count < 0) {
        m_status &= ~ST_DRQ;
        m_transfer_operation = TRF_NONE;
        return;
    }

    unsigned lba = 0;
    switch (m_transfer_operation) {
    case TRF_IDENTIFY:
        memcpy(m_buffer, m_identify_buffer, sizeof(m_identify_buffer));
        break;

    case TRF_SECTOR_WRITE:
        if (!m_transfer_write_first)
            flush_write_transfer();

    case TRF_SECTOR_READ:
        lba = lba_from_regs() * m_num_bytes_per_sector;
        m_last_buffer_lba = lba;

        fseek(m_disk_image, lba, SEEK_SET);
        fread(m_buffer, m_num_bytes_per_sector, 1, m_disk_image);
        m_buffer_pos = 0;

        chs_next_sector();
        break;
    }


    m_sector_count--;
    m_status |= ST_DRQ;
    raise_interrupt();
}

void ide_disk::flush_write_transfer()
{
    ata_log("flush write buffer\n");

    fseek(m_disk_image, m_last_buffer_lba, SEEK_SET);
    fwrite(m_buffer, m_num_bytes_per_sector, 1, m_disk_image);
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
    ata_log("read_sector(cyl_lo=%d, cyl_hi=%d, head=%d, sector=%d, sec_count=%d) [%0x]\n",
            m_cylinder_low, m_cylinder_high, m_drive_head,
            m_sector_number, m_sector_count,
            chs_to_lba(m_cylinder_low | (m_cylinder_high << 8), m_drive_head, m_sector_number));

    setup_transfer(TRF_SECTOR_READ);
}

void ide_disk::cmd_read_sector_wor()
{
    ata_log("read_sector_wor(cyl_lo=%d, cyl_hi=%d, head=%d, sector=%d, sec_count=%d) [%0x]\n",
            m_cylinder_low, m_cylinder_high, m_drive_head,
            m_sector_number, m_sector_count,
            chs_to_lba(m_cylinder_low | (m_cylinder_high << 8), m_drive_head, m_sector_number));
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
    ata_log("write_sector(cyl_lo=%d, cyl_hi=%d, head=%d, sector=%d, sec_count=%d) [%0x]\n",
            m_cylinder_low, m_cylinder_high, m_drive_head,
            m_sector_number, m_sector_count,
            chs_to_lba(m_cylinder_low | (m_cylinder_high << 8), m_drive_head, m_sector_number));

    setup_transfer(TRF_SECTOR_WRITE);
}

void ide_disk::cmd_write_sector_wor()
{
    ata_log("write_sector_wor(cyl_lo=%d, cyl_hi=%d, head=%d, sector=%d, sec_count=%d) [%0x]\n",
            m_cylinder_low, m_cylinder_high, m_drive_head,
            m_sector_number, m_sector_count,
            chs_to_lba(m_cylinder_low | (m_cylinder_high << 8), m_drive_head, m_sector_number));
}

void ide_disk::cmd_indentify_drive()
{
    ata_log("identify_drive()\n");
    setup_transfer(TRF_IDENTIFY);
}



}
