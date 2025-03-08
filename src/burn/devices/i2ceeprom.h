enum {
	I2C_NONE = 0,
	I2C_24C01,
	I2C_X24C01,
	I2C_X24C02,
	I2C_24C02,
	I2C_24C04,
	I2C_24C08,
	I2C_24C16,
	I2C_24C32,
	I2C_24C64,
	I2C_24C128,
	I2C_24C256
};

void i2c_init(UINT8 device, UINT8 sda, UINT8 scl, UINT8 sda_read);
void i2c_init(UINT8 device);
void i2c_exit();
void i2c_scan(INT32 nAction, INT32 *pnMin);
void i2c_reset();
bool i2c_read();
void i2c_write_bit(bool sda, bool scl);

UINT32 i2c_read_bus8(UINT32 address);
UINT32 i2c_read_bus16(UINT32 address);
void i2c_write_bus8(UINT32 address, UINT8 data);
void i2c_write_bus16(UINT32 data);
