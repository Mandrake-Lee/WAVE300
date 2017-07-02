/*
 * $Id: mtlk_channels.c 12599 2012-02-07 11:46:18Z bejs $
 *
 * Copyright (c) 2006-2007 Metalink Broadband (Israel)
 *
 * Written by: Roman Sikorskyy
 *
 */

#include "mtlkinc.h"
#include "mtlk_channels_propr.h"
#include "mtlk_scan_propr.h"
#include "channels.h"
#include "mtlkaux.h"
#include "mtlkerr.h"
#include "mtlk_osal.h"
#include "scan.h"
#include "eeprom.h"
#include "mtlk_eeprom.h"
#include "mtlkmib.h"
#include "mtlk_core_iface.h"
#include "mtlk_coreui.h"
#include "core.h"
#include "rdlim.h"

#define LOG_LOCAL_GID   GID_CHANNELS
#define LOG_LOCAL_FID   1

#ifndef INT16_MAX
#define INT16_MAX 0x7fff
#endif

struct reg_domain_list_t
{
  uint8 reg_domain;
  uint8 num_reg_class_sets;
  const struct reg_domain_t *dom;
  char *name;
  char *alias;
};

struct hw_tx_limit
{
  int16 tx_lim;
  uint16 freq;
  uint8 spectrum;
};

struct hw_rc_tx_limit
{
  uint8 reg_class;
  uint16 num_freq;
  struct hw_tx_limit *tx_lim;
};

struct hw_reg_tx_limit
{
  uint8 reg_domain;
  uint8 num_classes;
  struct hw_rc_tx_limit *tx_lim;
  struct hw_reg_tx_limit *next;
};

struct channel_tx_limit
{
  uint8 channel;
  uint8 mitigation;
  uint16 tx_lim;
};

struct reg_class_tx_limit
{
//  int16 tx_lim;
  uint8 reg_class;
//  uint8 mitigation;
  uint8 num_ch;
  uint8 spectrum;
//  uint8 *channels;
  struct channel_tx_limit *channels;
};

struct reg_domain_tx_limit
{
  uint8 num_classes;
  struct reg_class_tx_limit *tx_lim;
  struct reg_domain_tx_limit *next;
};

struct reg_tx_limit
{
  uint8 reg_domain;
  struct reg_domain_tx_limit *dom_lim;
  struct reg_tx_limit *next;
};

struct antenna_gain
{
  uint16 freq;
  int16  gain;
};

struct country_regulatory_t
{
  char country[MTLK_CHNLS_COUNTRY_BUFSIZE];
  uint8 domain;
};

#define CHANNELS(x) sizeof((x)) / sizeof((x)[0]), (x)
#define REG_CLASSES(x) sizeof((x)) / sizeof((x)[0]), (x)
#define REG_CLASS_SETS(x) sizeof((x)) / sizeof((x)[0]), (x)

static const uint8 ch_11a_c1[] = {36, 40, 44, 48};
static const uint8 ch_11a_c2[] = {52, 56, 60, 64};
static const uint8 ch_11a_c3[] = {149, 153, 157, 161};
static const uint8 ch_11a_c4[] = {100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140};
static const uint8 ch_11a_c5[] = {165};
static const uint8 ch_11a_c6[] = {100, 104, 108, 112, 116, 136, 140};
static const uint8 ch_11a_c7[] = {100, 104, 108, 112, 116, 128, 132, 136, 140};
static const uint8 ch_11a_c8[] = {120, 124};

/* A - channels */
static const struct reg_class_t reg_class_FCC_11a[] =
{
  {1, 50000, 20, 17, SCAN_ACTIVE,  0, 0, CHANNELS(ch_11a_c1),  0,  0,  0},
  {2, 50000, 20, 24, SCAN_PASSIVE, 1, 0, CHANNELS(ch_11a_c2), 60, 24, 30},
  {3, 50000, 20, 29, SCAN_ACTIVE,  0, 0, CHANNELS(ch_11a_c3),  0,  0,  0},
  {4, 50000, 20, 24, SCAN_PASSIVE, 1, 3, CHANNELS(ch_11a_c4), 60, 24, 30},
  {5, 50000, 20, 30, SCAN_ACTIVE,  0, 0, CHANNELS(ch_11a_c5),  0,  0,  0},
};

static const struct reg_class_t reg_class_DOC_11a[] =
{
  {1, 50000, 20, 17, SCAN_ACTIVE,  0, 0, CHANNELS(ch_11a_c1),  0,  0,  0},
  {2, 50000, 20, 24, SCAN_PASSIVE, 1, 0, CHANNELS(ch_11a_c2), 60, 24, 30},
  {3, 50000, 20, 29, SCAN_ACTIVE,  0, 0, CHANNELS(ch_11a_c3),  0,  0,  0},
  {4, 50000, 20, 24, SCAN_PASSIVE, 1, 3, CHANNELS(ch_11a_c6), 60, 24, 30},
};

static const struct reg_class_t reg_class_ETSI_11a[] =
{
  {1, 50000, 20, 22, SCAN_ACTIVE,  0, 0, CHANNELS(ch_11a_c1),  0,  0,  0},
  {2, 50000, 20, 22, SCAN_PASSIVE, 1, 0, CHANNELS(ch_11a_c2), 60, 24, 30},
  {3, 50000, 20, 28, SCAN_PASSIVE, 1, 3, CHANNELS(ch_11a_c4), 60, 24, 30},
};

static const struct reg_class_t reg_class_GERMANY_11a[] =
{
  {1, 50000, 20, 22, SCAN_ACTIVE,  0, 0, CHANNELS(ch_11a_c1),  0,   0,  0},
  {2, 50000, 20, 22, SCAN_PASSIVE, 1, 0, CHANNELS(ch_11a_c2), 60,  24, 30},
  {3, 50000, 20, 28, SCAN_PASSIVE, 1, 3, CHANNELS(ch_11a_c7), 60,  24, 30},
  {4, 50000, 20, 28, SCAN_PASSIVE, 1, 3, CHANNELS(ch_11a_c8), 600, 24, 30},
};

static const struct reg_class_t reg_class_MKK_11a[] =
{
  {1, 50000, 20, 22, SCAN_ACTIVE,  0, 0, CHANNELS(ch_11a_c1),  0,  0,  0},
  {2, 50000, 20, 22, SCAN_PASSIVE, 1, 0, CHANNELS(ch_11a_c2), 60, 24, 30},
  {3, 50000, 20, 28, SCAN_PASSIVE, 1, 3, CHANNELS(ch_11a_c4), 60, 24, 30},
};

static const struct reg_class_t reg_class_FRANCE_11a[] =
{
  {1, 50000, 20, 22, SCAN_ACTIVE,  0, 0, CHANNELS(ch_11a_c1),  0,  0,  0},
  {2, 50000, 20, 22, SCAN_PASSIVE, 1, 0, CHANNELS(ch_11a_c2), 60, 24, 30},
  {3, 50000, 20, 28, SCAN_PASSIVE, 1, 3, CHANNELS(ch_11a_c4), 60, 24, 30},
};

static const struct reg_class_t reg_class_APAC_11a[] =
{
  {1, 50000, 20, 23, SCAN_ACTIVE,  0, 0, CHANNELS(ch_11a_c1),  0,  0,  0},
  {2, 50000, 20, 23, SCAN_PASSIVE, 1, 0, CHANNELS(ch_11a_c2), 60, 24, 30},
  {3, 50000, 20, 30, SCAN_ACTIVE,  0, 0, CHANNELS(ch_11a_c3),  0,  0,  0},
  {4, 50000, 20, 30, SCAN_ACTIVE,  0, 0, CHANNELS(ch_11a_c5),  0,  0,  0},
};

static const struct reg_class_t reg_class_UAE_11a[] =
{
  {1, 50000, 20, 23, SCAN_ACTIVE,  0, 0, CHANNELS(ch_11a_c1),  0,  0,  0},
  {2, 50000, 20, 23, SCAN_PASSIVE, 1, 0, CHANNELS(ch_11a_c2), 60, 24, 30},
  {3, 50000, 20, 30, SCAN_PASSIVE, 1, 3, CHANNELS(ch_11a_c3), 60, 24, 30},
  {4, 50000, 20, 30, SCAN_PASSIVE, 1, 3, CHANNELS(ch_11a_c4), 60, 24, 30},
  {5, 50000, 20, 30, SCAN_PASSIVE, 1, 3, CHANNELS(ch_11a_c5), 60, 24, 30},
};

