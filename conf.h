// config flags                                                                                                              
#define CONF_CRLF 0x01
#define CONF_AUTOCR 0x02
#define CONF_UNSHIFT_ON_SPACE 0x04
#define CONF_TRANSLATE 0x08

#define EEP_CONFIGURED_LOCATION 0
#define EEP_CONFIGURED_SIZE 2
#define EEP_CONFIGURED_MAGIC 0x4545
#define EEP_BAUDDIV_LOCATION 2
#define EEP_BAUDDIV_SIZE 2

#define EEP_CONFFLAGS_LOCATION 4
#define EEP_CONFFLAGS_SIZE 1

// these will be used in the future for multiple and/or redefinable translation tables                                       
#define EEP_TABLE1_LTRS_LOCATION 128
#define EEP_TABLE1_FIGS_LOCATION 160
#define EEP_TABLE_SIZE 32

