// config flags                                                                                                              
#define CONF_CRLF	(1<<0)
#define CONF_AUTOCR	(1<<1)
#define CONF_UNSHIFT_ON_SPACE (1<<2)
#define CONF_TRANSLATE	 (1<<3)
#define CONF_8BIT	 (1<<4)

#define EEP_CONFIGURED_LOCATION 0
#define EEP_CONFIGURED_SIZE 2
#define EEP_CONFIGURED_MAGIC 0x4545
#define EEP_BAUDDIV_LOCATION 2
#define EEP_BAUDDIV_SIZE 2

#define EEP_CONFFLAGS_LOCATION 4
#define EEP_CONFFLAGS_SIZE 1

// these will be used in the future for multiple and/or redefinable translation tables                                       
#define EEP_TABLES_START 128
#define EEP_TABLE_SIZE 64
#define FIGS_OFFSET 32