/* BG - channels (no classes) */
static const uint8 ch_11bg_c1[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
static const uint8 ch_11bg_c2[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13};
static const uint8 ch_11bg_c3[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14};

/*TODO- what about SmRequired and timeouts for B/G ?*/
static const struct reg_class_t reg_class_FCC_11bg[] =
{
  {0, 24070, 20, 30, SCAN_PASSIVE, 0, 0, CHANNELS(ch_11bg_c1), 0, 0, 0}
};

static const struct reg_class_t reg_class_ETSI_11bg[] =
{
  {0, 24070, 20, 20, SCAN_PASSIVE, 0, 0, CHANNELS(ch_11bg_c2), 0, 0, 0}
};

static const struct reg_class_t reg_class_GERMANY_11bg[] =
{
  {0, 24070, 20, 20, SCAN_PASSIVE, 0, 0, CHANNELS(ch_11bg_c2), 0, 0, 0}
};

static const struct reg_class_t reg_class_MKK_11bg[] =
{
  {0, 24070, 20, 20, SCAN_PASSIVE, 0, 0, CHANNELS(ch_11bg_c3), 0, 0, 0}
};

static const struct reg_class_t reg_class_APAC_11bg[] =
{
  {0, 24070, 20, 23, SCAN_PASSIVE, 0, 0, CHANNELS(ch_11bg_c2), 0, 0, 0}
};

static const struct reg_class_t reg_class_UAE_11bg[] =
{
  {0, 24070, 20, 20, SCAN_PASSIVE, 0, 0, CHANNELS(ch_11bg_c2), 0, 0, 0}
};

/* N - channels */
static const uint8 ch_11n_c22[] = {36, 44};
static const uint8 ch_11n_c23[] = {52, 60};
static const uint8 ch_11n_c24[] = {100, 108, 116, 124, 132};
static const uint8 ch_11n_c25[] = {149, 157};
static const uint8 ch_11n_c26[] = {100, 108, 116};
static const uint8 ch_11n_c32[] = {1, 2, 3, 4, 5, 6, 7};
static const uint8 ch_11n_c33[] = {5, 6, 7, 8, 9, 10, 11};
static const uint8 ch_11n_c34[] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
static const uint8 ch_11n_c35[] = {5, 6, 7, 8, 9, 10, 11, 12, 13};
static const uint8 ch_11n_c36[] = {100, 108};

static const uint8 ch_11n_c37[] = {100, 108, 132};
static const uint8 ch_11n_c38[] = {116, 124};

static const struct reg_class_t reg_class_FCC_11n_52[] =
{
  { 1, 50000, 20, 17, SCAN_ACTIVE,  0, 0, CHANNELS(ch_11a_c1),   0,  0,  0},
  { 2, 50000, 20, 24, SCAN_PASSIVE, 1, 0, CHANNELS(ch_11a_c2),  60, 24, 30},
  { 3, 50000, 20, 29, SCAN_ACTIVE,  0, 0, CHANNELS(ch_11a_c3),   0,  0,  0},
  { 4, 50000, 20, 24, SCAN_PASSIVE, 1, 3, CHANNELS(ch_11a_c4),  60, 24, 30},
  { 5, 50000, 20, 30, SCAN_ACTIVE,  0, 0, CHANNELS(ch_11a_c5),   0,  0,  0},
  {22, 50000, 40, 17, SCAN_ACTIVE,  0, 0, CHANNELS(ch_11n_c22),  0,  0,  0},
  {23, 50000, 40, 24, SCAN_PASSIVE, 1, 0, CHANNELS(ch_11n_c23), 60, 24, 30},
  {24, 50000, 40, 24, SCAN_PASSIVE, 1, 3, CHANNELS(ch_11n_c24), 60, 24, 30},
  {25, 50000, 40, 29, SCAN_ACTIVE,  0, 0, CHANNELS(ch_11n_c25),  0,  0,  0}
};

static const struct reg_class_t reg_class_DOC_11n_52[] =
{
  { 1, 50000, 20, 17, SCAN_ACTIVE,  0, 0, CHANNELS(ch_11a_c1),   0,  0,  0},
  { 2, 50000, 20, 24, SCAN_PASSIVE, 1, 0, CHANNELS(ch_11a_c2),  60, 24, 30},
  { 3, 50000, 20, 29, SCAN_ACTIVE,  0, 0, CHANNELS(ch_11a_c3),   0,  0,  0},
  { 4, 50000, 20, 24, SCAN_PASSIVE, 1, 3, CHANNELS(ch_11a_c6),  60, 24, 30},
  {22, 50000, 40, 17, SCAN_ACTIVE,  0, 0, CHANNELS(ch_11n_c22),  0,  0,  0},
  {23, 50000, 40, 24, SCAN_PASSIVE, 1, 0, CHANNELS(ch_11n_c23), 60, 24, 30},
  {24, 50000, 40, 24, SCAN_PASSIVE, 1, 3, CHANNELS(ch_11n_c26), 60, 24, 30},
  {25, 50000, 40, 29, SCAN_ACTIVE,  0, 0, CHANNELS(ch_11n_c25),  0,  0,  0}
};

static const struct reg_class_t reg_class_FCC_11n_24[] =
{
  {11, 24070, 20, 30, SCAN_ACTIVE, 0, 0, CHANNELS(ch_11bg_c1), 0, 0, 0},
  {32, 24070, 40, 30, SCAN_ACTIVE, 0, 0, CHANNELS(ch_11n_c32), 0, 0, 0}
};

static const struct reg_class_t reg_class_ETSI_11n_52[] =
{
  {1, 50000, 20, 22, SCAN_ACTIVE,  0, 0, CHANNELS(ch_11a_c1),   0,  0,  0},
  {2, 50000, 20, 22, SCAN_PASSIVE, 1, 0, CHANNELS(ch_11a_c2),  60, 24, 30},
  {3, 50000, 20, 28, SCAN_PASSIVE, 1, 3, CHANNELS(ch_11a_c4),  60, 24, 30},
  {5, 50000, 40, 22, SCAN_ACTIVE,  0, 0, CHANNELS(ch_11n_c22),  0,  0,  0},
  {6, 50000, 40, 22, SCAN_PASSIVE, 1, 0, CHANNELS(ch_11n_c23), 60, 24, 30},
  {7, 50000, 40, 28, SCAN_PASSIVE, 1, 3, CHANNELS(ch_11n_c24), 60, 24, 30}
};

static const struct reg_class_t reg_class_GERMANY_11n_52[] =
{
  {1, 50000, 20, 22, SCAN_ACTIVE,  0, 0, CHANNELS(ch_11a_c1),   0,   0,  0},
  {2, 50000, 20, 22, SCAN_PASSIVE, 1, 0, CHANNELS(ch_11a_c2),  60,  24, 30},
  {3, 50000, 20, 28, SCAN_PASSIVE, 1, 3, CHANNELS(ch_11a_c7),  60,  24, 30},
  {4, 50000, 20, 28, SCAN_PASSIVE, 1, 3, CHANNELS(ch_11a_c8),  600, 24, 30},
  {5, 50000, 40, 22, SCAN_ACTIVE,  0, 0, CHANNELS(ch_11n_c22),  0,   0,  0},
  {6, 50000, 40, 22, SCAN_PASSIVE, 1, 0, CHANNELS(ch_11n_c23), 60,  24, 30},
  {7, 50000, 40, 28, SCAN_PASSIVE, 1, 3, CHANNELS(ch_11n_c37), 60,  24, 30},
  {8, 50000, 40, 28, SCAN_PASSIVE, 1, 3, CHANNELS(ch_11n_c38), 600, 24, 30}
};

static const struct reg_class_t reg_class_FRANCE_11n_52[] =
{
  {1, 50000, 20, 22, SCAN_ACTIVE,  0, 0, CHANNELS(ch_11a_c1),   0,  0,  0},
  {2, 50000, 20, 22, SCAN_PASSIVE, 1, 0, CHANNELS(ch_11a_c2),  60, 24, 30},
  {3, 50000, 20, 28, SCAN_PASSIVE, 1, 3, CHANNELS(ch_11a_c4),  60, 24, 30},
  {5, 50000, 40, 22, SCAN_ACTIVE,  0, 0, CHANNELS(ch_11n_c22),  0,  0,  0},
  {6, 50000, 40, 22, SCAN_PASSIVE, 1, 0, CHANNELS(ch_11n_c23), 60, 24, 30},
  {7, 50000, 40, 28, SCAN_PASSIVE, 1, 3, CHANNELS(ch_11n_c24), 60, 24, 30},
};

static const struct reg_class_t reg_class_ETSI_11n_24[] =
{
  {11, 24070, 20, 20, SCAN_ACTIVE, 0, 0, CHANNELS(ch_11bg_c2), 0, 0, 0},
  {11, 24070, 40, 20, SCAN_ACTIVE, 0, 0, CHANNELS(ch_11n_c34), 0, 0, 0}
};

static const struct reg_class_t reg_class_MKK_11n_52[] =
{
  {1, 50000, 20, 22, SCAN_ACTIVE,  0, 0, CHANNELS(ch_11a_c1),   0,  0,  0},
  {2, 50000, 20, 22, SCAN_PASSIVE, 1, 0, CHANNELS(ch_11a_c2),  60, 24, 30},
  {3, 50000, 20, 28, SCAN_PASSIVE, 1, 3, CHANNELS(ch_11a_c4),  60, 24, 30},
  {5, 50000, 40, 22, SCAN_ACTIVE,  0, 0, CHANNELS(ch_11n_c22),  0,  0,  0},
  {6, 50000, 40, 22, SCAN_PASSIVE, 1, 0, CHANNELS(ch_11n_c23), 60, 24, 30},
  {7, 50000, 40, 28, SCAN_PASSIVE, 1, 3, CHANNELS(ch_11n_c24), 60, 24, 30}
};

static const struct reg_class_t reg_class_MKK_11n_24[] =
{
  {11, 24070, 20, 23, SCAN_ACTIVE, 0, 0, CHANNELS(ch_11bg_c3), 0, 0, 0},
  {11, 24070, 40, 23, SCAN_ACTIVE, 0, 0, CHANNELS(ch_11n_c34), 0, 0, 0}
};

static const struct reg_class_t reg_class_APAC_11n_52[] =
{
  {1, 50000, 20, 23, SCAN_ACTIVE,  0, 0, CHANNELS(ch_11a_c1),   0,   0,  0},
  {2, 50000, 20, 23, SCAN_PASSIVE, 1, 0, CHANNELS(ch_11a_c2),  60,  24, 30},
  {3, 50000, 20, 30, SCAN_ACTIVE,  0, 0, CHANNELS(ch_11a_c3),   0,   0,  0},
  {5, 50000, 20, 30, SCAN_ACTIVE,  0, 0, CHANNELS(ch_11a_c5),   0,   0,  0},
  {5, 50000, 40, 23, SCAN_ACTIVE,  0, 0, CHANNELS(ch_11n_c22),  0,   0,  0},
  {6, 50000, 40, 23, SCAN_PASSIVE, 1, 0, CHANNELS(ch_11n_c23), 60,  24, 30},
  {25, 50000, 40, 30, SCAN_ACTIVE,  0, 0, CHANNELS(ch_11n_c25),  0,  0,  0},	
};

static const struct reg_class_t reg_class_APAC_11n_24[] =
{
  {11, 24070, 20, 23, SCAN_ACTIVE, 0, 0, CHANNELS(ch_11bg_c2), 0, 0, 0},
  {32, 24070, 40, 23, SCAN_ACTIVE, 0, 0, CHANNELS(ch_11n_c34), 0, 0, 0},
};

static const struct reg_class_t reg_class_UAE_11n_52[] =
{
  { 1, 50000, 20, 23, SCAN_ACTIVE,  0, 0, CHANNELS(ch_11a_c1),   0,  0,  0},
  { 2, 50000, 20, 23, SCAN_PASSIVE, 1, 0, CHANNELS(ch_11a_c2),  60, 24, 30},
  { 3, 50000, 20, 30, SCAN_PASSIVE, 1, 3, CHANNELS(ch_11a_c3),  60, 24, 30},
  { 4, 50000, 20, 30, SCAN_PASSIVE, 1, 3, CHANNELS(ch_11a_c4),  60, 24, 30},
  { 5, 50000, 20, 30, SCAN_PASSIVE, 1, 3, CHANNELS(ch_11a_c5),  60, 24, 30},
  {22, 50000, 40, 23, SCAN_ACTIVE,  0, 0, CHANNELS(ch_11n_c22),  0,  0,  0},
  {23, 50000, 40, 23, SCAN_PASSIVE, 1, 0, CHANNELS(ch_11n_c23), 60, 24, 30},
  {24, 50000, 40, 30, SCAN_PASSIVE, 1, 3, CHANNELS(ch_11n_c24), 60, 24, 30},
  {25, 50000, 40, 30, SCAN_PASSIVE, 1, 3, CHANNELS(ch_11n_c25), 60, 24, 30}
};

static const struct reg_class_t reg_class_UAE_11n_24[] =
{
  {11, 24070, 20, 20, SCAN_ACTIVE, 0, 0, CHANNELS(ch_11bg_c2), 0, 0, 0},
  {32, 24070, 40, 20, SCAN_ACTIVE, 0, 0, CHANNELS(ch_11n_c34), 0, 0, 0}
};

/* Regulatory domains */
static const struct reg_domain_t reg_domain_FCC[] =
{
  {REG_CLASSES(reg_class_FCC_11a)},
  {REG_CLASSES(reg_class_FCC_11bg)},
  {REG_CLASSES(reg_class_FCC_11n_52)},
  {REG_CLASSES(reg_class_FCC_11n_24)},
};

static const struct reg_domain_t reg_domain_DOC[] =
{
  {REG_CLASSES(reg_class_DOC_11a)},
  {REG_CLASSES(reg_class_FCC_11bg)},
  {REG_CLASSES(reg_class_DOC_11n_52)},
  {REG_CLASSES(reg_class_FCC_11n_24)},
};

static const struct reg_domain_t reg_domain_ETSI[] =
{
  {REG_CLASSES(reg_class_ETSI_11a)},
  {REG_CLASSES(reg_class_ETSI_11bg)},
  {REG_CLASSES(reg_class_ETSI_11n_52)},
  {REG_CLASSES(reg_class_ETSI_11n_24)},
};

static const struct reg_domain_t reg_domain_GERMANY[] =
{
  {REG_CLASSES(reg_class_GERMANY_11a)},
  {REG_CLASSES(reg_class_ETSI_11bg)},
  {REG_CLASSES(reg_class_GERMANY_11n_52)},
  {REG_CLASSES(reg_class_ETSI_11n_24)},
};

static const struct reg_domain_t reg_domain_MKK[] =
{
  {REG_CLASSES(reg_class_MKK_11a)},
  {REG_CLASSES(reg_class_MKK_11bg)},
  {REG_CLASSES(reg_class_MKK_11n_52)},
  {REG_CLASSES(reg_class_MKK_11n_24)},
};

static const struct reg_domain_t reg_domain_FRANCE[] =
{ 
  {REG_CLASSES(reg_class_FRANCE_11a)},
  {REG_CLASSES(reg_class_ETSI_11bg)},
  {REG_CLASSES(reg_class_FRANCE_11n_52)},
  {REG_CLASSES(reg_class_ETSI_11n_24)},
};

static const struct reg_domain_t reg_domain_APAC[] =
{
  {REG_CLASSES(reg_class_APAC_11a)},
  {REG_CLASSES(reg_class_APAC_11bg)},
  {REG_CLASSES(reg_class_APAC_11n_52)},
  {REG_CLASSES(reg_class_APAC_11n_24)},
};

static const struct reg_domain_t reg_domain_UAE[] =
{
  {REG_CLASSES(reg_class_UAE_11a)},
  {REG_CLASSES(reg_class_UAE_11bg)},
  {REG_CLASSES(reg_class_UAE_11n_52)},
  {REG_CLASSES(reg_class_UAE_11n_24)},
};

/* All regulatory domains table. */
static const struct reg_domain_list_t all_domains[] =
{
  {MIB_REG_DOMAIN_FCC,     REG_CLASS_SETS(reg_domain_FCC),     "FCC",     "USA"},
  {MIB_REG_DOMAIN_DOC,     REG_CLASS_SETS(reg_domain_DOC),     "DOC",     "CANADA"},
  {MIB_REG_DOMAIN_ETSI,    REG_CLASS_SETS(reg_domain_ETSI),    "ETSI",    "EUROPE"},
  {MIB_REG_DOMAIN_GERMANY, REG_CLASS_SETS(reg_domain_GERMANY), "GERMANY", "GERMANY"},
  {MIB_REG_DOMAIN_MKK,     REG_CLASS_SETS(reg_domain_MKK),     "MKK",     "JAPAN"},
  {MIB_REG_DOMAIN_FRANCE,  REG_CLASS_SETS(reg_domain_FRANCE),  "FRANCE",  "FRANCE"},
  {MIB_REG_DOMAIN_UAE,     REG_CLASS_SETS(reg_domain_UAE),     "UAE",     "UAE"},
  {MIB_REG_DOMAIN_APAC,    REG_CLASS_SETS(reg_domain_APAC),    "APAC",    "ASIA"},
  {0, 0, NULL, "", ""}
};

/***************************************** LOWER SUPPORT **********************************************/
static const uint8 ch_11n_c22_l[] = {40, 48};
static const uint8 ch_11n_c23_l[] = {56, 64};
static const uint8 ch_11n_c24_l[] = {104, 112, 120, 128, 136};
static const uint8 ch_11n_c25_l[] = {153, 161};
static const uint8 ch_11n_c26_l[] = {104, 112, 136};
static const uint8 ch_11n_c33_l[] = {5, 6, 7, 8, 9, 10, 11};
static const uint8 ch_11n_c35_l[] = {5, 6, 7, 8, 9, 10, 11, 12, 13};
static const uint8 ch_11n_c36_l[] = {104, 112};
static const uint8 ch_11n_c37_l[] = {104, 112, 136};
static const uint8 ch_11n_c38_l[] = {120, 128};

/**************** FCC ***********************************/
static const struct reg_class_t reg_class_FCC_11n_52_lower[] =
{
  {22, 50000, 40, 17, SCAN_ACTIVE,  0, 0, CHANNELS(ch_11n_c22_l),  0,  0,  0},
  {23, 50000, 40, 24, SCAN_PASSIVE, 1, 3, CHANNELS(ch_11n_c23_l), 60, 24, 30},
  {24, 50000, 40, 24, SCAN_PASSIVE, 1, 3, CHANNELS(ch_11n_c24_l), 60, 24, 30},
  {25, 50000, 40, 29, SCAN_ACTIVE,  0, 0, CHANNELS(ch_11n_c25_l),  0,  0,  0},
};

static const struct reg_class_t reg_class_FCC_11n_24_lower[] =
{
  {33, 24070, 40, 30, SCAN_ACTIVE, 0, 0, CHANNELS(ch_11n_c33_l), 0, 0, 0}
};

static const struct reg_domain_t reg_domain_FCC_lower[] =
{
  {0, NULL},
  {0, NULL},
  {REG_CLASSES(reg_class_FCC_11n_52_lower)},
  {REG_CLASSES(reg_class_FCC_11n_24_lower)},
};

/**************** DOC ***********************************/
static const struct reg_class_t reg_class_DOC_11n_52_lower[] =
{
  {22, 50000, 40, 17, SCAN_ACTIVE,  0, 0, CHANNELS(ch_11n_c22_l),  0,  0,  0},
  {23, 50000, 40, 24, SCAN_PASSIVE, 1, 3, CHANNELS(ch_11n_c23_l), 60, 24, 30},
  {24, 50000, 40, 24, SCAN_PASSIVE, 1, 3, CHANNELS(ch_11n_c26_l), 60, 24, 30},
  {25, 50000, 40, 29, SCAN_ACTIVE,  0, 0, CHANNELS(ch_11n_c25_l),  0,  0,  0},
};

static const struct reg_domain_t reg_domain_DOC_lower[] =
{
  {0, NULL},
  {0, NULL},
  {REG_CLASSES(reg_class_DOC_11n_52_lower)},
  {REG_CLASSES(reg_class_FCC_11n_24_lower)},
};

/**************** ETSI ***********************************/
static const struct reg_class_t reg_class_ETSI_11n_52_lower[] =
{
  {5, 50000, 40, 22, SCAN_ACTIVE,  0, 0, CHANNELS(ch_11n_c22_l),  0,  0,  0},
  {6, 50000, 40, 22, SCAN_PASSIVE, 1, 3, CHANNELS(ch_11n_c23_l), 60, 24, 30},
  {7, 50000, 40, 28, SCAN_PASSIVE, 1, 3, CHANNELS(ch_11n_c24_l), 60, 24, 30},
};

static const struct reg_class_t reg_class_GERMANY_11n_52_lower[] =
{
  {5, 50000, 40, 22, SCAN_ACTIVE,  0, 0, CHANNELS(ch_11n_c22_l),  0,   0,  0},
  {6, 50000, 40, 22, SCAN_PASSIVE, 1, 3, CHANNELS(ch_11n_c23_l), 60,  24, 30},
  {7, 50000, 40, 28, SCAN_PASSIVE, 1, 3, CHANNELS(ch_11n_c37_l), 60,  24, 30},
  {8, 50000, 40, 28, SCAN_PASSIVE, 1, 3, CHANNELS(ch_11n_c38_l), 600, 24, 30},
};

static const struct reg_class_t reg_class_ETSI_11n_24_lower[] =
{
  {12, 24070, 40, 20, SCAN_ACTIVE, 0, 0, CHANNELS(ch_11n_c35_l), 0, 0, 0}
};

static const struct reg_domain_t reg_domain_ETSI_lower[] =
{
  {0, NULL},
  {0, NULL},
  {REG_CLASSES(reg_class_ETSI_11n_52_lower)},
  {REG_CLASSES(reg_class_ETSI_11n_24_lower)},
};

static const struct reg_domain_t reg_domain_GERMANY_lower[] =
{
  {0, NULL},
  {0, NULL},
  {REG_CLASSES(reg_class_GERMANY_11n_52_lower)},
  {REG_CLASSES(reg_class_ETSI_11n_24_lower)},
};

/**************** MKK ***********************************/
static const struct reg_class_t reg_class_MKK_11n_52_lower[] =
{
  {5, 50000, 40, 22, SCAN_ACTIVE,  0, 0, CHANNELS(ch_11n_c22_l),  0,  0,  0},
  {6, 50000, 40, 22, SCAN_PASSIVE, 1, 3, CHANNELS(ch_11n_c23_l), 60, 24, 30},
  {7, 50000, 40, 28, SCAN_PASSIVE, 1, 3, CHANNELS(ch_11n_c24_l), 60, 24, 30}
};

static const struct reg_class_t reg_class_MKK_11n_24_lower[] =
{
  {12, 24070, 40, 23, SCAN_ACTIVE, 0, 0, CHANNELS(ch_11n_c35_l), 0, 0, 0}
};

static const struct reg_domain_t reg_domain_MKK_lower[] =
{
  {0, NULL},
  {0, NULL},
  {REG_CLASSES(reg_class_MKK_11n_52_lower)},
  {REG_CLASSES(reg_class_MKK_11n_24_lower)},
};
/**************** FRANCE ***********************************/
static const struct reg_class_t reg_class_FRANCE_11n_52_lower[] =
{
  {5, 50000, 40, 22, SCAN_ACTIVE,  0, 0, CHANNELS(ch_11n_c22_l),  0,  0,  0},
  {6, 50000, 40, 22, SCAN_PASSIVE, 1, 3, CHANNELS(ch_11n_c23_l), 60, 24, 30},
  {7, 50000, 40, 28, SCAN_PASSIVE, 1, 3, CHANNELS(ch_11n_c24_l), 60, 24, 30},
};

static const struct reg_domain_t reg_domain_FRANCE_lower[] =
{
  {0, NULL},
  {0, NULL},
  {REG_CLASSES(reg_class_FRANCE_11n_52_lower)},
  {REG_CLASSES(reg_class_ETSI_11n_24_lower)},
};

/**************** APAC ***********************************/
static const struct reg_class_t reg_class_APAC_11n_52_lower[] =
{
  {5, 50000, 40, 23, SCAN_ACTIVE,  0, 0, CHANNELS(ch_11n_c22_l),  0,   0,  0},
  {6, 50000, 40, 23, SCAN_PASSIVE, 1, 0, CHANNELS(ch_11n_c23_l), 60,  24, 30},
  {25, 50000, 40, 30, SCAN_ACTIVE,  0, 0, CHANNELS(ch_11n_c25_l),  0,  0,  0},
};

static const struct reg_class_t reg_class_APAC_11n_24_lower[] =
{
  {33, 24070, 40, 23, SCAN_ACTIVE, 0, 0, CHANNELS(ch_11n_c35_l), 0, 0, 0},
};

static const struct reg_domain_t reg_domain_APAC_lower[] =
{
  {0, NULL},
  {0, NULL},
  {REG_CLASSES(reg_class_APAC_11n_52_lower)},
  {REG_CLASSES(reg_class_APAC_11n_24_lower)},
};

/**************** UAE ***********************************/
static const struct reg_class_t reg_class_UAE_11n_52_lower[] =
{
  {5, 50000, 40, 23, SCAN_ACTIVE,  0, 0, CHANNELS(ch_11n_c22_l),  0,  0,  0},
  {6, 50000, 40, 23, SCAN_PASSIVE, 1, 3, CHANNELS(ch_11n_c23_l), 60, 24, 30},
  {7, 50000, 40, 30, SCAN_PASSIVE, 1, 3, CHANNELS(ch_11n_c24_l), 60, 24, 30},
  {8, 50000, 40, 30, SCAN_PASSIVE, 1, 3, CHANNELS(ch_11n_c25_l), 60, 24, 30},
};

static const struct reg_class_t reg_class_UAE_11n_24_lower[] =
{
  {12, 24070, 40, 20, SCAN_ACTIVE, 0, 0, CHANNELS(ch_11n_c35_l), 0, 0, 0}
};

static const struct reg_domain_t reg_domain_UAE_lower[] =
{
  {0, NULL},
  {0, NULL},
  {REG_CLASSES(reg_class_ETSI_11n_52_lower)},
  {REG_CLASSES(reg_class_ETSI_11n_24_lower)},
};

/*
 * country name :: regulatory domain :: eeprom country code
 * index in table is eeprom country code
 */
static const struct country_regulatory_t country_reg_table[] = 
{
  {"??", 0                    }, /*   CAUTION!!! first entry corresponds to undefined country */
  {"AF", MIB_REG_DOMAIN_ETSI  }, /*   1 */
  {"AX", MIB_REG_DOMAIN_ETSI  }, /*   2 */
  {"AL", MIB_REG_DOMAIN_ETSI  }, /*   3 */
  {"DZ", MIB_REG_DOMAIN_ETSI  }, /*   4 */
  {"AS", MIB_REG_DOMAIN_ETSI  }, /*   5 */
  {"AD", MIB_REG_DOMAIN_ETSI  }, /*   6 */
  {"AO", MIB_REG_DOMAIN_ETSI  }, /*   7 */
  {"AI", MIB_REG_DOMAIN_FCC   }, /*   8 */
  {"AQ", MIB_REG_DOMAIN_ETSI  }, /*   9 */
  {"AG", MIB_REG_DOMAIN_FCC   }, /*  10 */
  {"AR", MIB_REG_DOMAIN_FCC   }, /*  11 */
  {"AM", MIB_REG_DOMAIN_ETSI  }, /*  12 */
  {"AW", MIB_REG_DOMAIN_FCC   }, /*  13 */
  {"AU", MIB_REG_DOMAIN_APAC  }, /*  14 */
  {"AT", MIB_REG_DOMAIN_ETSI  }, /*  15 */
  {"AZ", MIB_REG_DOMAIN_ETSI  }, /*  16 */
  {"BS", MIB_REG_DOMAIN_FCC   }, /*  17 */
  {"BH", MIB_REG_DOMAIN_ETSI  }, /*  18 */
  {"BD", MIB_REG_DOMAIN_MKK   }, /*  19 */
  {"BB", MIB_REG_DOMAIN_FCC   }, /*  20 */
  {"BY", MIB_REG_DOMAIN_ETSI  }, /*  21 */
  {"BE", MIB_REG_DOMAIN_ETSI  }, /*  22 */
  {"BZ", MIB_REG_DOMAIN_FCC   }, /*  23 */
  {"BJ", MIB_REG_DOMAIN_ETSI  }, /*  24 */
  {"BM", MIB_REG_DOMAIN_FCC   }, /*  25 */
  {"BT", MIB_REG_DOMAIN_MKK   }, /*  26 */
  {"BO", MIB_REG_DOMAIN_FCC   }, /*  27 */
  {"BA", MIB_REG_DOMAIN_ETSI  }, /*  28 */
  {"BW", MIB_REG_DOMAIN_MKK   }, /*  29 */
  {"BV", MIB_REG_DOMAIN_MKK   }, /*  30 */
  {"BR", MIB_REG_DOMAIN_FCC   }, /*  31 */
  {"IO", MIB_REG_DOMAIN_MKK   }, /*  32 */
  {"BN", MIB_REG_DOMAIN_MKK   }, /*  33 */
  {"BG", MIB_REG_DOMAIN_ETSI  }, /*  34 */
  {"BF", MIB_REG_DOMAIN_MKK   }, /*  35 */
  {"BI", MIB_REG_DOMAIN_ETSI  }, /*  36 */
  {"KH", MIB_REG_DOMAIN_ETSI  }, /*  37 */
  {"CM", MIB_REG_DOMAIN_ETSI  }, /*  38 */
  {"CA", MIB_REG_DOMAIN_DOC   }, /*  39 */
  {"CV", MIB_REG_DOMAIN_MKK   }, /*  40 */
  {"KY", MIB_REG_DOMAIN_FCC   }, /*  41 */
  {"CF", MIB_REG_DOMAIN_ETSI  }, /*  42 */
  {"TD", MIB_REG_DOMAIN_ETSI  }, /*  43 */
  {"CL", MIB_REG_DOMAIN_FCC   }, /*  44 */
  {"CN", MIB_REG_DOMAIN_MKK   }, /*  45 */
  {"CX", MIB_REG_DOMAIN_MKK   }, /*  46 */
  {"CC", MIB_REG_DOMAIN_MKK   }, /*  47 */
  {"CO", MIB_REG_DOMAIN_FCC   }, /*  48 */
  {"KM", MIB_REG_DOMAIN_ETSI  }, /*  49 */
  {"CG", MIB_REG_DOMAIN_ETSI  }, /*  50 */
  {"CD", MIB_REG_DOMAIN_MKK   }, /*  51 */
  {"CK", MIB_REG_DOMAIN_MKK   }, /*  52 */
  {"CR", MIB_REG_DOMAIN_FCC   }, /*  53 */
  {"CI", MIB_REG_DOMAIN_MKK   }, /*  54 */
  {"HR", MIB_REG_DOMAIN_ETSI  }, /*  55 */
  {"CU", MIB_REG_DOMAIN_FCC   }, /*  56 */
  {"CY", MIB_REG_DOMAIN_ETSI  }, /*  57 */
  {"CZ", MIB_REG_DOMAIN_ETSI  }, /*  58 */
  {"DK", MIB_REG_DOMAIN_ETSI  }, /*  59 */
  {"DJ", MIB_REG_DOMAIN_MKK   }, /*  60 */
  {"DM", MIB_REG_DOMAIN_FCC   }, /*  61 */
  {"DO", MIB_REG_DOMAIN_FCC   }, /*  62 */
  {"EC", MIB_REG_DOMAIN_FCC   }, /*  63 */
  {"EG", MIB_REG_DOMAIN_ETSI  }, /*  64 */
  {"??", 0                    }, /*  65 */
  {"??", 0                    }, /*  66 */
  {"SV", MIB_REG_DOMAIN_FCC   }, /*  67 */
  {"GQ", MIB_REG_DOMAIN_ETSI  }, /*  68 */
  {"ER", MIB_REG_DOMAIN_ETSI  }, /*  69 */
  {"EE", MIB_REG_DOMAIN_ETSI  }, /*  70 */
  {"ET", MIB_REG_DOMAIN_ETSI  }, /*  71 */
  {"FK", MIB_REG_DOMAIN_FCC   }, /*  72 */
  {"FO", MIB_REG_DOMAIN_ETSI  }, /*  73 */
  {"FJ", MIB_REG_DOMAIN_MKK   }, /*  74 */
  {"FI", MIB_REG_DOMAIN_ETSI  }, /*  75 */
  {"FR", MIB_REG_DOMAIN_FRANCE}, /*  76 */
  {"GF", MIB_REG_DOMAIN_ETSI  }, /*  77 */
  {"PF", MIB_REG_DOMAIN_ETSI  }, /*  78 */
  {"TF", MIB_REG_DOMAIN_MKK   }, /*  79 */
  {"GA", MIB_REG_DOMAIN_ETSI  }, /*  80 */
  {"GM", MIB_REG_DOMAIN_ETSI  }, /*  81 */
  {"GE", MIB_REG_DOMAIN_ETSI  }, /*  82 */
  {"DE", MIB_REG_DOMAIN_GERMANY},/*  83 */
  {"GH", MIB_REG_DOMAIN_MKK   }, /*  84 */
  {"GI", MIB_REG_DOMAIN_ETSI  }, /*  85 */
  {"GR", MIB_REG_DOMAIN_ETSI  }, /*  86 */
  {"GL", MIB_REG_DOMAIN_FCC   }, /*  87 */
  {"GD", MIB_REG_DOMAIN_FCC   }, /*  88 */
  {"GP", MIB_REG_DOMAIN_ETSI  }, /*  89 */
  {"GU", MIB_REG_DOMAIN_MKK   }, /*  90 */
  {"GT", MIB_REG_DOMAIN_FCC   }, /*  91 */
  {"GG", MIB_REG_DOMAIN_ETSI  }, /*  92 */
  {"GN", MIB_REG_DOMAIN_ETSI  }, /*  93 */
  {"GW", MIB_REG_DOMAIN_ETSI  }, /*  94 */
  {"GY", MIB_REG_DOMAIN_FCC   }, /*  95 */
  {"HT", MIB_REG_DOMAIN_FCC   }, /*  96 */
  {"HM", MIB_REG_DOMAIN_MKK   }, /*  97 */
  {"VA", MIB_REG_DOMAIN_ETSI  }, /*  98 */
  {"HN", MIB_REG_DOMAIN_FCC   }, /*  99 */
  {"HK", MIB_REG_DOMAIN_APAC  }, /* 100 */
  {"HU", MIB_REG_DOMAIN_ETSI  }, /* 101 */
  {"IS", MIB_REG_DOMAIN_ETSI  }, /* 102 */
  {"IN", MIB_REG_DOMAIN_APAC  }, /* 103 */
  {"ID", MIB_REG_DOMAIN_APAC  }, /* 104 */
  {"IR", MIB_REG_DOMAIN_ETSI  }, /* 105 */
  {"IQ", MIB_REG_DOMAIN_ETSI  }, /* 106 */
  {"IE", MIB_REG_DOMAIN_ETSI  }, /* 107 */
  {"IM", MIB_REG_DOMAIN_ETSI  }, /* 108 */
  {"IL", MIB_REG_DOMAIN_ETSI  }, /* 109 */
  {"IT", MIB_REG_DOMAIN_ETSI  }, /* 110 */
  {"JM", MIB_REG_DOMAIN_FCC   }, /* 111 */
  {"JP", MIB_REG_DOMAIN_MKK   }, /* 112 */
  {"JE", MIB_REG_DOMAIN_ETSI  }, /* 113 */
  {"JO", MIB_REG_DOMAIN_ETSI  }, /* 114 */
  {"KZ", MIB_REG_DOMAIN_ETSI  }, /* 115 */
  {"KE", MIB_REG_DOMAIN_ETSI  }, /* 116 */
  {"KI", MIB_REG_DOMAIN_ETSI  }, /* 117 */
  {"KP", MIB_REG_DOMAIN_MKK   }, /* 118 */
  {"KR", MIB_REG_DOMAIN_MKK   }, /* 119 */
  {"KW", MIB_REG_DOMAIN_ETSI  }, /* 120 */
  {"KG", MIB_REG_DOMAIN_ETSI  }, /* 121 */
  {"LA", MIB_REG_DOMAIN_MKK   }, /* 122 */
  {"LV", MIB_REG_DOMAIN_ETSI  }, /* 123 */
  {"LB", MIB_REG_DOMAIN_ETSI  }, /* 124 */
  {"LS", MIB_REG_DOMAIN_ETSI  }, /* 125 */
  {"LR", MIB_REG_DOMAIN_ETSI  }, /* 126 */
  {"LY", MIB_REG_DOMAIN_ETSI  }, /* 127 */
  {"LI", MIB_REG_DOMAIN_ETSI  }, /* 128 */
  {"LT", MIB_REG_DOMAIN_ETSI  }, /* 129 */
  {"LU", MIB_REG_DOMAIN_ETSI  }, /* 130 */
  {"MO", MIB_REG_DOMAIN_ETSI  }, /* 131 */
  {"MK", MIB_REG_DOMAIN_ETSI  }, /* 132 */
  {"MG", MIB_REG_DOMAIN_ETSI  }, /* 133 */
  {"MW", MIB_REG_DOMAIN_ETSI  }, /* 134 */
  {"MY", MIB_REG_DOMAIN_APAC  }, /* 135 */
  {"MV", MIB_REG_DOMAIN_MKK   }, /* 136 */
  {"ML", MIB_REG_DOMAIN_ETSI  }, /* 137 */
  {"MT", MIB_REG_DOMAIN_ETSI  }, /* 138 */
  {"MH", MIB_REG_DOMAIN_MKK   }, /* 139 */
  {"MQ", MIB_REG_DOMAIN_ETSI  }, /* 140 */
  {"MR", MIB_REG_DOMAIN_ETSI  }, /* 141 */
  {"MU", MIB_REG_DOMAIN_ETSI  }, /* 142 */
  {"YT", MIB_REG_DOMAIN_ETSI  }, /* 143 */
  {"MX", MIB_REG_DOMAIN_FCC   }, /* 144 */
  {"FM", MIB_REG_DOMAIN_MKK   }, /* 145 */
  {"MD", MIB_REG_DOMAIN_ETSI  }, /* 146 */
  {"MC", MIB_REG_DOMAIN_ETSI  }, /* 147 */
  {"MN", MIB_REG_DOMAIN_ETSI  }, /* 148 */
  {"ME", MIB_REG_DOMAIN_ETSI  }, /* 149 */
  {"MS", MIB_REG_DOMAIN_ETSI  }, /* 150 */
  {"MA", MIB_REG_DOMAIN_ETSI  }, /* 151 */
  {"MZ", MIB_REG_DOMAIN_ETSI  }, /* 152 */
  {"MM", MIB_REG_DOMAIN_ETSI  }, /* 153 */
  {"NA", MIB_REG_DOMAIN_ETSI  }, /* 154 */
  {"NR", MIB_REG_DOMAIN_MKK   }, /* 155 */
  {"NP", MIB_REG_DOMAIN_MKK   }, /* 156 */
  {"NL", MIB_REG_DOMAIN_ETSI  }, /* 157 */
  {"AN", MIB_REG_DOMAIN_FCC   }, /* 158 */
  {"NC", MIB_REG_DOMAIN_MKK   }, /* 159 */
  {"NZ", MIB_REG_DOMAIN_MKK  }, /* 160 */
  {"NI", MIB_REG_DOMAIN_FCC   }, /* 161 */
  {"NE", MIB_REG_DOMAIN_ETSI  }, /* 162 */
  {"NG", MIB_REG_DOMAIN_ETSI  }, /* 163 */
  {"NU", MIB_REG_DOMAIN_MKK   }, /* 164 */
  {"NF", MIB_REG_DOMAIN_MKK   }, /* 165 */
  {"MP", MIB_REG_DOMAIN_MKK   }, /* 166 */
  {"NO", MIB_REG_DOMAIN_ETSI  }, /* 167 */
  {"OM", MIB_REG_DOMAIN_ETSI  }, /* 168 */
  {"PK", MIB_REG_DOMAIN_ETSI  }, /* 169 */
  {"PW", MIB_REG_DOMAIN_MKK   }, /* 170 */
  {"PA", MIB_REG_DOMAIN_FCC   }, /* 171 */
  {"PG", MIB_REG_DOMAIN_ETSI  }, /* 172 */
  {"PY", MIB_REG_DOMAIN_FCC   }, /* 173 */
  {"PE", MIB_REG_DOMAIN_FCC   }, /* 174 */
  {"PH", MIB_REG_DOMAIN_APAC  }, /* 175 */
  {"PN", MIB_REG_DOMAIN_MKK   }, /* 176 */
  {"PL", MIB_REG_DOMAIN_ETSI  }, /* 177 */
  {"PT", MIB_REG_DOMAIN_ETSI  }, /* 178 */
  {"PR", MIB_REG_DOMAIN_FCC   }, /* 179 */
  {"QA", MIB_REG_DOMAIN_ETSI  }, /* 180 */
  {"RE", MIB_REG_DOMAIN_ETSI  }, /* 181 */
  {"RO", MIB_REG_DOMAIN_ETSI  }, /* 182 */
  {"RU", MIB_REG_DOMAIN_ETSI  }, /* 183 */
  {"RW", MIB_REG_DOMAIN_ETSI  }, /* 184 */
  {"BL", MIB_REG_DOMAIN_FCC   }, /* 185 */
  {"SH", MIB_REG_DOMAIN_ETSI  }, /* 186 */
  {"KN", MIB_REG_DOMAIN_FCC   }, /* 187 */
  {"LC", MIB_REG_DOMAIN_FCC   }, /* 188 */
  {"MF", MIB_REG_DOMAIN_FCC   }, /* 189 */
  {"PM", MIB_REG_DOMAIN_FCC   }, /* 190 */
  {"VC", MIB_REG_DOMAIN_FCC   }, /* 191 */
  {"WS", MIB_REG_DOMAIN_MKK   }, /* 192 */
  {"SM", MIB_REG_DOMAIN_ETSI  }, /* 193 */
  {"ST", MIB_REG_DOMAIN_ETSI  }, /* 194 */
  {"SA", MIB_REG_DOMAIN_ETSI  }, /* 195 */
  {"SN", MIB_REG_DOMAIN_ETSI  }, /* 196 */
  {"RS", MIB_REG_DOMAIN_ETSI  }, /* 197 */
  {"SC", MIB_REG_DOMAIN_MKK   }, /* 198 */
  {"SL", MIB_REG_DOMAIN_ETSI  }, /* 199 */
  {"SG", MIB_REG_DOMAIN_APAC  }, /* 200 */
  {"SK", MIB_REG_DOMAIN_ETSI  }, /* 201 */
  {"SI", MIB_REG_DOMAIN_ETSI  }, /* 202 */
  {"SB", MIB_REG_DOMAIN_MKK   }, /* 203 */
  {"SO", MIB_REG_DOMAIN_ETSI  }, /* 204 */
  {"ZA", MIB_REG_DOMAIN_ETSI  }, /* 205 */
  {"GS", MIB_REG_DOMAIN_ETSI  }, /* 206 */
  {"ES", MIB_REG_DOMAIN_ETSI  }, /* 207 */
  {"LK", MIB_REG_DOMAIN_MKK   }, /* 208 */
  {"SD", MIB_REG_DOMAIN_ETSI  }, /* 209 */
  {"SR", MIB_REG_DOMAIN_FCC   }, /* 210 */
  {"SJ", MIB_REG_DOMAIN_ETSI  }, /* 211 */
  {"SZ", MIB_REG_DOMAIN_ETSI  }, /* 212 */
  {"SE", MIB_REG_DOMAIN_ETSI  }, /* 213 */
  {"CH", MIB_REG_DOMAIN_ETSI  }, /* 214 */
  {"SY", MIB_REG_DOMAIN_ETSI  }, /* 215 */
  {"TW", MIB_REG_DOMAIN_MKK   }, /* 216 */
  {"TJ", MIB_REG_DOMAIN_ETSI  }, /* 217 */
  {"TZ", MIB_REG_DOMAIN_ETSI  }, /* 218 */
  {"TH", MIB_REG_DOMAIN_MKK   }, /* 219 */
  {"TL", MIB_REG_DOMAIN_MKK   }, /* 220 */
  {"TG", MIB_REG_DOMAIN_ETSI  }, /* 221 */
  {"TK", MIB_REG_DOMAIN_MKK   }, /* 222 */
  {"TO", MIB_REG_DOMAIN_ETSI  }, /* 223 */
  {"TT", MIB_REG_DOMAIN_ETSI  }, /* 224 */
  {"TN", MIB_REG_DOMAIN_ETSI  }, /* 225 */
  {"TR", MIB_REG_DOMAIN_ETSI  }, /* 226 */
  {"TM", MIB_REG_DOMAIN_ETSI  }, /* 227 */
  {"TC", MIB_REG_DOMAIN_FCC   }, /* 228 */
  {"TV", MIB_REG_DOMAIN_MKK   }, /* 229 */
  {"UG", MIB_REG_DOMAIN_ETSI  }, /* 230 */
  {"UA", MIB_REG_DOMAIN_ETSI  }, /* 231 */
  {"AE", MIB_REG_DOMAIN_UAE   }, /* 232 */
  {"GB", MIB_REG_DOMAIN_ETSI  }, /* 233 */
  {"US", MIB_REG_DOMAIN_FCC   }, /* 234 */
  {"UM", MIB_REG_DOMAIN_FCC   }, /* 235 */
  {"UY", MIB_REG_DOMAIN_FCC   }, /* 236 */
  {"UZ", MIB_REG_DOMAIN_ETSI  }, /* 237 */
  {"VU", MIB_REG_DOMAIN_MKK   }, /* 238 */
  {"??", 0                    }, /* 239 */
  {"VE", MIB_REG_DOMAIN_FCC   }, /* 240 */
  {"VN", MIB_REG_DOMAIN_MKK   }, /* 241 */
  {"VG", MIB_REG_DOMAIN_FCC   }, /* 242 */
  {"VI", MIB_REG_DOMAIN_FCC   }, /* 243 */
  {"WF", MIB_REG_DOMAIN_ETSI  }, /* 244 */
  {"EH", MIB_REG_DOMAIN_ETSI  }, /* 245 */
  {"YE", MIB_REG_DOMAIN_ETSI  }, /* 246 */
  {"ZM", MIB_REG_DOMAIN_ETSI  }, /* 247 */
  {"ZW", MIB_REG_DOMAIN_ETSI  }, /* 248 */
};

/* Build scan vector according to rules in SRD-051-248-AP_SCAN.doc ch. 5.4 */
static int
scan_vector_grow (mtlk_scan_vector_t *vector, uint16 grow_count)
{
  void *mem = NULL;
  size_t new_sz = (vector->count + grow_count) * sizeof(FREQUENCY_ELEMENT);
  mem = mtlk_osal_mem_alloc((uint32)new_sz, MTLK_MEM_TAG_SCAN_VECTOR);
  if (!mem)
    return MTLK_ERR_NO_MEM;

  memset(mem, 0, new_sz);
  
  if (vector->params && vector->used) {
    memcpy(mem, vector->params, vector->used * sizeof(FREQUENCY_ELEMENT));
  }

  if (vector->params) {
    mtlk_osal_mem_free(vector->params);
  }

  vector->count = (uint16)(vector->count + grow_count);
  vector->params = mem;

  ILOG4_DDP("count = %d, used = %d, params = %p", vector->count, vector->used, vector->params);
  return MTLK_ERR_OK;
}

void __MTLK_IFUNC
mtlk_free_scan_vector (mtlk_scan_vector_t *vector)
{
  if (vector->params) {
    mtlk_osal_mem_free(vector->params);
    memset(vector, 0, sizeof(*vector));
    ILOG4_V("Scan vector freed");
  }
}

static void
use_strictest(mtlk_scan_vector_t *vector, const struct reg_class_t *cls, uint8 idx)
{
  int i;

  for (i = 0; i < vector->used; i++) {
    if (vector->params[i].u16Channel == HOST_TO_MAC16((uint16)(cls->channels[idx]))) {
      if (vector->params[i].u8ScanType < cls->scan_type)
        vector->params[i].u8ScanType = cls->scan_type;
#if 1
      if (vector->params[i].i16CbTransmitPowerLimit > HOST_TO_MAC16((int16)(cls->max_power))) {
        vector->params[i].i16CbTransmitPowerLimit = HOST_TO_MAC16((int16)(cls->max_power));
        vector->params[i].i16nCbTransmitPowerLimit = HOST_TO_MAC16((int16)(cls->max_power));
      }
      vector->params[i].i16AntennaGain = 0;
#endif
      vector->params[i].u16ChannelAvailabilityCheckTime = HOST_TO_MAC16(cls->channelAvailabilityCheckTime);
      vector->params[i].u8SmRequired = cls->sm_required;
      return;
    }
  }

  if (vector->used == vector->count) {
#if 0
    ILOG2_D("Increasing scan vector by %d elements", cls->num_channels);
    scan_vector_grow(vector, cls->num_channels);
#else
    ELOG_D("No space left in scan vector (%d more elements needed)",
        cls->num_channels);
    return;
#endif
  }

  vector->params[vector->used].u16Channel = HOST_TO_MAC16((uint16)(cls->channels[idx]));
  vector->params[vector->used].u8ScanType = cls->scan_type;
#if 1
  vector->params[vector->used].i16CbTransmitPowerLimit = HOST_TO_MAC16((int16)(cls->max_power));
  vector->params[vector->used].i16nCbTransmitPowerLimit = HOST_TO_MAC16((int16)(cls->max_power));
  vector->params[vector->used].i16AntennaGain = 0;
#endif
  vector->params[vector->used].u16ChannelAvailabilityCheckTime = HOST_TO_MAC16(cls->channelAvailabilityCheckTime);
  vector->params[vector->used].u8SmRequired = cls->sm_required;

  vector->used++;
}

static __INLINE int
check_band (const struct reg_class_t *rcl, int freq)
{
  ILOG3_DD("Check band: class->freq = %d, freq = %d", rcl->start_freq, freq);
  if ((rcl->start_freq == freq) || (rcl->start_freq == 48500 && freq == 50000))
    return 1;

  return 0;
}

static int
fill_domain_channels (mtlk_scan_vector_t *vector,
    const struct reg_domain_t *domain, int frequency, uint16 num_of_protocols,
    uint8 spectrum, BOOL disable_sm_channels)
{
  uint8 i, j, switch_mode;
  uint16 p;
  const struct reg_domain_t *dom = domain;
  const struct reg_class_t * cls;

  switch_mode = mtlk_get_chnl_switch_mode(spectrum, ALTERNATE_UPPER, 0);
  spectrum = spectrum == SPECTRUM_40MHZ ? 40 : 20;
  for (p = 0; p < num_of_protocols; p ++) {
    for (i = 0; i < dom[p].num_classes; i++) {
      cls = &dom[p].classes[i];

      if (check_band(cls, frequency) == 0)
        continue;

      if (cls->sm_required && disable_sm_channels)
        continue;
      if (cls->spacing == spectrum)
        for (j = 0; j < cls->num_channels; j++) {
          use_strictest(vector, cls, j);
        }
    }
  }
  /* set switch mode */
  for (i = 0; i < vector->used; i++)
    vector->params[i].u8SwitchMode = switch_mode;
  return MTLK_ERR_OK;
}

static int
build_scan_vector (mtlk_scan_vector_t *vector, uint8 reg_domain, int frequency,
  uint8 spectrum, BOOL disable_sm_channels)
{
  int i = 0;
  int res = MTLK_ERR_OK;
  uint16 num_of_protocols;
  const struct reg_domain_t *domain = mtlk_get_domain(reg_domain
                                                      ,&res
                                                      ,&num_of_protocols
                                                      ,(uint8)0
                                                      ,MTLK_CHNLS_SCAN_CALLER);

  mtlk_free_scan_vector(vector);

  if (scan_vector_grow(vector, MAX_CHANNELS) != 0)
    return MTLK_ERR_NO_MEM;

  if (domain) {
    ILOG4_D("Scan domain %d", reg_domain);
    return fill_domain_channels(vector, domain, frequency, num_of_protocols,
      spectrum, disable_sm_channels);
  } else {
    ILOG2_V("Scan all domains");
    num_of_protocols = sizeof(reg_domain_FCC) / sizeof(reg_domain_FCC[0]);
    /* this number is the same for all domains (4 as of now) */
    while (all_domains[i].dom) {
      ILOG4_DP("all_domains[%d] = %p", i, all_domains[i].dom);
      res = fill_domain_channels(vector, all_domains[i].dom, frequency,
        num_of_protocols, spectrum, disable_sm_channels);
      if (res < 0)
        return res;

      i++;
    }
  }

  return MTLK_ERR_OK;
}

/* TODO: implement correct algorithm*/
static int
fix_scan_vector_scan_type (mtlk_handle_t context, mtlk_scan_vector_t *vector, uint8 _11d, uint8 reg_domain)
{
  int i;
  uint16 channel;
  uint8 mode = 0xff;
  uint8 _11h_radar_detection_enabled = 0;

  /* if 11d enabled - use passive scan */
  if (_11d)
    mode = SCAN_PASSIVE;

  /* if reg_domain known - use table for scan */
  if (reg_domain) // use scan modes from table
    mode = 0xff;

  /* if neither 11d nor reg_domain known - use active scan */
  if (!_11d && !reg_domain) // set active mode
    mode = SCAN_ACTIVE;

  if (mode != 0xff) {
    for (i = 0; i < vector->used; i++)
      vector->params[i].u8ScanType = mode;
  }

  for (i = 0; i < vector->used; i++) {
    channel = MAC_TO_HOST16(vector->params[i].u16Channel);
    if ((channel >= 1 && channel <= 13) || (channel >= 36 && channel <= 48))
      vector->params[i].u8ScanType = SCAN_ACTIVE;
  }

  for (i = 0; i < vector->used; i++) {
    uint16 pl, ag, ch;

    pl = mtlk_scan_calc_tx_power_lim_wrapper(context, 1 /* 40 MHz */,
        reg_domain, (uint8)MAC_TO_HOST16(vector->params[i].u16Channel));
    vector->params[i].i16CbTransmitPowerLimit = HOST_TO_MAC16(pl);
    pl = mtlk_scan_calc_tx_power_lim_wrapper(context, 0 /* 20 MHz */,
        reg_domain, (uint8)MAC_TO_HOST16(vector->params[i].u16Channel));
    vector->params[i].i16nCbTransmitPowerLimit = HOST_TO_MAC16(pl);
    ch = MAC_TO_HOST16(vector->params[i].u16Channel);
    ag = mtlk_get_antenna_gain_wrapper(context, (uint8)ch);
    vector->params[i].i16AntennaGain = HOST_TO_MAC16(ag);

    mtlk_fill_channel_params_by_tpc(context, &vector->params[i]);
  }

  _11h_radar_detection_enabled = mtlk_pdb_get_int(mtlk_vap_get_param_db(((mtlk_core_t*)context)->vap_handle), PARAM_DB_DFS_RADAR_DETECTION);
  ILOG3_D("11h_radar_detect = %d", _11h_radar_detection_enabled);
  if (_11h_radar_detection_enabled == 0) {
    for (i = 0; i < vector->used; i++) {
      vector->params[i].u8SmRequired = 0;
    }
  }

  return 0;
}

int __MTLK_IFUNC
mtlk_prepare_scan_vector(mtlk_handle_t context, struct mtlk_scan *scan_data, int freq, uint8 reg_domain)
{
  int frequency = 50000;
  int res;
  mtlk_eeprom_data_t *eeprom_data = mtlk_core_get_eeprom((mtlk_core_t*) context);

  ILOG3_DD("domain = %d, freq = %d", reg_domain, freq);

  if (mtlk_eeprom_is_band_valid(eeprom_data, freq) == MTLK_ERR_OK)
  {
    if (MTLK_HW_BAND_2_4_GHZ == freq)
    {
        frequency = 24070;
    }
    else if (MTLK_HW_BAND_5_2_GHZ == freq)
    {
        frequency = 50000;
    }
    else
    {
        return MTLK_ERR_PARAMS;
    }
  } else {
    return MTLK_ERR_PARAMS; /* don't prepare scan vector if there is no TPC*/
  }

  res = build_scan_vector(scan_data->vector, reg_domain, frequency,
          scan_data->spectrum, mtlk_eeprom_get_disable_sm_channels(eeprom_data) );

  if (res != MTLK_ERR_OK)
    return res;

  // If regulatory domain is not known use PASSIVE scan only if p802_11d param is set
  fix_scan_vector_scan_type(context, scan_data->vector, mtlk_core_get_dot11d(((mtlk_core_t*)context)), reg_domain);

  return MTLK_ERR_OK;
}

/* This function returns offset of last read channel in channels array.
   If there are no more channels it equals scan_data->scan_vector.used.
*/
uint8 __MTLK_IFUNC
mtlk_get_channels_for_reg_domain (struct mtlk_scan *scan_data, FREQUENCY_ELEMENT *channels,
    uint8 *num_channels)
{
  uint8 offs = scan_data->ch_offset;
  uint8 num = (uint8)(mtlk_scan_vector_get_used(scan_data->vector) - offs);
  num = (num < (*num_channels)) ? num : (*num_channels);

  ILOG4_DD("Offset = %d, num = %d", offs, num);

  /* Set up rates and channels depending upon the frequency band and HT / Legacy mode */

  memcpy(channels, mtlk_scan_vector_get_offset(scan_data->vector, offs), sizeof(FREQUENCY_ELEMENT) * num);

  *num_channels = num;

  offs = (uint8)(offs + num);

  ILOG4_D("offset = %d", offs);
  return offs;
}
/******          AOCS related functions             ******/

/**************************************************************************
* mtlk_get_domain
*/
const struct reg_domain_t * __MTLK_IFUNC
mtlk_get_domain(uint8 reg_domain, int *result, uint16 *num_of_protocols, uint8 u8Upper, uint16 caller)
{
  const struct reg_domain_t *domain = NULL;
  *result = MTLK_ERR_OK;

  ILOG3_DD("caller = %d, u8Upper = %d",caller,u8Upper);
  switch (reg_domain) {
    case MIB_REG_DOMAIN_FCC:
      if (u8Upper == 0 && caller == MTLK_CHNLS_DOT11H_CALLER) {
        domain = reg_domain_FCC_lower;
        *num_of_protocols = sizeof(reg_domain_FCC_lower)/sizeof(reg_domain_FCC_lower[0]);
        ILOG3_V("Lower");
      }
      else {
        domain = reg_domain_FCC;
        *num_of_protocols = sizeof(reg_domain_FCC)/sizeof(reg_domain_FCC[0]);
        ILOG3_V("Upper");
      }
      ILOG3_D("domain = reg_domain_FCC, entries = %d. ",*num_of_protocols);
      break;
    case MIB_REG_DOMAIN_DOC:
      if (u8Upper == 0 && caller == MTLK_CHNLS_DOT11H_CALLER) {
        domain = reg_domain_DOC_lower;
        *num_of_protocols = sizeof(reg_domain_DOC_lower)/sizeof(reg_domain_DOC_lower[0]);
        ILOG3_V("Lower");
      }
      else {
        domain = reg_domain_DOC;
        *num_of_protocols = sizeof(reg_domain_DOC)/sizeof(reg_domain_DOC[0]);
        ILOG3_V("Upper");
      }
      ILOG3_D("domain = reg_domain_DOC, entries = %d. ",*num_of_protocols);
      break;
    case MIB_REG_DOMAIN_ETSI:
      if (u8Upper == 0 && caller == MTLK_CHNLS_DOT11H_CALLER) {
        domain = reg_domain_ETSI_lower;
        *num_of_protocols = sizeof(reg_domain_ETSI_lower)/sizeof(reg_domain_ETSI_lower[0]);
        ILOG3_V("Lower");
      }
      else {
        domain = reg_domain_ETSI;
        *num_of_protocols = sizeof(reg_domain_ETSI)/sizeof(reg_domain_ETSI[0]);
        ILOG3_V("Upper");
      }
      ILOG3_D("domain = reg_domain_ETSI, entries = %d. ",*num_of_protocols);
      break;

    case MIB_REG_DOMAIN_GERMANY:
      if (u8Upper == 0 && caller == MTLK_CHNLS_DOT11H_CALLER) {
        domain = reg_domain_GERMANY_lower;
        *num_of_protocols = sizeof(reg_domain_GERMANY_lower)/sizeof(reg_domain_GERMANY_lower[0]);
        ILOG3_V("Lower");
      }
      else {
        domain = reg_domain_GERMANY;
        *num_of_protocols = sizeof(reg_domain_GERMANY)/sizeof(reg_domain_GERMANY[0]);
        ILOG3_V("Upper");
      }
      ILOG3_D("domain = reg_domain_ETSI, entries = %d. ",*num_of_protocols);
      break;

    case MIB_REG_DOMAIN_MKK:
      if (u8Upper == 0 && caller == MTLK_CHNLS_DOT11H_CALLER) {
        domain = reg_domain_MKK_lower;
        *num_of_protocols = sizeof(reg_domain_MKK_lower)/sizeof(reg_domain_MKK_lower[0]);
        ILOG3_V("Lower");
      }
      else {
        domain = reg_domain_MKK;
        *num_of_protocols = sizeof(reg_domain_MKK)/sizeof(reg_domain_MKK[0]);
        ILOG3_V("Upper");
      }
      ILOG3_D("domain = reg_domain_MKK, entries = %d. ",*num_of_protocols);
      break;
    case MIB_REG_DOMAIN_FRANCE:
      if (u8Upper == 0 && caller == MTLK_CHNLS_DOT11H_CALLER) {
        domain = reg_domain_FRANCE_lower;
        *num_of_protocols = sizeof(reg_domain_FRANCE_lower)/sizeof(reg_domain_FRANCE_lower[0]);
        ILOG3_V("Lower");
      }
      else {
        domain = reg_domain_FRANCE;
        *num_of_protocols = sizeof(reg_domain_FRANCE)/sizeof(reg_domain_FRANCE[0]);
        ILOG3_V("Upper");
      }
      ILOG3_D("domain = reg_domain_FRANCE, entries = %d. ",*num_of_protocols);
      break;
    case MIB_REG_DOMAIN_APAC:
      if (u8Upper == 0 && caller == MTLK_CHNLS_DOT11H_CALLER) {
        domain = reg_domain_APAC_lower;
        *num_of_protocols = sizeof(reg_domain_APAC_lower)/sizeof(reg_domain_APAC_lower[0]);
        ILOG3_V("Lower");
      }
      else {
        domain = reg_domain_APAC;
        *num_of_protocols = sizeof(reg_domain_APAC)/sizeof(reg_domain_APAC[0]);
        ILOG3_V("Upper");
      }
      ILOG3_D("domain = reg_domain_APAC, entries = %d. ",*num_of_protocols);
      break;
    case MIB_REG_DOMAIN_UAE:
      if (u8Upper == 0 && caller == MTLK_CHNLS_DOT11H_CALLER) {
        domain = reg_domain_UAE_lower;
        *num_of_protocols = sizeof(reg_domain_UAE_lower)/sizeof(reg_domain_UAE_lower[0]);
        ILOG3_V("Lower");
      }
      else {
        domain = reg_domain_UAE;
        *num_of_protocols = sizeof(reg_domain_UAE)/sizeof(reg_domain_UAE[0]);
        ILOG3_V("Upper");
      }
      ILOG3_D("domain = reg_domain_UAE, entries = %d. ",*num_of_protocols);
      break;
    default:
      ILOG2_V("Error in mtlk_get_domain(), bad domain. ");
      *result = MTLK_ERR_UNKNOWN;
      *num_of_protocols = 0;
      break;
//      return MTLK_ERR_UNKNOWN;
  }/*switch*/
  return domain;
}

int __MTLK_IFUNC
mtlk_get_channel_data (mtlk_get_channel_data_t *params,
  FREQUENCY_ELEMENT *freq_element, uint8 *non_occupied_period,
  uint8 *radar_detection_validity_time)
{
  const struct reg_domain_t *domain = NULL;
  const struct reg_class_t * cls;
  uint8 scan_type = 0, sm_required = 0, get_data = 0, u8UpperLowerBonding = 0;
  uint16 i, j, m, channelAvailabilityCheckTime = 0, num_of_protocols_in_domains = 0 ;
  uint8 band = channel_to_band(params->channel);
  int res = MTLK_ERR_OK, result = MTLK_ERR_OK;

  /*TODO: AP- use strict? or according to protocol? */
  if (non_occupied_period)
    *non_occupied_period = 0;
  if (radar_detection_validity_time)
    *radar_detection_validity_time = 0;
  ILOG3_DDD("reg_domain = %d, ap = %d, channel = %d \n", params->reg_domain,
    params->ap, params->channel);

  /* fill in the FREQUENCY_ELEMENT struct.
  run over all classes in the protocol (channel may be in more than one class).
     note- in case of passive scan for any class having the channel in,
     return passive */
  /*use the requested domain only (set k = 5),
  and loop one time only on table (set num_of_protocols_in_domains = 1)*/
  ILOG3_DD("Bonding = %d (0=upper,1=lower), SpectrumMode = %d",
    params->bonding, params->spectrum_mode);
  if ((!params->bonding /*reverse logic*/&& params->spectrum_mode) ||
      (!params->spectrum_mode/*if nCB upper/lower not relevant*/)){
    u8UpperLowerBonding = 1;
  }
  domain = mtlk_get_domain(params->reg_domain, &result, &num_of_protocols_in_domains,
    u8UpperLowerBonding, MTLK_CHNLS_DOT11H_CALLER);
  if (result != MTLK_ERR_OK) {
    ILOG3_V("return error from mtlk_get_domain");
    return MTLK_ERR_PARAMS;
  }
  for (m = 0; m < num_of_protocols_in_domains; m++) {
    ILOG3_D("domain num_classes %d", domain[m].num_classes);
    /* mtlk_vap_is_ap(core->vap_handle) */
    if (params->ap && (m == MAKE_PROTOCOL_INDEX(params->is_ht, band))) {
      /*AP knows domain, band and HT / Legacy mode*/
      get_data = 1;
      ILOG3_D("AP, get_data = %d",get_data);
    }
    /* mtlk_vap_is_ap(core->vap_handle) */
    else if (!params->ap) {
      /*STA search all table*/
      get_data = 1;
      ILOG3_D("STA, get_data = %d",get_data);
    }
    /*else - skip this protocol search*/
    for (i = 0; i < domain[m].num_classes; i++) {
      /*
      run through the domains,
      for example, for reg_domain_FCC[] for entry m=3 the first domain is:
          {REG_CLASSES(reg_class_FCC_11n_52)},
      run over the classes (reg_class_t) in the selected domain,
      the first class in this protocol is:
          {16, 50000, 40, 16, SCAN_ACTIVE, 3, CHANNELS(ch_11n_c16)},
      and the channels in the first class are:    ch_11n_c16[] = {36, 44}
      */
      if (get_data) {
        cls = &domain[m].classes[i];
        ILOG3_PDD("cls = %p, num_channels = %d, first = %d",cls, (int)cls->num_channels, cls->channels[0]);
        for (j = 0; j < cls->num_channels; j++) {
          /*run through the channels in the class*/
          if (params->channel == cls->channels[j]) {
            if ((params->spectrum_mode == 1 && cls->spacing == 40) ||
                (params->spectrum_mode == 0 && cls->spacing == 20)) {
              ILOG3_DD("channel = cls->channels[%d] = %d",j, params->channel);
              sm_required |= cls->sm_required;
              scan_type |= cls->scan_type;
              /*to avoid 0 when more than one channel and one have value != 0*/
              if ((cls->non_occupied_period != 0) && (non_occupied_period))
                *non_occupied_period = cls->non_occupied_period;
              /*to avoid 0 when more than one channel and one have value != 0*/
              if ((cls->radar_detection_validity_time != 0) && (radar_detection_validity_time))
                *radar_detection_validity_time = cls->radar_detection_validity_time;
              if (channelAvailabilityCheckTime < cls->channelAvailabilityCheckTime)
                channelAvailabilityCheckTime = cls->channelAvailabilityCheckTime;
            }
            else
              ILOG3_DDD("channel %d u8SpectrumMode = %d, spacing = %d", cls->channels[j],
                params->spectrum_mode, cls->spacing);
          }
        }
      }/*if (get_data)*/
      /*fill the struct*/
      if(freq_element)
      {
        freq_element->u16Channel = (uint16)params->channel;
        freq_element->u8ScanType = scan_type;
        freq_element->u8SmRequired = sm_required;
        freq_element->u16ChannelAvailabilityCheckTime  = channelAvailabilityCheckTime;
      }
    }/*for(i..*/
    /*clear get_data for next protocol use*/
    get_data = 0;
  }/*for (m..*/
  return res;
}

/******    Power calculations related functions    ******/
/*****************************************************************************/
/* 10 * ILOG30(num) = table_log_mul10[num - 1], num = [1..5] */
static const int16 table_log_mul10[] = {0, 3, 4, 6, 7};

static int16
get_log_value (uint8 num)
{
  if (num == 0)
    return (-1);

  if (num > sizeof(table_log_mul10) / sizeof(table_log_mul10[0]))
    return 0;

  return table_log_mul10[num - 1];
}

static int16
get_from_reg_table (struct reg_tx_limit *reg_lim, uint8 reg_domain, uint16 channel, uint8 *mitigation)
{
  int16 tx = INT16_MAX;
  uint8 i, j;
  struct reg_domain_tx_limit *dom_lim;

  ILOG3_DD("Table lookup for: reg_dom = 0x%x, channel = %d", reg_domain, channel);
  *mitigation = 0;

  while (reg_lim) {
    if (reg_lim->reg_domain == reg_domain) {
      dom_lim = reg_lim->dom_lim;

      while (dom_lim) {
        for (i = 0; i < dom_lim->num_classes; i++)
          for (j = 0; j < dom_lim->tx_lim[i].num_ch; j++)
            if (dom_lim->tx_lim[i].channels[j].channel == channel) {
              if (dom_lim->tx_lim[i].channels[j].tx_lim < tx) {
                tx = dom_lim->tx_lim[i].channels[j].tx_lim;
                *mitigation = dom_lim->tx_lim[i].channels[j].mitigation;
              }
              break;
            }

        dom_lim = dom_lim->next;
      }
    }

    reg_lim = reg_lim->next;
  }

  ILOG3_DD("tx = %d (INT16_MAX = %d)", tx, INT16_MAX);
  return tx == INT16_MAX ? 0 : tx;
}

static int16
get_from_hw_table (struct hw_reg_tx_limit *hw_lim, uint8 reg_domain,
  uint16 freq, uint8 CB, uint8 ignore_cb)
{
  int16 tx = INT16_MAX;
  uint8 i;
  uint16 j;

  /* TODO: if we will handle regulatory classes:
     add if (!reg_classes_required){
           for (...; num_classes;...)
         } else {
           // go to specified class and try to find frequency there
         }
  */

  ILOG3_DDD("Looking for limit for: freq=%d, reg_dom=%d, CB=%d", freq, reg_domain, CB);

  while (hw_lim) {
    if (hw_lim->reg_domain == reg_domain) {
      for (i = 0; i < hw_lim->num_classes; i++) {
        for (j = 0; j < hw_lim->tx_lim[i].num_freq; j++) {
          if (hw_lim->tx_lim[i].tx_lim[j].freq == freq &&
              (ignore_cb || hw_lim->tx_lim[i].tx_lim[j].spectrum == CB)) {
            if (hw_lim->tx_lim[i].tx_lim[j].tx_lim < tx) {
              tx = hw_lim->tx_lim[i].tx_lim[j].tx_lim;
            }
            break;
          }
        }
      }
    }

    hw_lim = hw_lim->next;
  }

  ILOG3_D("HW Tx limit = %u", tx);

  return tx;
}

uint16  __MTLK_IFUNC
mtlk_calc_start_freq (drv_params_t *param, uint16 channel)
{
  uint16 offset = 0;

  if (param->upper_lower == ALTERNATE_UPPER) {
    offset = 10 * ((param->bandwidth == 20) ? 2 : 1);
  } else if (param->upper_lower == ALTERNATE_LOWER) {
    offset = -10 * ((param->bandwidth == 20) ? 2 : 1);
  }

  return channel_to_frequency(channel) + offset;
}

#define LIST_SORTED
int16 __MTLK_IFUNC
mtlk_get_antenna_gain (tx_limit_t *lim, uint16 channel)
{
  /* if we can assume list of channels in antenna gain
     table is sorted - we can use binary search algorithm
     if channel not found, low points to the place where it
     should be inserted.
   */
  uint16 freq = channel_to_frequency(channel);
#ifdef LIST_SORTED
  size_t low = 0, high, mid;

  if (!lim || !lim->gain) {
    ILOG3_V("No limit table found -> antenna gain = 0");
    return 0;
  }

  high = lim->num_gains;
  while (low <= high) {
    mid = (low + high) / 2;
    if (lim->gain[mid].freq > freq) {
      high = mid - 1;
    } else if (lim->gain[mid].freq < freq) {
      low = mid + 1;
    } else
      return lim->gain[mid].gain;
  }

  return lim->gain[low].gain;
#else
  size_t i = 0;

  if (!lim->gain)
    return 0;

  for (; i < lim->num_gains; i++) {
    if (channel == lim->gain[i].freq)
      return lim->gain[i].gain;
  }
  /*TODO: if gain for frequency is not specified -
          select closest neighbor
   */
  return 0;
#endif
}

static __INLINE int16
get_hw_lim (tx_limit_t *lim, drv_params_t *param, uint16 channel)
{
  return get_from_hw_table(lim->hw_lim, param->reg_domain,
    mtlk_calc_start_freq(param, channel), param->spectrum_mode,
    param->band == MTLK_HW_BAND_5_2_GHZ ? 1 : 0);
}

static int16
get_reg_lim (tx_limit_t *lim, drv_params_t *param, uint16 channel, uint8 antennas)
{
  uint16 ch_alt = channel;
  int8 mitigation_p;
  int8 mitigation_a;
  int16 reg_lim_p = get_from_reg_table(lim->reg_lim, param->reg_domain, channel, (uint8*)&mitigation_p);
  int16 reg_lim_a;
  int16 log = get_log_value(antennas);

  /* if we can't find regulatory limit for primary channel, something goes wrong:
     - invalid channel number;
     - invalid regulatory domain code.
     So, we simply return 0, which means "no limits".
   */

  if (reg_lim_p == 0)
    return 0;

  if (param->upper_lower == ALTERNATE_UPPER) {
    ch_alt = channel + 4;
  } else if (param->upper_lower == ALTERNATE_LOWER) {
    ch_alt = channel - 4;
  }

  ILOG3_DD("Channel = %d, alternate channel = %d", channel, ch_alt);

  if (ch_alt != channel) {
    reg_lim_a = get_from_reg_table(lim->reg_lim, param->reg_domain, ch_alt, (uint8*)&mitigation_a);
    ILOG3_DD("reg_lim_p = %d, reg_lim_a = %d", reg_lim_p, reg_lim_a);
    if (!reg_lim_a)
      return reg_lim_p - mitigation_p - log;
    return (reg_lim_p < reg_lim_a ? reg_lim_p - mitigation_p : reg_lim_a - mitigation_a) - log;
  }

  return reg_lim_p - mitigation_p - log;
}

int16 __MTLK_IFUNC
mtlk_calc_tx_power_lim (tx_limit_t *lim, uint16 channel,
    uint8 reg_domain, uint8 spectrum_mode, int8 upper_lower,
    uint8 num_antennas)
{
  drv_params_t params;
  int16 hw_lim = 0;
  int16 reg_lim = 0;

  if (!lim) {
    ILOG3_V("No tables - tx limit -> 0");
    return 0;
  }

  params.band = channel_to_band(channel);
  params.bandwidth = ((spectrum_mode == 1) ? 40 : 20);
  params.upper_lower = (uint8) ((spectrum_mode == 1) ? upper_lower : ALTERNATE_NONE);
  params.reg_domain = reg_domain;
  params.spectrum_mode = spectrum_mode;

  ILOG3_DDDDD("Calculating Tx power limit for ch=%d, reg_dom=%d, spectrum=%d, upper=%d, bw=%d",
      channel, reg_domain, spectrum_mode, params.upper_lower, params.bandwidth);

  hw_lim = get_hw_lim(lim, &params, channel);
  reg_lim = get_reg_lim(lim, &params, channel, num_antennas) << 3;

  ILOG3_DD("hw_lim = %u, reg_lim = %u", hw_lim, reg_lim);

  return (hw_lim < reg_lim ? hw_lim : reg_lim);
}

static struct hw_reg_tx_limit *
find_hw_domain (tx_limit_t *lim, uint8 reg_domain)
{
  struct hw_reg_tx_limit *tmp = lim->hw_lim;

  ILOG3_DPP("Looking for domain data: dom = 0x%x in lim = %p, tmp = %p", reg_domain, lim, tmp);

  while (tmp) {
    ILOG3_D("reg_domain = 0x%x", tmp->reg_domain);
    if (tmp->reg_domain == reg_domain)
      return tmp;

    tmp = tmp->next;
  }

  return NULL;
}

static int
update_hw_freq_entry(struct hw_tx_limit *el, struct hw_rc_tx_limit *lim)
{
  struct hw_tx_limit *p = NULL;
  int i;

  for (i = 0; i < lim->num_freq; i++)
    if ((lim->tx_lim[i].freq == el->freq) && (lim->tx_lim[i].spectrum == el->spectrum)) {
      p = &lim->tx_lim[i];
      break;
    }

  if (p && (el->tx_lim != -1)) /* update */
    p->tx_lim = el->tx_lim;
  else if (p && (el->tx_lim == -1)) { /* remove */
    memmove(p, p + 1, (lim->tx_lim + lim->num_freq - (p + 1))*sizeof(*p));
    lim->num_freq--;
  } else if (!p && (el->tx_lim != -1)) { /* add */
    p = mtlk_osal_mem_alloc((lim->num_freq + 1) * sizeof(struct hw_tx_limit), MTLK_MEM_TAG_REG_HW_LIMITS);
    if (p == NULL)
      return MTLK_ERR_NO_MEM;
    if (lim->tx_lim) {
      memcpy(p, lim->tx_lim, lim->num_freq * sizeof(struct hw_tx_limit));
      mtlk_osal_mem_free(lim->tx_lim);
    }
    lim->tx_lim = p;
    lim->tx_lim[lim->num_freq] = *el;
    lim->num_freq++;
  }

  return MTLK_ERR_OK;
}

static int
load_single_hw_limit (tx_limit_t *lim, uint8 reg_domain, struct hw_tx_limit *tmp_lim)
{
  struct hw_reg_tx_limit *hw_lim;
  struct hw_rc_tx_limit *tx_lim;

  ILOG3_DDDD("Loading HW data for 0x%x domain, limit %d freq %d spectrum %d", reg_domain,
    tmp_lim->tx_lim, tmp_lim->freq, tmp_lim->spectrum);

  tmp_lim->spectrum = tmp_lim->spectrum == 20 ? 0 : 1;

  hw_lim = find_hw_domain(lim, reg_domain);
  if (!hw_lim) {
    ILOG3_V("Domain data not found: create hw_lim");
    hw_lim = mtlk_osal_mem_alloc(sizeof(struct hw_reg_tx_limit), MTLK_MEM_TAG_REG_HW_LIMIT);
    if (!hw_lim)
      return MTLK_ERR_NO_MEM;
    memset(hw_lim, 0, sizeof(*hw_lim));
    /* add to list if not found */
    hw_lim->next = lim->hw_lim;
    lim->hw_lim = hw_lim;
    hw_lim->reg_domain = reg_domain;
    hw_lim->num_classes = 1;
    hw_lim->tx_lim = mtlk_osal_mem_alloc(sizeof(struct hw_rc_tx_limit), MTLK_MEM_TAG_REG_RC_LIMIT);
    memset(hw_lim->tx_lim, 0, sizeof(struct hw_rc_tx_limit));
  }
  tx_lim = hw_lim->tx_lim;
  update_hw_freq_entry(tmp_lim, tx_lim);
  return MTLK_ERR_OK;
}

int __MTLK_IFUNC
mtlk_set_hw_limit (tx_limit_t *lim, mtlk_hw_cfg_t *cfg)
{
  uint8 reg_domain;
  struct hw_tx_limit tmp_lim;
  int i = 0, result = MTLK_ERR_OK;

  MTLK_ASSERT(NULL != lim);
  MTLK_ASSERT(NULL != cfg);

  /* get domain */
  reg_domain = 0;
  while (all_domains[i].dom) {
    /* check name */
    if (!strcmp(cfg->buf, all_domains[i].name)) {
      reg_domain = all_domains[i].reg_domain;
      break;
    }
    /* check alias */
    if (!strcmp(cfg->buf, all_domains[i].alias)) {
      reg_domain = all_domains[i].reg_domain;
      break;
    }
    i++;
  }
  if (0 == reg_domain) {
    ELOG_S("Unknown regulatory domain: %s", cfg->buf);
    result = MTLK_ERR_PARAMS;
    goto FINISH;
  }
  ILOG2_SD("Domain is %s (0x%02x)", cfg->buf, reg_domain);
  tmp_lim.freq = cfg->field_01;
  tmp_lim.spectrum = cfg->field_02;
  tmp_lim.tx_lim = cfg->field_03;
  result = load_single_hw_limit(lim, reg_domain, &tmp_lim);

FINISH:
  return result;
}

static void load_default_hw_table (tx_limit_t *lim, uint16 vendor_id, uint16 device_id,
  uint8 hw_type, uint8 hw_revision)
{
  const struct hw_lim_table_entry *hw_lim_table;
  struct hw_tx_limit tmp_lim;
  uint8 hw_ant_cfg = 0;
  int i;

  lim->vendor_id = vendor_id;
  lim->device_id = device_id;
  lim->hw_type = hw_type;
  lim->hw_revision = hw_revision;
  /* now load default hw limits */
  hw_lim_table = NULL;
  ILOG3_DDDD("Loading HW limits for 0x%04x:0x%04x, type 0x%02x rev 0x%02x",
    vendor_id, device_id, hw_type, hw_revision);
  for (i = 0; hw_lim_lookup_table[i].vendor_id != 0x0000 ; i++) {
    if (hw_lim_lookup_table[i].vendor_id != vendor_id)
      continue;
    if (hw_lim_lookup_table[i].device_id != device_id)
      continue;
    if (hw_lim_lookup_table[i].hw_type != hw_type)
      continue;
    if (hw_lim_lookup_table[i].hw_revision != hw_revision)
      continue;
    /* found */
    hw_lim_table = hw_lim_lookup_table[i].table;
    hw_ant_cfg = hw_lim_lookup_table[i].hw_ant_cfg;
    break;
  }
  if (!hw_lim_table) {
    WLOG_DDDD("No HW limits is available for this platform "
                  "(0x%04x:0x%04x, type 0x%02x rev 0x%02x)",
                  vendor_id, device_id, hw_type, hw_revision);
    return;
  }

  lim->num_tx_antennas = RDLIM_TX_ANT_NUM(hw_ant_cfg);
  lim->num_rx_antennas = RDLIM_RX_ANT_NUM(hw_ant_cfg);
  ILOG0_DD("HW configuration is %dX%d", 
    (int) lim->num_tx_antennas, (int) lim->num_rx_antennas);

  /* table was found, load it */
  for (i = 0; hw_lim_table[i].reg_dom != 0; i++) {
    tmp_lim.tx_lim = hw_lim_table[i].limit;
    tmp_lim.freq = hw_lim_table[i].freq;
    tmp_lim.spectrum = hw_lim_table[i].spacing;
    load_single_hw_limit(lim, hw_lim_table[i].reg_dom, &tmp_lim);
  }
}

int __MTLK_IFUNC
mtlk_init_tx_limit_tables (tx_limit_t *lim, uint16 vendor_id, uint16 device_id,
  uint8 hw_type, uint8 hw_revision)
{
  const struct reg_domain_t *dom = NULL;
  struct reg_tx_limit *reg_lim;
  struct reg_tx_limit *head = NULL;
  struct reg_domain_tx_limit *rdom;
  int idx = 0;
  uint8 i, j, k;
  int res = MTLK_ERR_OK;

  ILOG3_V("Init Tx power limit tables.");

  lim->num_gains = 0;
  lim->gain = NULL;
  /* HW tables are left emply until data will be loaded from DM */
  lim->hw_lim = NULL;

  ILOG3_V("Build regulatory limit table...");
  while (all_domains[idx].dom) {
    dom = all_domains[idx].dom;
    ILOG3_DD("idx = %d, domain = 0x%x", (int)idx, all_domains[idx].reg_domain);
    reg_lim = mtlk_osal_mem_alloc(sizeof(struct reg_tx_limit), MTLK_MEM_TAG_REG_LIMIT);
    if (!reg_lim) {
      res = MTLK_ERR_NO_MEM;
      ELOG_V("Can't allocate memory for reg_lim");
      goto FINISH;
    }

    memset(reg_lim, 0, sizeof(*reg_lim));

    /* add reg_lim to list of regulatory domains. */
    reg_lim->next = head;
    head = reg_lim;

    reg_lim->reg_domain = all_domains[idx].reg_domain;

    for (j = 0; j < all_domains[idx].num_reg_class_sets; j ++) {
      ILOG3_D("dom->num_classes = %d", dom->num_classes);
      rdom = mtlk_osal_mem_alloc(sizeof(struct reg_domain_tx_limit), MTLK_MEM_TAG_REG_DOMAIN);
      if (!rdom) {
        res = MTLK_ERR_NO_MEM;
        ILOG3_V("Can't allocate memory for rdom");
        goto FINISH;
      }

      memset(rdom, 0, sizeof(*rdom));

      /* add rdom to list of domains */
      rdom->next = reg_lim->dom_lim;
      reg_lim->dom_lim = rdom;
      ILOG3_PPP("rdom = %p, rdom->next = %p, reg_lim->dom_lim = %p", rdom, rdom->next, reg_lim->dom_lim);

      rdom->num_classes = dom->num_classes;
      rdom->tx_lim = mtlk_osal_mem_alloc(rdom->num_classes * sizeof(struct reg_class_tx_limit), MTLK_MEM_TAG_REG_CLASS);
      if (!rdom->tx_lim) {
        res = MTLK_ERR_NO_MEM;
        ELOG_V("Can't allocate memory for rdom->tx_lim");
        goto FINISH;
      }

      memset(rdom->tx_lim, 0, rdom->num_classes * sizeof(*(rdom->tx_lim)));

      for (i = 0; i < dom->num_classes; i++) {
        ILOG3_D("reg_class = %d", dom->classes[i].reg_class);
//        rdom->tx_lim[i].tx_lim = dom->classes[i].max_power;
        rdom->tx_lim[i].reg_class = dom->classes[i].reg_class;
        rdom->tx_lim[i].spectrum = dom->classes[i].spacing;
//        rdom->tx_lim[i].mitigation = dom->classes[i].mitigation;
        rdom->tx_lim[i].num_ch = dom->classes[i].num_channels;
        rdom->tx_lim[i].channels = mtlk_osal_mem_alloc(rdom->tx_lim[i].num_ch * sizeof(struct channel_tx_limit), MTLK_MEM_TAG_REG_CHANNEL);
        if (!rdom->tx_lim[i].channels) {
          res = MTLK_ERR_NO_MEM;
          ELOG_D("Can't allocate memory for rdom->tx_lim[%d].channels", i);
          goto FINISH;
        }
//        memcpy(rdom->tx_lim[i].channels, dom->classes[i].channels, rdom->tx_lim[i].num_ch);
        for (k = 0; k < rdom->tx_lim[i].num_ch; k++) {
          rdom->tx_lim[i].channels[k].channel = dom->classes[i].channels[k];
          rdom->tx_lim[i].channels[k].tx_lim = dom->classes[i].max_power;
          rdom->tx_lim[i].channels[k].mitigation = dom->classes[i].mitigation;
        }
      }
      dom++;
    }

    idx++;
  }

  ILOG3_V("Tx power limit tables initialized.");
FINISH:
  lim->reg_lim = head;
  load_default_hw_table(lim, vendor_id, device_id, hw_type, hw_revision);
  return res;
}

static void
cleanup_reg_dom_lim (struct reg_domain_tx_limit *reg_lim)
{
  uint8 i;
  struct reg_domain_tx_limit *tmp;

  while (reg_lim) {
    if (reg_lim->tx_lim) {
      for (i = 0; i < reg_lim->num_classes; i++) {
        ILOG3_D("Cleanup reg_class = %d", reg_lim->tx_lim[i].reg_class);
        if (reg_lim->tx_lim[i].channels)
          mtlk_osal_mem_free(reg_lim->tx_lim[i].channels);
      }
      mtlk_osal_mem_free(reg_lim->tx_lim);
    }

    tmp = reg_lim;
    reg_lim = reg_lim->next;

    mtlk_osal_mem_free(tmp);
  }
}

static void
cleanup_reg_lim(struct reg_tx_limit *reg_lim)
{
  struct reg_tx_limit *tmp;

  while (reg_lim) {
    ILOG3_D("Cleanup regulatory limit: reg_domain = 0x%x", reg_lim->reg_domain);
    cleanup_reg_dom_lim(reg_lim->dom_lim);

    tmp = reg_lim;
    reg_lim = reg_lim->next;

    mtlk_osal_mem_free(tmp);
  }
}

static void
cleanup_hw_lim (struct hw_reg_tx_limit *hw_lim)
{
  uint8 i;
  struct hw_reg_tx_limit *tmp;

  while (hw_lim) {
    if (hw_lim->tx_lim) {
      ILOG3_D("Cleanup HW limit: reg_domain = 0x%x", hw_lim->reg_domain);
      for (i = 0; i < hw_lim->num_classes; i++) {
        if (hw_lim->tx_lim[i].tx_lim)
          mtlk_osal_mem_free(hw_lim->tx_lim[i].tx_lim);
      }
      mtlk_osal_mem_free(hw_lim->tx_lim);
    }

    tmp = hw_lim;
    hw_lim = hw_lim->next;

    mtlk_osal_mem_free(tmp);
  }
}

int __MTLK_IFUNC
mtlk_cleanup_tx_limit_tables (tx_limit_t *lim)
{
  ILOG3_V("Cleanup regulatory limit tables.");
  cleanup_reg_lim(lim->reg_lim);

  ILOG3_V("Cleanup HW specific limit tables.");
  cleanup_hw_lim(lim->hw_lim);

  lim->reg_lim = NULL;
  lim->hw_lim  = NULL;
  ILOG3_V("Limit tables clean");

  if(NULL != lim->gain)
  {
    mtlk_osal_mem_free(lim->gain);
    lim->gain = NULL;
  }

  return MTLK_ERR_OK;
}

int __MTLK_IFUNC
mtlk_reset_tx_limit_tables (tx_limit_t *lim)
{
  mtlk_cleanup_tx_limit_tables(lim);
  return mtlk_init_tx_limit_tables(lim, lim->vendor_id, lim->device_id,
    lim->hw_type, lim->hw_revision);
}

static struct reg_tx_limit *
find_reg_domain (tx_limit_t *lim, uint8 reg_domain)
{
  struct reg_tx_limit *tmp = lim->reg_lim;

  while (tmp) {
    if (tmp->reg_domain == reg_domain)
      return tmp;

    tmp = tmp->next;
  }

  return NULL;
}

static void
update_reg_channel (uint8 channel, int16 tx_lim, uint8 mitigation, struct reg_domain_tx_limit *lim)
{
  uint8 i, j;

  while (lim) {
    for (i = 0; i < lim->num_classes; i++) {
      for (j = 0; j < lim->tx_lim[i].num_ch; j++) {
        if (lim->tx_lim[i].channels[j].channel == channel) {
          ILOG5_D("Updating channel: %d", channel);
          lim->tx_lim[i].channels[j].tx_lim = tx_lim;
          if (channel_to_band(channel) == MTLK_HW_BAND_5_2_GHZ)
            lim->tx_lim[i].channels[j].mitigation = mitigation;
        }
      }
    }

    lim = lim->next;
  }

}

int __MTLK_IFUNC
mtlk_update_reg_limit_table (mtlk_handle_t handle, struct country_ie_t *ie, int8 power_constraint)
{
  uint8 count = 0;
  char buf[3];
  tx_limit_t *lim = (tx_limit_t*)mtlk_core_get_tx_limits_handle(handle);
  struct reg_tx_limit *reg_lim = NULL;
  //struct reg_class_tx_limit rlim;
  uint8 reg_domain = 0;
  uint8 i, j, step;

  if (!ie)
    return MTLK_ERR_PARAMS;
  memcpy(buf, ie->country, 2);
  buf[2] = 0;
  ILOG3_S("buf = '%s'", buf);
  reg_domain = country_to_domain(buf);

  if (!reg_domain) {
    ILOG4_D("Illegal domain: 0x%x", reg_domain);
    return MTLK_ERR_PARAMS;
  }

  ILOG1_D("Updating reg_domain = 0x%x", reg_domain);
  reg_lim = find_reg_domain(lim, reg_domain);

  if (!reg_lim) {
    reg_lim = mtlk_osal_mem_alloc(sizeof(struct reg_tx_limit), MTLK_MEM_TAG_REG_LIMIT);
    if (!reg_lim) {
      return MTLK_ERR_NO_MEM;
    }

    memset(reg_lim, 0, sizeof(*reg_lim));

    /* add to list if not found */
    reg_lim->next = lim->reg_lim;
    lim->reg_lim = reg_lim;

    reg_lim->reg_domain = reg_domain;
    reg_lim->dom_lim = mtlk_osal_mem_alloc(sizeof(struct reg_domain_tx_limit), MTLK_MEM_TAG_REG_DOMAIN);
    if (!reg_lim->dom_lim) {
      return MTLK_ERR_NO_MEM;
    }

    memset(reg_lim->dom_lim, 0, sizeof(*(reg_lim->dom_lim)));
  }

  count = ie->length - sizeof(ie->country);
  count /= sizeof(struct country_constr_t);
  ILOG3_D("Element has %d entries", count);

  for (i = 0; i < count; i++) {
    if (ie->constr[i].first_ch < 201) {
      ILOG3_DDD("Element entry: f_ch = %d, num_ch = %d, tx_lim = %d",
        ie->constr[i].first_ch, ie->constr[i].num_ch, ie->constr[i].max_power);
      if (channel_to_band(ie->constr[i].first_ch) == MTLK_HW_BAND_2_4_GHZ)
        step = 1;
      else
        step = 4;

      for (j = 0; j < ie->constr[i].num_ch; j++) {
        update_reg_channel(ie->constr[i].first_ch + j * step, ie->constr[i].max_power, power_constraint, reg_lim->dom_lim);
      }

    } else {
      WLOG_V("Regulatory extension doesn't supported!");
      /* TODO: add support for regulatory classes */
    }
  }

  return MTLK_ERR_OK;
}

int __MTLK_IFUNC
mtlk_set_ant_gain (tx_limit_t *lim, mtlk_ant_cfg_t *cfg)
{
  struct antenna_gain *p = NULL;
  int32  gain, freq;
  int i;

  MTLK_ASSERT(NULL != lim);
  MTLK_ASSERT(NULL != cfg);

  freq = cfg->field_01;
  gain = cfg->field_02;

  for (i = 0; i < lim->num_gains; i++)
    if (lim->gain[i].freq == freq) {
      p = &lim->gain[i];
      break;
    }

  if (p && (gain != -1)) /* update */
    p->gain = gain;
  else if (p && (gain == -1)) { /* remove */
    memmove(p, p + 1, lim->gain + lim->num_gains*sizeof(*p) - p);
    lim->num_gains--;
  } else if (!p && (gain != -1)) { /* add */
    p = mtlk_osal_mem_alloc((lim->num_gains + 1) * sizeof(struct antenna_gain), MTLK_MEM_TAG_ANTENNA_GAIN);
    if (p == NULL)
      return MTLK_ERR_NO_MEM;
    if (lim->gain) {
      memcpy(p, lim->gain, lim->num_gains * sizeof(struct antenna_gain));
      mtlk_osal_mem_free(lim->gain);
    }
    lim->gain = p;
    lim->gain[lim->num_gains].freq = (uint16)freq;
    lim->gain[lim->num_gains].gain = (uint8)gain;
    lim->num_gains++;
  }
  return MTLK_ERR_OK;
}

int __MTLK_IFUNC
mtlk_set_power_limit (mtlk_core_t *core, mtlk_tx_power_limit_cfg_t *cfg)
{
  mtlk_txmm_msg_t     man_msg;
  mtlk_txmm_data_t*   man_entry = NULL;
  UMI_TX_POWER_LIMIT *mac_msg;
  int                 res = MTLK_ERR_OK;

  MTLK_ASSERT(core != NULL);

  if (mtlk_core_scan_is_running(core)) {
    ELOG_D("CID-%04x: Cannot set TX POWER limit while scan is running", mtlk_vap_get_oid(core->vap_handle));
    return MTLK_ERR_BUSY;
  }

  ILOG4_DD("CID-%04x: UM_MAN_CHANGE_TX_POWER_LIMIT_REQ TxPowerLimitOption %u", mtlk_vap_get_oid(core->vap_handle), cfg->field_01);

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, mtlk_vap_get_txmm(core->vap_handle), &res);
  if (man_entry == NULL)
  {
    ELOG_D("CID-%04x: Can't send UM_MAN_CHANGE_TX_POWER_LIMIT_REQ due to the lack of MAN_MSG", mtlk_vap_get_oid(core->vap_handle));
    return res;
  }

  man_entry->id           = UM_MAN_CHANGE_TX_POWER_LIMIT_REQ;
  man_entry->payload_size = sizeof(UMI_TX_POWER_LIMIT);
  mac_msg = (UMI_TX_POWER_LIMIT *)man_entry->payload;

  memset(mac_msg, 0, sizeof(*mac_msg));

  mac_msg->TxPowerLimitOption = cfg->field_01;

  mtlk_dump(3, man_entry->payload, sizeof(UMI_TX_POWER_LIMIT), "UM_MAN_CHANGE_TX_POWER_LIMIT_REQ dump:");

  res = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);
  if (res != MTLK_ERR_OK) {
    ELOG_D("Error sending UM_MAN_CHANGE_TX_POWER_LIMIT_REQ (%d)", res);
    goto FINISH;
  }

  ILOG2_DD("UM_MAN_CHANGE_TX_POWER_LIMIT_REQ: Limit(%u) Status(%u)", (int)cfg->field_01, (int)mac_msg->Status);
  if (mac_msg->Status != UMI_OK) {
    ELOG_DD("FW refused to set Power Limit (%u) with status (%u)", (int)cfg->field_01, (int)mac_msg->Status);
    res = MTLK_ERR_FW;
    goto FINISH;
  }

