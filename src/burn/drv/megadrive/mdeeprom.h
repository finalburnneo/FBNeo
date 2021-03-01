void EEPROM_init(UINT8 *data);
void EEPROM_init(UINT8 type, UINT8 cl, UINT8 i, UINT8 o, UINT8 *data);
void EEPROM_write16(UINT32 d);
void EEPROM_write8(UINT32 a, UINT8 d);
UINT32 EEPROM_read();
UINT32 EEPROM_read8(UINT32 a);
void EEPROM_scan();
