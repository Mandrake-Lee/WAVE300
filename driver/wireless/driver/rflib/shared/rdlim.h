#ifndef _MTLK_RDLIM_PROPR_H_
#define _MTLK_RDLIM_PROPR_H_

#define RDILIM_USE_2TX 0x02
#define RDILIM_USE_3TX 0x03
#define RDILIM_USE_2RX (0x02 << 4)
#define RDILIM_USE_3RX (0x03 << 4)

#define RDLIM_TX_ANT_NUM(x) ((x) & 0xF)
#define RDLIM_RX_ANT_NUM(x) ( ((x) >> 4) & 0xF )

struct hw_lim_table_entry {
  uint8 reg_dom;
  uint16 freq;
  uint8 spacing;
  int16 limit;
};

struct hw_lim_lookup_table_entry {
  uint16 vendor_id;
  uint16 device_id;
  uint8 hw_type;
  uint8 hw_revision;
  uint8 hw_ant_cfg;
  const struct hw_lim_table_entry *table;
};

typedef struct _aocs_default_penalty_t {
  uint16 freq;
  uint16 penalty;
} aocs_default_penalty_t;

extern const struct hw_lim_lookup_table_entry hw_lim_lookup_table[];
extern const aocs_default_penalty_t aocs_default_penalty[];

#endif /* _MTLK_RDLIM_PROPR_H_ */