FINISH:
  if (man_entry) {
    mtlk_txmm_msg_cleanup(&man_msg);
  }

  return res;
}

/* HW specific limits. */
int __MTLK_IFUNC
mtlk_channels_get_hw_limits(tx_limit_t *lim, mtlk_clpb_t *clpb)
{
  uint8 i, j;
  struct hw_reg_tx_limit *hw_lim;
  mtlk_hw_limits_stat_entry_t stat_entry;
  int res = MTLK_ERR_UNKNOWN;

  MTLK_ASSERT(NULL != lim);
  MTLK_ASSERT(NULL != clpb);

  /* create returned data */
  hw_lim = lim->hw_lim;
  while (hw_lim) {
    if (hw_lim->tx_lim) {
      for (i = 0; i < hw_lim->num_classes; i++) {
        if (hw_lim->tx_lim[i].tx_lim) {
          for (j = 0; j < hw_lim->tx_lim[i].num_freq; j++) {
            memset(&stat_entry, 0, sizeof(stat_entry));
            stat_entry.freq = hw_lim->tx_lim[i].tx_lim[j].freq;
            stat_entry.spectrum = hw_lim->tx_lim[i].tx_lim[j].spectrum == 0 ? 20 : 40;
            stat_entry.tx_lim = hw_lim->tx_lim[i].tx_lim[j].tx_lim;
            stat_entry.reg_domain = hw_lim->reg_domain;

            if (MTLK_ERR_OK != (res = mtlk_clpb_push(clpb, &stat_entry, sizeof(stat_entry)))) {
              goto err_push;
            }
          }
        }
      }
    }
    hw_lim = hw_lim->next;
  }
  return MTLK_ERR_OK;

err_push:
  mtlk_clpb_purge(clpb);
  return res;
}

