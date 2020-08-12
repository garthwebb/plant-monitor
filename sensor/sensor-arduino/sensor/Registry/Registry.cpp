#include <EEPROM.h>
#include "Registry.h"

Registry::Registry() {
  _initSelfID();
}

bool Registry::hasCollectorID() {
  return EEPROM.read(EEPROM_ADDR_COLLECTOR_ID_FLAG) == FLAG_ID_SET;
}

uint32_t Registry::getSelfID() {
  if (_self_id == 0) {
    _self_id = _readIdFromEEPROM(EEPROM_ADDR_SELF_ID);
  }
  return _self_id;
}

uint32_t Registry::getCollectorID() {
  if (_collector_id == 0) {
    _collector_id = _readIdFromEEPROM(EEPROM_ADDR_COLLECTOR_ID);
  }
  return _collector_id;
}

void Registry::setCollectorID(uint32_t id) {
  _collector_id = id;
  _writeIdToEEPROM(EEPROM_ADDR_COLLECTOR_ID, id);
  _setFlagInEEPROM(EEPROM_ADDR_COLLECTOR_ID_FLAG);
}

void Registry::clearCollectorID() {
  _collector_id = 0;
  EEPROM.write(EEPROM_ADDR_COLLECTOR_ID_FLAG, FLAG_ID_CLEAR);
}

void Registry::_initSelfID() {
  if (!_hasSelfID()) {
    _setSelfID(__TIME_UNIX__);
  }
}

bool Registry::_hasSelfID() {
  return EEPROM.read(EEPROM_ADDR_SELF_ID_FLAG) == FLAG_ID_SET;
}

void Registry::_setSelfID(uint32_t id) {
  _writeIdToEEPROM(EEPROM_ADDR_SELF_ID, id);
  _setFlagInEEPROM(EEPROM_ADDR_SELF_ID_FLAG);
}

void Registry::_setFlagInEEPROM(uint8_t addr) {
  EEPROM.write(addr, FLAG_ID_SET);
}

void Registry::_writeIdToEEPROM(uint8_t addr, uint32_t value) {
  for (int i=0; i <= 3; i++) {
    EEPROM.write(addr+i, (value >> i*8) & 0xFF);
  }
}

uint32_t Registry::_readIdFromEEPROM(uint8_t addr) {
  uint32_t id_val = 0;
  uint32_t value;
  
  for (int i=0; i <= 3; i++) {
    value = EEPROM.read(addr+i);
    id_val += value << i*8;
  }

  return id_val;
}
