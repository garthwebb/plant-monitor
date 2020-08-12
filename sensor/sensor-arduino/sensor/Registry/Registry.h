
// Elaborate stuff to get a time in seconds at compile time (see https://stackoverflow.com/a/44271643)
#ifndef REGISTRY_H_
#define REGISTRY_H_

// extracts 1..4 characters from a string and interprets it as a decimal value
#define CONV_STR2DEC_1(str, i)  (str[i]>'0'?str[i]-'0':0)
#define CONV_STR2DEC_2(str, i)  (CONV_STR2DEC_1(str, i)*10 + str[i+1]-'0')
#define CONV_STR2DEC_3(str, i)  (CONV_STR2DEC_2(str, i)*10 + str[i+2]-'0')
#define CONV_STR2DEC_4(str, i)  (CONV_STR2DEC_3(str, i)*10 + str[i+3]-'0')

// Some definitions for calculation
#define SEC_PER_MIN             60UL
#define SEC_PER_HOUR            3600UL
#define SEC_PER_DAY             86400UL
#define SEC_PER_YEAR            (SEC_PER_DAY*365)
#define UNIX_START_YEAR         1970UL

// Custom "glue logic" to convert the month name to a usable number
#define GET_MONTH(str, i)      (str[i]=='J' && str[i+1]=='a' && str[i+2]=='n' ? 1 :     \
                                str[i]=='F' && str[i+1]=='e' && str[i+2]=='b' ? 2 :     \
                                str[i]=='M' && str[i+1]=='a' && str[i+2]=='r' ? 3 :     \
                                str[i]=='A' && str[i+1]=='p' && str[i+2]=='r' ? 4 :     \
                                str[i]=='M' && str[i+1]=='a' && str[i+2]=='y' ? 5 :     \
                                str[i]=='J' && str[i+1]=='u' && str[i+2]=='n' ? 6 :     \
                                str[i]=='J' && str[i+1]=='u' && str[i+2]=='l' ? 7 :     \
                                str[i]=='A' && str[i+1]=='u' && str[i+2]=='g' ? 8 :     \
                                str[i]=='S' && str[i+1]=='e' && str[i+2]=='p' ? 9 :     \
                                str[i]=='O' && str[i+1]=='c' && str[i+2]=='t' ? 10 :    \
                                str[i]=='N' && str[i+1]=='o' && str[i+2]=='v' ? 11 :    \
                                str[i]=='D' && str[i+1]=='e' && str[i+2]=='c' ? 12 : 0)

#define GET_MONTH2DAYS(month)  ((month == 1 ? 0 : 31 +                      \
                                (month == 2 ? 0 : 28 +                      \
                                (month == 3 ? 0 : 31 +                      \
                                (month == 4 ? 0 : 30 +                      \
                                (month == 5 ? 0 : 31 +                      \
                                (month == 6 ? 0 : 30 +                      \
                                (month == 7 ? 0 : 31 +                      \
                                (month == 8 ? 0 : 31 +                      \
                                (month == 9 ? 0 : 30 +                      \
                                (month == 10 ? 0 : 31 +                     \
                                (month == 11 ? 0 : 30))))))))))))           \


#define GET_LEAP_DAYS           ((__TIME_YEARS__-1968)/4 - (__TIME_MONTH__ <=2 ? 1 : 0))



#define __TIME_SECONDS__        CONV_STR2DEC_2(__TIME__, 6)
#define __TIME_MINUTES__        CONV_STR2DEC_2(__TIME__, 3)
#define __TIME_HOURS__          CONV_STR2DEC_2(__TIME__, 0)
#define __TIME_DAYS__           CONV_STR2DEC_2(__DATE__, 4)
#define __TIME_MONTH__          GET_MONTH(__DATE__, 0)
#define __TIME_YEARS__          CONV_STR2DEC_4(__DATE__, 7)

#define __TIME_UNIX__         ((__TIME_YEARS__-UNIX_START_YEAR)*SEC_PER_YEAR+       \
                                GET_LEAP_DAYS*SEC_PER_DAY+                          \
                                GET_MONTH2DAYS(__TIME_MONTH__)*SEC_PER_DAY+         \
                                __TIME_DAYS__*SEC_PER_DAY-SEC_PER_DAY+              \
                                __TIME_HOURS__*SEC_PER_HOUR+                        \
                                __TIME_MINUTES__*SEC_PER_MIN+                       \
                                __TIME_SECONDS__)


// Whether we've set our self ID yet
#define EEPROM_ADDR_SELF_ID_FLAG 0x00
// Whether we've identified our nearest collector
#define EEPROM_ADDR_COLLECTOR_ID_FLAG 0x01
// Our ID (should be assigned once)
#define EEPROM_ADDR_SELF_ID 0x02
// Nearest collector (should be set once, then updated if too many retries)
#define EEPROM_ADDR_COLLECTOR_ID 0x06

// A value indicating whether an ID has been set.  Arbitrary. 10-4 good buddy!
#define FLAG_ID_SET 0xa4
#define FLAG_ID_CLEAR 0xff

class Registry {
  public:
    Registry();
    bool hasCollectorID(void);
    uint32_t getSelfID(void);
    uint32_t getCollectorID(void);
    void setCollectorID(uint32_t id);
    void clearCollectorID(void);
  private:
    void _initSelfID(void);
    bool _hasSelfID(void);
    void _setSelfID(uint32_t id);
    void _setFlagInEEPROM(uint8_t addr);
    void _writeIdToEEPROM(uint8_t addr, uint32_t value);
    uint32_t Registry::_readIdFromEEPROM(uint8_t addr);
    uint32_t _self_id = 0;
    uint32_t _collector_id = 0;
};

#endif /* REGISTRY_H_ */