/* regulatory domain limits. */
int mtlk_channels_get_reg_limits(tx_limit_t *lim, mtlk_clpb_t *clpb)
{
  uint8 i, j;
  struct reg_tx_limit *reg_lim = lim->reg_lim;
  struct reg_domain_tx_limit *dom_lim;
  mtlk_reg_limits_stat_entry_t stat_entry;
  int res = MTLK_ERR_UNKNOWN;

  MTLK_ASSERT(NULL != lim);
  MTLK_ASSERT(NULL != clpb);

  /* create returned data */
  reg_lim = lim->reg_lim;
  while (reg_lim) {
    dom_lim = reg_lim->dom_lim;
    while (dom_lim) {
      if (dom_lim->tx_lim) {
        for (i = 0; i < dom_lim->num_classes; i++) {
          if (dom_lim->tx_lim[i].channels) {
            for (j = 0; j < dom_lim->tx_lim[i].num_ch; j++) {
              memset(&stat_entry, 0, sizeof(stat_entry));
              stat_entry.tx_lim = dom_lim->tx_lim[i].channels[j].tx_lim;
              stat_entry.reg_domain = reg_lim->reg_domain;
              stat_entry.reg_class = dom_lim->tx_lim[i].reg_class;
              stat_entry.spectrum = dom_lim->tx_lim[i].spectrum;
              stat_entry.channel = dom_lim->tx_lim[i].channels[j].channel;
              stat_entry.mitigation = dom_lim->tx_lim[i].channels[j].mitigation;

              if (MTLK_ERR_OK != (res = mtlk_clpb_push(clpb, &stat_entry, sizeof(stat_entry)))) {
                goto err_push;
              }
            }
          }
        }
      }
      dom_lim = dom_lim->next;
    }
    reg_lim = reg_lim->next;
  }
  return MTLK_ERR_OK;

err_push:
  mtlk_clpb_purge(clpb);
  return res;
}

/* Antenna Gain. */
int __MTLK_IFUNC
mtlk_channels_get_ant_gain(tx_limit_t *lim, mtlk_clpb_t *clpb)
{
  uint8 i;
  mtlk_ant_gain_stat_entry_t stat_entry;
  int res = MTLK_ERR_UNKNOWN;

  ILOG2_D("tx_limits->num_gains = %u", lim->num_gains);

  for (i = 0; i < lim->num_gains; i++) {
    memset(&stat_entry, 0, sizeof(stat_entry));
    stat_entry.freq = lim->gain[i].freq;
    stat_entry.gain = lim->gain[i].gain;

    if (MTLK_ERR_OK != (res = mtlk_clpb_push(clpb, &stat_entry, sizeof(stat_entry)))) {
      goto err_push;
    }
  }
  return MTLK_ERR_OK;

err_push:
  mtlk_clpb_purge(clpb);
  return res;
}

uint8 __MTLK_IFUNC
mtlk_get_channel_mitigation(uint8 reg_domain,
                      uint8 is_ht,
                      uint8 spectrum_mode,
                      uint16 channel)
{
  const struct reg_domain_t *domain = NULL;
  const struct reg_class_t * cls;
  uint16 i, j, m, num_of_protocols_in_domains = 0;
  uint8 frequency_band = channel_to_band(channel);
  int result = MTLK_ERR_OK;

  ILOG3_DDDD("Looking for mitigation for: ch = %d, reg_dom = %d, ht = %d, spectrum = %d",
    channel, reg_domain, is_ht, spectrum_mode);

  domain = mtlk_get_domain(reg_domain, &result, &num_of_protocols_in_domains,
    0, MTLK_CHNLS_COUNTRY_CALLER);

  if (result != MTLK_ERR_OK) {
    return 0;
  }

  for (m = 0; m < num_of_protocols_in_domains; m++) {
    for (i = 0; i < domain[m].num_classes; i++) {
      if (m == MAKE_PROTOCOL_INDEX(is_ht, frequency_band)) {
        cls = &domain[m].classes[i];
        for (j = 0; j < cls->num_channels; j++) {
          if (channel == cls->channels[j]) {
            if ((spectrum_mode == 1 && cls->spacing == 40) ||
                (spectrum_mode == 0 && cls->spacing == 20)) {
              ILOG3_DD("channel = %d, mitigation = %d", channel, cls->mitigation);
              return cls->mitigation;
            }
          }
        }
      }/*if (get_data)*/
    }/*for(i..*/
  }/*for (m..*/

  return 0;
}

int __MTLK_IFUNC
mtlk_get_avail_channels(mtlk_get_channel_data_t *param, uint8 *channels)
{
  const struct reg_domain_t *domain = NULL;
  int result;
  uint16 num_protocols = 0;
  uint8 upper_lower = 0;
  int protocol;
  int cls_idx;

  if ((!param->bonding && (SPECTRUM_40MHZ == param->spectrum_mode)) ||
      (SPECTRUM_20MHZ == param->spectrum_mode/*if nCB upper/lower not relevant*/)){
    upper_lower = 1;
  }

  ILOG3_DDDD("Looking for availiable channels for reg. domain %d, band %d, bonding %d, spectrum %d", 
    param->reg_domain, param->frequency_band, param->bonding, param->spectrum_mode);

  domain = mtlk_get_domain(param->reg_domain, &result, &num_protocols, upper_lower, MTLK_CHNLS_DOT11H_CALLER);

  if (result != MTLK_ERR_OK) {
    ILOG2_V("Error getting domain");
    return MTLK_ERR_PARAMS;
  }

  result = 0;

  for (protocol = 0; protocol < num_protocols; protocol++) {
    if (MAKE_PROTOCOL_INDEX(param->is_ht, param->frequency_band) != protocol)
      continue;

    for (cls_idx = 0; cls_idx < domain[protocol].num_classes; cls_idx++) {
      const struct reg_class_t *cls = &domain[protocol].classes[cls_idx];
      int k;
      if (cls->sm_required && param->disable_sm_channels)
        continue;
      if ((MTLK_HW_BAND_2_4_GHZ == param->frequency_band) && (24070 != cls->start_freq))
        continue;
      if ((MTLK_HW_BAND_5_2_GHZ == param->frequency_band) && (50000 != cls->start_freq))
        continue;
      if ((SPECTRUM_20MHZ == param->spectrum_mode) && (20 != cls->spacing))
        continue;
      if ((SPECTRUM_40MHZ == param->spectrum_mode) && (40 != cls->spacing))
        continue;

      for (k = 0; k < cls->num_channels; k++) {
        int n;
        channels[result++] = cls->channels[k];
        for(n=0; n < result - 1; n++) {
          /* check for duplications */
          if (channels[n] == cls->channels[k]) {
            result--;
            break;
          }
        }
      }
    }
  }

  return result;
}

int __MTLK_IFUNC
mtlk_check_channel(mtlk_get_channel_data_t *param, uint8 channel)
{
  int result = MTLK_ERR_NOT_SUPPORTED;
  uint8 channels[MAX_CHANNELS];
  int num_channels = 0;
  int i;

  num_channels = mtlk_get_avail_channels(param, channels);

  if (0 < num_channels) {
    for (i = 0; i < num_channels; i++) {
      if (i >= MAX_CHANNELS)
        break;
      if (channel == channels[i]) {
        result = MTLK_ERR_OK;
        break;
      }
    }
  }

  return result;
}

static int
check_overlapping_range (MIB_CHANNELS_TYPE_ENTRY *ent, uint16 *count, uint16 max_entries, const struct reg_class_t *cls)
{
  uint16 i;
  uint8 first_channel = cls->channels[0];
  uint8 num_channels = cls->num_channels;
  uint8 last_channel = cls->channels[cls->num_channels - 1];
  uint8 ent_last;
  uint8 step;

  MTLK_UNREFERENCED_PARAM(max_entries);

  for (i = 0; i < *count; i++) {
    if (channel_to_band(ent[i].u8FirstChannelNumber) == MTLK_HW_BAND_2_4_GHZ)
      step = 1;
    else
      step = 4;

    ent_last = ent[i].u8FirstChannelNumber + step * (ent[i].u8NumberOfChannels - 1);
    ILOG4_D("ent_last = %d", ent_last);

    if (first_channel >= ent[i].u8FirstChannelNumber &&
        last_channel <= ent_last) {
      ILOG4_V("Entry fully enclosed into existing range - skipping");
      return MTLK_ERR_OK;
    }

    if (first_channel < ent[i].u8FirstChannelNumber &&
        last_channel > ent_last) {
      ILOG4_DDD("1. Edit entry: old (%d:%d:%d)",
        ent[i].u8FirstChannelNumber, ent[i].u8NumberOfChannels, ent[i].u8MaximumTransmitPowerLevel);
      ent[i].u8FirstChannelNumber = first_channel;
      ent[i].u8NumberOfChannels = num_channels;
      ILOG4_DDD("1. Edit entry: new (%d:%d:%d)",
        ent[i].u8FirstChannelNumber, ent[i].u8NumberOfChannels, ent[i].u8MaximumTransmitPowerLevel);
      return MTLK_ERR_OK;
    }

    if (first_channel < ent[i].u8FirstChannelNumber &&
        last_channel < ent_last && last_channel >= ent[i].u8FirstChannelNumber) {
      ILOG4_DDD("2. Edit entry: old (%d:%d:%d)",
        ent[i].u8FirstChannelNumber, ent[i].u8NumberOfChannels, ent[i].u8MaximumTransmitPowerLevel);
      ent[i].u8NumberOfChannels += (ent[i].u8FirstChannelNumber - first_channel) / step;
      ent[i].u8FirstChannelNumber = first_channel;
      ILOG4_DDD("2. Edit entry: new (%d:%d:%d)",
        ent[i].u8FirstChannelNumber, ent[i].u8NumberOfChannels, ent[i].u8MaximumTransmitPowerLevel);
      return MTLK_ERR_OK;
    }

    if (first_channel > ent[i].u8FirstChannelNumber &&
        first_channel <= ent_last && last_channel > ent_last) {
      ILOG4_DDD("3. Edit entry: old (%d:%d:%d)",
        ent[i].u8FirstChannelNumber, ent[i].u8NumberOfChannels, ent[i].u8MaximumTransmitPowerLevel);
      ent[i].u8NumberOfChannels += (last_channel - ent_last) / step;
      ILOG4_DDD("3. Edit entry: new (%d:%d:%d)",
        ent[i].u8FirstChannelNumber, ent[i].u8NumberOfChannels, ent[i].u8MaximumTransmitPowerLevel);
      return MTLK_ERR_OK;
    }
  }

  ent[*count].u8FirstChannelNumber = first_channel;
  ent[*count].u8NumberOfChannels = num_channels;
  ent[*count].u8MaximumTransmitPowerLevel = cls->max_power;
  ILOG4_DDD("Add entry: (%d:%d:%d)",
    ent[i].u8FirstChannelNumber, ent[i].u8NumberOfChannels, ent[i].u8MaximumTransmitPowerLevel);
  (*count)++;

  return MTLK_ERR_OK;
}

static int
fill_country_channels (uint8 reg_domain, uint8 is_ht, uint8 frequency_band,
    MIB_CHANNELS_TYPE *ie, uint16 max_entries, BOOL is_ap)
{
  uint16 num_of_protocols, p;
  int res;
  const struct reg_domain_t *domain = mtlk_get_domain(reg_domain
                                                      ,&res
                                                      ,&num_of_protocols
                                                      ,(uint8)0
                                                      ,MTLK_CHNLS_COUNTRY_CALLER);
  const struct reg_class_t *cls = NULL;
  MIB_CHANNELS_TYPE_ENTRY *ent = ie->Table;
  uint8 i, count = 0, start = 0;
  uint16 cur = 0;

  if (!domain || !ie) {
    return MTLK_ERR_PARAMS;
  }

  ILOG3_S("Configuration: %s", is_ap ? "AP": "STA");

  memset(ent, 0, max_entries);

  for (p = 0; p < num_of_protocols; p ++) {
    ILOG3_PD("Domain = %p, num = %d", &domain[p], domain[p].num_classes);
    if (is_ap) {
      if (p == MAKE_PROTOCOL_INDEX(is_ht, frequency_band)) {
        count = (uint8)MIN(domain[p].num_classes, max_entries);

        for (i = 0; i < count; i++) {
          cls = &domain[p].classes[i];
          check_overlapping_range(ent, &cur, max_entries, cls);
        }
      }
    } else {
      if (IS_HT_PROTOCOL_INDEX(p)) {
        i = count;
        start = count;
        count = count + (uint8)MIN(domain[p].num_classes, max_entries - count);
        ILOG3_DD("i = %d, count = %d", i, count);
        for (; i < count; i++) {
          cls = &domain[p].classes[i - start];
          check_overlapping_range(ent, &cur, max_entries, cls);
        }
      }
    }
  }

  /* set the 4th byte of country to number of entries actually filled */
  ILOG3_D("Filled %d elements", cur);
  ie->Country[3] =(uint8) cur;

#if 0
  ILOG3_D("Country IE: num_elem = %d", cur);
  for (i = 0; i < cur; i++) {
    ILOG3_DDDD("ent[%d]: first = %d, number = %d, power = %d", i, ie->Table[i].u8FirstChannelNumber,
        ie->Table[i].u8NumberOfChannels, ie->Table[i].u8MaximumTransmitPowerLevel);
  }
#endif

  return MTLK_ERR_OK;
}

int __MTLK_IFUNC
mtlk_set_country_mib (mtlk_txmm_t *txmm, 
                      uint8 reg_domain, 
                      uint8 is_ht, 
                      uint8 frequency_band, 
                      BOOL  is_ap, 
                      const char *country,
                      BOOL is_dot11d_active)
{
  MIB_VALUE uValue;
  memset(&uValue, 0, sizeof(MIB_VALUE));

  if (is_dot11d_active || !is_ap) { // Enable country information for AP (for beacons)
                                 // only if dot11d is active.
                                 // Enable country information for STA always.
                                 // FIXME: does MAC really need country information on STA?
    memcpy(uValue.sCountry.Country, country, 2);
 
    uValue.sCountry.Country[2] = ' ';
    ILOG3_S("Set country code: '%s '", country);

    fill_country_channels(reg_domain, is_ht, frequency_band, &uValue.sCountry, MIB_CHANNELS_MAX_GROUPS, is_ap);
  }

  return mtlk_set_mib_value_raw(txmm, MIB_COUNTRY, &uValue);
}

uint8 __MTLK_IFUNC
mtlk_select_reg_domain (uint16 channel)
{
  int i = 0, p, j, k;
  uint8 reg_dom_cur = 0;
  uint8 max_power_cur = 0;
  uint8 band = channel_to_band(channel);
  uint16 num_proto = sizeof(reg_domain_FCC) / sizeof(reg_domain_FCC[0]);

  while (all_domains[i].dom) {
    for (p = 0; p < num_proto; p++) {
      for (j = 0; j < all_domains[i].dom[p].num_classes; j++) {
        if (check_band(&all_domains[i].dom[p].classes[j], band? 24070: 50000) == 0)
          continue;

        for (k = 0; k < all_domains[i].dom[p].classes[j].num_channels; k++) {
          if (channel == all_domains[i].dom[p].classes[j].channels[k]) {
            if (max_power_cur < all_domains[i].dom[p].classes[j].max_power) {
              max_power_cur = all_domains[i].dom[p].classes[j].max_power;
              reg_dom_cur = all_domains[i].reg_domain;
              ILOG4_DD("max_power = %d, reg_domain = 0x%x", max_power_cur, reg_dom_cur);
            }
          }
        }
      }
    }
    
    i++;
  }
  
  ILOG4_D("Selected reg_domain = 0x%x", reg_dom_cur);
  return reg_dom_cur;
}

uint8 __MTLK_IFUNC
mtlk_get_chnl_switch_mode (uint8 spectrum_mode, uint8 bonding, uint8 is_silent_sw)
{
  uint8 switch_mode;

  if (spectrum_mode == SPECTRUM_40MHZ)
    switch_mode = bonding == ALTERNATE_LOWER ? UMI_CHANNEL_SW_MODE_SCB : UMI_CHANNEL_SW_MODE_SCA;
  else
    switch_mode = UMI_CHANNEL_SW_MODE_SCN;
  switch_mode |= is_silent_sw ? UMI_CHANNEL_SW_MODE_SILENT : UMI_CHANNEL_SW_MODE_NORMAL;

  ILOG1_SSSD("Spectrum %s, bonding %s, is silent: %s - 0x%02x",
    spectrum_mode == SPECTRUM_40MHZ ? "40MHz" : "20MHz",
    bonding == ALTERNATE_LOWER ? "lower" : "upper",
    is_silent_sw ? "yes" : "no", switch_mode);

  return switch_mode;
}

BOOL __MTLK_IFUNC
mtlk_channels_does_domain_exist(uint8 reg_domain)
{
  unsigned i;
  for (i = 0; all_domains[i].reg_domain; i++) {
    if (all_domains[i].reg_domain == reg_domain)
      return TRUE;
  }

  return FALSE;
}

uint8 __MTLK_IFUNC
country_code_to_domain (uint8 country_code)
{
  if (country_code < ARRAY_SIZE(country_reg_table))
    return country_reg_table[country_code].domain;
  else
    return 0;
}

const char * __MTLK_IFUNC
country_code_to_country (uint8 country_code)
{
  if (country_code_to_domain(country_code))
    return country_reg_table[country_code].country;
  else
    return "??";  // undefined
}

uint8 __MTLK_IFUNC
country_to_country_code (const char *country)
{
  uint8 country_code;

  for (country_code = 0; country_code < ARRAY_SIZE(country_reg_table); country_code++)
    if (strncmp(country_code_to_country(country_code), country, 2) == 0)
      return country_code;

  return 0; // undefied
}

void __MTLK_IFUNC
get_all_countries_for_domain(uint8 domain, mtlk_gen_core_country_name_t *countries, uint32 countries_buffer_size)
{
  uint8 i,j;

  /* To make sure that sizes are synchronized, */
  /* ARRAY_SIZE(country_reg_table) can't be shared directly */
  MTLK_ASSERT(countries_buffer_size == ARRAY_SIZE(country_reg_table));

  /* enumerate countries in current domain */
  for(i=0, j=0; i<countries_buffer_size; i++) {
    if (domain == country_reg_table[i].domain) {
      strcpy(countries[j++].name, country_reg_table[i].country);
    }
  }
}

static void
_mtlk_channels_frequency_element_dump (mtlk_vap_handle_t vap_handle, const FREQUENCY_ELEMENT *fr_el)
{
  int i;

  ILOG0_DD("CID-%04x: Channel        : %u", mtlk_vap_get_oid(vap_handle), MAC_TO_HOST16(fr_el->u16Channel));
  ILOG0_DD("CID-%04x: ChannelACT     : %u", mtlk_vap_get_oid(vap_handle), MAC_TO_HOST16(fr_el->u16ChannelAvailabilityCheckTime));
  ILOG0_DD("CID-%04x: ScanType       : %u", mtlk_vap_get_oid(vap_handle), fr_el->u8ScanType);
  ILOG0_DD("CID-%04x: ChannelSwCnt   : %u", mtlk_vap_get_oid(vap_handle), fr_el->u8ChannelSwitchCount);
  ILOG0_DD("CID-%04x: SwitchMode     : %u", mtlk_vap_get_oid(vap_handle), fr_el->u8SwitchMode);
  ILOG0_DD("CID-%04x: SmRequired     : %u", mtlk_vap_get_oid(vap_handle), fr_el->u8SmRequired);
  ILOG0_DD("CID-%04x: CbTxPowerLim   : %d", mtlk_vap_get_oid(vap_handle), MAC_TO_HOST16(fr_el->i16CbTransmitPowerLimit));
  ILOG0_DD("CID-%04x: nCbTxPowerLim  : %d", mtlk_vap_get_oid(vap_handle), MAC_TO_HOST16(fr_el->i16nCbTransmitPowerLimit));
  ILOG0_DD("CID-%04x: AntennaGain    : %d", mtlk_vap_get_oid(vap_handle), MAC_TO_HOST16(fr_el->i16AntennaGain));
  ILOG0_DD("CID-%04x: ChannelLoad    : %d", mtlk_vap_get_oid(vap_handle), MAC_TO_HOST16(fr_el->u16ChannelLoad));
  for (i = 0; i < MAX_TX_POWER_ELEMENTS; i++) {
    ILOG0_DDD("CID-%04x: MaxTxPwr[%u]    : %u", mtlk_vap_get_oid(vap_handle), i, fr_el->u8MaxTxPower[i]);
    ILOG0_DDD("CID-%04x: MaxTxPwrIdx[%u] : %u", mtlk_vap_get_oid(vap_handle), i, fr_el->u8MaxTxPowerIndex[i]);
  }
  ILOG0_DD("CID-%04x: SwitchType     : %d", mtlk_vap_get_oid(vap_handle), MAC_TO_HOST32(fr_el->u32SwitchType));
}

static void 
_mtlk_channels_fill_mbss_frequency_element_data(mtlk_get_channel_data_t *params,
                                                mtlk_core_t             *core,
                                                uint8                    u8SwitchMode,
                                                FREQUENCY_ELEMENT       *fr_el)
{
  FREQUENCY_ELEMENT cs_cfg_s;
  uint16 pl, ag;

  mtlk_get_channel_data(params, &cs_cfg_s, NULL, NULL);

  fr_el->u16Channel = cpu_to_le16(params->channel);
  fr_el->u16ChannelAvailabilityCheckTime = HOST_TO_MAC16(cs_cfg_s.u16ChannelAvailabilityCheckTime);
  fr_el->u8ScanType = cs_cfg_s.u8ScanType;
  fr_el->u8SwitchMode = u8SwitchMode;
  fr_el->u8ChannelSwitchCount = 0; /*don't care*/
  fr_el->u8SmRequired = cs_cfg_s.u8SmRequired;

  /*TODO- add SmRequired to 11d table !!*/
  if (FALSE == mtlk_pdb_get_int(mtlk_vap_get_param_db(core->vap_handle), PARAM_DB_DFS_RADAR_DETECTION)) {
     fr_el->u8SmRequired = 0; /*no 11h support (by proc)*/
  }
 
  pl = mtlk_calc_tx_power_lim_wrapper(HANDLE_T(core), 0, params->channel);
  fr_el->i16nCbTransmitPowerLimit = HOST_TO_MAC16(pl);
  if (params->spectrum_mode)
      pl = mtlk_calc_tx_power_lim_wrapper(HANDLE_T(core), 1, params->channel);
  fr_el->i16CbTransmitPowerLimit = HOST_TO_MAC16(pl);
  ag = mtlk_get_antenna_gain(mtlk_core_get_tx_limits(core), params->channel);
  fr_el->i16AntennaGain = HOST_TO_MAC16(ag);

  _mtlk_channels_frequency_element_dump(core->vap_handle, fr_el);
}


void __MTLK_IFUNC mtlk_channels_fill_mbss_pre_activate_req_ext_data(mtlk_get_channel_data_t *params,
  mtlk_core_t             *core,
  uint8                    u8SwitchMode,
  void                    *data)
{
  UMI_MBSS_PRE_ACTIVATE *pre_areq = data;

  _mtlk_channels_fill_mbss_frequency_element_data(params, core, u8SwitchMode, &pre_areq->sFrequencyElement);
}

void __MTLK_IFUNC mtlk_channels_fill_activate_req_ext_data(mtlk_get_channel_data_t *params,
                                                           mtlk_core_t             *core,
                                                           uint8                   u8SwitchMode,
                                                           void                    *data)
{
  UMI_ACTIVATE *areq = data;

  _mtlk_channels_fill_mbss_frequency_element_data(params, core, u8SwitchMode, &areq->sFrequencyElement);
}


void __MTLK_IFUNC
mtlk_fill_channel_params_by_tpc_by_vap(mtlk_vap_handle_t vap_handle, FREQUENCY_ELEMENT *params)
{
  mtlk_fill_channel_params_by_tpc(HANDLE_T(mtlk_vap_get_core(vap_handle)), params);
}

void __MTLK_IFUNC
mtlk_fill_channel_params_by_tpc(mtlk_handle_t context, FREQUENCY_ELEMENT *params)
{
    uint8 ant;
    mtlk_eeprom_tpc_data_t *tpc;
    mtlk_eeprom_data_t* eeprom;

    eeprom = mtlk_core_get_eeprom((mtlk_core_t*)context);
    tpc = mtlk_find_closest_freq((uint8)MAC_TO_HOST16(params->u16Channel), eeprom);
    if (tpc) {
      for(ant = 0; ant < mtlk_eeprom_get_num_antennas(eeprom); ant++) {
          params->u8MaxTxPower[ant] = tpc->points[ant][0].y;
          params->u8MaxTxPowerIndex[ant] = tpc->tpc_values[ant];
      }
    }
}

typedef struct
{
  int                         primary;
  int                         secondary;
  int                         secondary_offset;
} channel_pair_descriptor;

channel_pair_descriptor usa_2ghz_pairs[] =
{
  { 1,  6, ALTERNATE_UPPER},
  { 6,  1, ALTERNATE_LOWER},
  { 6, 11, ALTERNATE_UPPER},
  { 11, 6, ALTERNATE_LOWER},
  { 0,  0, ALTERNATE_NONE }
};/* using only channels 1, 6, 11 is recommended in the US to avoid interference */


channel_pair_descriptor rest_of_world_2ghz_pairs[] = 
{
  { 1,  5,  ALTERNATE_UPPER},
  { 2,  6,  ALTERNATE_UPPER},
  { 3,  7,  ALTERNATE_UPPER},
  { 4,  8,  ALTERNATE_UPPER},
  { 5,  1,  ALTERNATE_LOWER},
  { 5,  9,  ALTERNATE_UPPER},
  { 6,  2,  ALTERNATE_LOWER},
  { 6, 10,  ALTERNATE_UPPER},
  { 7,  3,  ALTERNATE_LOWER},
  { 7, 11,  ALTERNATE_UPPER},
  { 8,  4,  ALTERNATE_LOWER},
  { 9,  5,  ALTERNATE_LOWER},
  {10,  6,  ALTERNATE_LOWER},
  {11,  7,  ALTERNATE_LOWER},
  { 0,  0,  ALTERNATE_NONE }
};

BOOL __MTLK_IFUNC mtlk_channels_find_secondary_channel_no(int reg_domain, uint16 primary_channel_no, int secondary_channel_offset, uint16 *secondary_channel_no)
{
  BOOL res = FALSE;
  int i = 0;
  channel_pair_descriptor *cur_reg_domain_2ghz_channels_pairs = NULL;

  if ((UMI_CHANNEL_SW_MODE_SCA == secondary_channel_offset) ||
    (UMI_CHANNEL_SW_MODE_SCB == secondary_channel_offset))
  {
    cur_reg_domain_2ghz_channels_pairs = rest_of_world_2ghz_pairs;

    while ((cur_reg_domain_2ghz_channels_pairs[i].primary != 0) && (res == FALSE)) 
    {
      if(primary_channel_no == cur_reg_domain_2ghz_channels_pairs[i].primary)
      {
        if ((secondary_channel_offset == UMI_CHANNEL_SW_MODE_SCA) && 
          (cur_reg_domain_2ghz_channels_pairs[i].secondary_offset == ALTERNATE_UPPER))
        {
          *secondary_channel_no = cur_reg_domain_2ghz_channels_pairs[i].secondary;
          res = TRUE;
        }
        else if ((secondary_channel_offset == UMI_CHANNEL_SW_MODE_SCB) && 
          (cur_reg_domain_2ghz_channels_pairs[i].secondary_offset == ALTERNATE_LOWER))
        {
          *secondary_channel_no = cur_reg_domain_2ghz_channels_pairs[i].secondary;
          res = TRUE;
        } 
      }
      ++i;
    }
  }

  return res;
}

#define CB_CHANNEL_OFFSET 4

uint16 __MTLK_IFUNC mtlk_channels_get_secondary_channel_no_by_offset(uint16 primary_channel_no, uint8 secondary_channel_offset)
{
  uint16 res = 0;

  switch (secondary_channel_offset)
  {
  case UMI_CHANNEL_SW_MODE_SCA:
    res = primary_channel_no + CB_CHANNEL_OFFSET;
    break;
  case UMI_CHANNEL_SW_MODE_SCB:
    res = primary_channel_no - CB_CHANNEL_OFFSET;
    break;
  case UMI_CHANNEL_SW_MODE_SCN:
    break;
  default:
    ELOG_D("Incorrect secondary_channel_offset (%u)", secondary_channel_offset);
    MTLK_ASSERT(0);
    break;
  }

  return res;
}
