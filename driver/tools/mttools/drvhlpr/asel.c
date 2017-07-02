#include "mtlkinc.h"
#include "asel.h"
#include "mtlk_osal.h"
#include "mhi_umi.h"
#include "mtlkaselirb.h"
#include "mtlkirba.h"
#include "mtlkhash.h"
#include "mtlkmetrics.h"
#include "mtlk_cli_server.h"

#include "mhi_frame.h"

#define LOG_LOCAL_GID   GID_ASEL
#define LOG_LOCAL_FID   1

#define LBF_ON      /* Enables LQ proprietary G3 LBF  */
#define ASEL_ON     /* Enables LQ proprietary G2 ASEL */
#define CLI_DBG_ON  /* Enables MTLK CLI based DBG RF MGMT interface */

/*****************************************************************************
 * Common RF MGMT related definitions
 *****************************************************************************/
#define MAX_METRICS_SIZE MAX(sizeof(ASL_SHRAM_METRIC_T), sizeof(RFM_SHRAM_METRIC_T))

const static mtlk_guid_t IRBE_RF_MGMT_SET_TYPE      = MTLK_IRB_GUID_RF_MGMT_SET_TYPE;
const static mtlk_guid_t IRBE_RF_MGMT_GET_TYPE      = MTLK_IRB_GUID_RF_MGMT_GET_TYPE;
const static mtlk_guid_t IRBE_RF_MGMT_SET_DEF_DATA  = MTLK_IRB_GUID_RF_MGMT_SET_DEF_DATA;
const static mtlk_guid_t IRBE_RF_MGMT_GET_DEF_DATA  = MTLK_IRB_GUID_RF_MGMT_GET_DEF_DATA;
const static mtlk_guid_t IRBE_RF_MGMT_GET_PEER_DATA = MTLK_IRB_GUID_RF_MGMT_GET_PEER_DATA;
const static mtlk_guid_t IRBE_RF_MGMT_SET_PEER_DATA = MTLK_IRB_GUID_RF_MGMT_SET_PEER_DATA;
const static mtlk_guid_t IRBE_RF_MGMT_SEND_SP       = MTLK_IRB_GUID_RF_MGMT_SEND_SP;
const static mtlk_guid_t IRBE_RF_MGMT_GET_SPR       = MTLK_IRB_GUID_RF_MGMT_GET_SPR;
const static mtlk_guid_t IRBE_RF_MGMT_SPR_ARRIVED   = MTLK_IRB_GUID_RF_MGMT_SPR_ARRIVED;

#define RF_MGMT_MAX_AVERAGING_ALPHA      100
#define RF_MGMT_MAX_ENABLE_ATTEMPTS      3
#define RF_MGMT_MAX_ENABLE_PERIOD        1000  /* ms */

#define RF_MGMT_DEF_SPR_QUEUE_SIZE       32    /* Must be at lest as the max STAs for AP */

#define RF_MGMT_INACTIVITY_CHECK_PEROD_MS(asel)    \
  ((asel)->cfg.keep_alive_tmout_sec * 1000 / 5)

static const uint8 all_ranks[] =  
{
  1,
  2
};

static __INLINE uint8
_RF_MGMT_GET_S_RANK_IDX (uint8 rank_idx)
{
  MTLK_ASSERT(rank_idx == 0 || rank_idx == 1);

  return (uint8)((0 + 1) - rank_idx);
}

#define RF_MGMT_NOF_RANKS ARRAY_SIZE(all_ranks)

typedef uint8 rf_mgmt_data_t;

static __INLINE uint32
__rf_mgmt_aplha_filter (uint32 old_val, uint32 new_val, uint32 alpha_coeff)
{
  return (
    alpha_coeff * new_val + (RF_MGMT_MAX_AVERAGING_ALPHA - alpha_coeff) * old_val
    ) / RF_MGMT_MAX_AVERAGING_ALPHA;
}
/*****************************************************************************/

/*****************************************************************************
 * BF MGMT object related definitions
 *****************************************************************************/
typedef struct
{
  uint16        (__MTLK_IFUNC *get_sp_size)(void);
  uint16        (__MTLK_IFUNC *get_peer_private_data_size)(void);
  mtlk_handle_t (__MTLK_IFUNC *constructor)(const struct mtlk_rf_mgmt_cfg *cfg);
  int           (__MTLK_IFUNC *fill_sp)(mtlk_handle_t   ctx, 
                                        void           *sp_buff,
                                        uint32          sp_cnt,
                                        uint8          *rank,
                                        rf_mgmt_data_t *rf_mgmt_data);
  int           (__MTLK_IFUNC *handle_spr)(mtlk_handle_t                 ctx,
                                           const void                   *peer_private_data,
                                           const MTLK_VSAF_SPR_ITEM_HDR *spr_hdr,
                                           const void                   *sp_data,
                                           const void                   *metrics,
                                           rf_mgmt_data_t               *rf_mgmt_data);
  void          (__MTLK_IFUNC *log_peer)(mtlk_handle_t ctx,
                                         const void   *peer_private_data);
  void          (__MTLK_IFUNC *destructor)(mtlk_handle_t ctx);
} mtlk_rf_mgmt_obj_vft_t;
/*****************************************************************************/

#ifdef LBF_ON
/*****************************************************************************
 * BF MGMT MTLK LBF related definitions
 *****************************************************************************/
/* For more info see comment above the LBFRank1Val/LBFRank1Va2 definition */
static const rf_mgmt_data_t all_lbf_data_rank_1[] = 
  MTLK_LBF_G3_RANK1_AVAILABLE_VALUES;

static const rf_mgmt_data_t all_lbf_data_rank_2[] = 
  MTLK_LBF_G3_RANK2_AVAILABLE_VALUES;

struct mtlk_rf_mgmt_lbf_rank_data
{
  const rf_mgmt_data_t *values;
  const uint16          nof_values;
};

const struct mtlk_rf_mgmt_lbf_rank_data all_lbf_data[] = 
{
  { all_lbf_data_rank_1, ARRAY_SIZE(all_lbf_data_rank_1) },
  { all_lbf_data_rank_2, ARRAY_SIZE(all_lbf_data_rank_2) }
};

#define LBF_NOF_RANK1_VALS ARRAY_SIZE(all_lbf_data_rank_1)
#define LBF_NOF_RANK2_VALS ARRAY_SIZE(all_lbf_data_rank_2)

#define LBF_NOF_VALS (LBF_NOF_RANK1_VALS + LBF_NOF_RANK2_VALS)

static __INLINE uint16
LBF_DB_IDX_BY_ID (uint32 id)
{
  return (id % LBF_NOF_VALS);
}

static __INLINE uint16
LBF_DB_IDX_BY_RANK_AND_DATA_IDXs (uint16 rank_idx, uint16 data_idx)
{
  uint16 res = rank_idx * LBF_NOF_RANK1_VALS + data_idx;
  
  MTLK_ASSERT(res < LBF_NOF_VALS);

  return res;
}

static __INLINE void
LBF_RANK_AND_DATA_IDXs_BY_ID (uint32 id, uint16 *rank_idx, uint16 *data_idx)
{
  uint16 val_idx = LBF_DB_IDX_BY_ID(id);

  MTLK_ASSERT(rank_idx != NULL);
  MTLK_ASSERT(data_idx != NULL);

  if (val_idx < LBF_NOF_RANK1_VALS) {
    *rank_idx = 0;
    *data_idx = val_idx;
  }
  else {
    *rank_idx = 1;
    *data_idx = val_idx - LBF_NOF_RANK1_VALS;
  }
}

#define LBF_RF_MGMT_VAL(rank1_val, rank2_val) \
  (MTLK_BFIELD_VALUE(LBFRank1Val, (rank1_val), rf_mgmt_data_t) | \
   MTLK_BFIELD_VALUE(LBFRank2Val, (rank2_val), rf_mgmt_data_t))

struct mtlk_rf_mgmt_lbf_sp
{
  uint32 id;
};

struct mtlk_rf_mgmt_lbf_data_info
{
  /* Metrics related Data Members */
  uint32 average_esnr_db;  /* Average Effective SNR in DB */
  /* Internal Data Members */
  int    in_use;           /* Active/inactive flag */
};

#define LBF_BEST_DATA_IDX_NONE  ((uint16)-1)

struct mtlk_rf_mgmt_lbf_peer_info
{
  struct mtlk_rf_mgmt_lbf_data_info db[LBF_NOF_VALS];
  uint32                            last_received_sp_id;
  uint16                            best_data_idx[RF_MGMT_NOF_RANKS];
  int                               in_use; /* Active/inactive flag */
};

struct mtlk_rf_mgmt_lbf_obj
{
  const struct mtlk_rf_mgmt_cfg *cfg;
  MTLK_DECLARE_INIT_STATUS;
};

static __INLINE BOOL
_lbf_greater_op (struct mtlk_rf_mgmt_lbf_obj       *lbf,
                 struct mtlk_rf_mgmt_lbf_peer_info *peer_info,
                 uint16                             rank_idx,
                 uint16                             data_idx_1,
                 uint16                             data_idx_2)
{
  return (peer_info->db[LBF_DB_IDX_BY_RANK_AND_DATA_IDXs(rank_idx, data_idx_1)].average_esnr_db >= 
          peer_info->db[LBF_DB_IDX_BY_RANK_AND_DATA_IDXs(rank_idx, data_idx_2)].average_esnr_db)?TRUE:FALSE;
}

static uint16
_lbf_get_rank_best_data_idx (struct mtlk_rf_mgmt_lbf_obj       *lbf,
                             struct mtlk_rf_mgmt_lbf_peer_info *peer_info,
                             uint32                             rank_idx)
{
  uint16 i   = 0;
  uint16 res = LBF_BEST_DATA_IDX_NONE;

  /* Pass through the rank to find the best Data using bubble sort */
  for (i = 0; i < all_lbf_data[rank_idx].nof_values; ++i) {
    if (!peer_info->db[LBF_DB_IDX_BY_RANK_AND_DATA_IDXs(rank_idx, i)].in_use) {
      continue;
    }

    if (res == LBF_BEST_DATA_IDX_NONE || /* initial selection */
        _lbf_greater_op(lbf,
                        peer_info,
                        rank_idx,
                        i,
                        res)) {
      res = i;
    }
  }

  MTLK_ASSERT(res != LBF_BEST_DATA_IDX_NONE);

  return res;
}

static rf_mgmt_data_t
_lbf_recalc (struct mtlk_rf_mgmt_lbf_obj       *lbf,
             struct mtlk_rf_mgmt_lbf_peer_info *peer_info,
             uint16                             rank_idx,
             uint16                             data_idx,
             rf_mgmt_data_t                     rf_mgmt_data)
{
  BOOL best_in_rank = FALSE;

  if (peer_info->best_data_idx[rank_idx] == LBF_BEST_DATA_IDX_NONE) {
    /* There was no previous best on rank => enum rank for best Data */
    data_idx  = _lbf_get_rank_best_data_idx(lbf, 
                                            peer_info,
                                            rank_idx);
    best_in_rank = TRUE;
    ILOG1_D("Best on rank (enum): %d", data_idx);
  }
  else {
    best_in_rank = _lbf_greater_op(lbf, 
                                   peer_info,
                                   rank_idx,
                                   data_idx,
                                   peer_info->best_data_idx[rank_idx]);
  }

  if (best_in_rank) {
    uint16         s_rank_idx = _RF_MGMT_GET_S_RANK_IDX(rank_idx);
    rf_mgmt_data_t rank_data[2]; /* we work here with 2 ranks (current and supplementary) - 
                                  * according to NOF LBF_RF_MGMT_VAL params 
                                  */

    MTLK_ASSERT(data_idx < all_lbf_data[rank_idx].nof_values);
    rank_data[rank_idx] = all_lbf_data[rank_idx].values[data_idx];

    /* If best set on supplement is set => use it, else - use default */
    if (peer_info->best_data_idx[s_rank_idx] != LBF_BEST_DATA_IDX_NONE) {
      MTLK_ASSERT(peer_info->best_data_idx[s_rank_idx] < all_lbf_data[s_rank_idx].nof_values);
      rank_data[s_rank_idx] = all_lbf_data[s_rank_idx].values[peer_info->best_data_idx[s_rank_idx]];
    }
    else {
      rank_data[s_rank_idx] = MTLK_RF_MGMT_DATA_DEFAULT;
    }

    rf_mgmt_data = LBF_RF_MGMT_VAL(rank_data[0], rank_data[1]);

    peer_info->best_data_idx[rank_idx] = data_idx;
  }

  return rf_mgmt_data;
}

MTLK_INIT_STEPS_LIST_BEGIN(lbf_obj)
MTLK_INIT_INNER_STEPS_BEGIN(lbf_obj)
MTLK_INIT_STEPS_LIST_END(lbf_obj)

static void
_mtlk_rf_mgmt_lbf_cleanup (struct mtlk_rf_mgmt_lbf_obj *lbf)
{
  MTLK_CLEANUP_BEGIN(lbf_obj, MTLK_OBJ_PTR(lbf))
  MTLK_CLEANUP_END(lbf_obj, MTLK_OBJ_PTR(lbf))
}

static int
_mtlk_rf_mgmt_lbf_init (struct mtlk_rf_mgmt_lbf_obj   *lbf,
                        const struct mtlk_rf_mgmt_cfg *cfg)
{
  MTLK_INIT_TRY(lbf_obj, MTLK_OBJ_PTR(lbf))
    lbf->cfg = cfg;
  MTLK_INIT_FINALLY(lbf_obj, MTLK_OBJ_PTR(lbf))
  MTLK_INIT_RETURN(lbf_obj, MTLK_OBJ_PTR(lbf), _mtlk_rf_mgmt_lbf_cleanup, (lbf))
}

static uint16 __MTLK_IFUNC
_mtlk_rf_mgmt_lbf_get_sp_size (void)
{
  return (uint16)sizeof(struct mtlk_rf_mgmt_lbf_sp);
}

static uint16 __MTLK_IFUNC
_mtlk_rf_mgmt_lbf_get_peer_private_data_size (void)
{
  return (uint16)sizeof(struct mtlk_rf_mgmt_lbf_peer_info);
}

static mtlk_handle_t __MTLK_IFUNC
_mtlk_rf_mgmt_lbf_constructor (const struct mtlk_rf_mgmt_cfg *cfg)
{
  struct mtlk_rf_mgmt_lbf_obj *lbf = 
    (struct mtlk_rf_mgmt_lbf_obj *)mtlk_osal_mem_alloc(sizeof(*lbf),
                                                       MTLK_MEM_TAG_RFMGMT);

  MTLK_ASSERT(cfg != NULL);

  if (lbf) {
    int res;

    memset(lbf, 0, sizeof(*lbf));

    res = _mtlk_rf_mgmt_lbf_init(lbf, cfg);
    if (res != MTLK_ERR_OK) {
      ELOG_D("Can't init LBF object (err=%d)", res);
      mtlk_osal_mem_free(lbf);
      lbf = NULL;
    }
  }

  return HANDLE_T(lbf);
}

static int __MTLK_IFUNC
_mtlk_rf_mgmt_lbf_fill_sp (mtlk_handle_t   ctx, 
                           void           *sp_buff,
                           uint32          sp_cnt,
                           uint8          *rank,
                           rf_mgmt_data_t *rf_mgmt_data)
{
  struct mtlk_rf_mgmt_lbf_obj *lbf      = 
    HANDLE_T_PTR(struct mtlk_rf_mgmt_lbf_obj, ctx);
  struct mtlk_rf_mgmt_lbf_sp  *sp       = 
    (struct mtlk_rf_mgmt_lbf_sp *)sp_buff;
  uint16                       rank_idx;
  uint16                       data_idx;

  MTLK_UNREFERENCED_PARAM(lbf);

  MTLK_ASSERT(lbf != NULL);
  MTLK_ASSERT(sp != NULL);
  MTLK_ASSERT(rank != NULL);
  MTLK_ASSERT(rf_mgmt_data != NULL);

  LBF_RANK_AND_DATA_IDXs_BY_ID(sp_cnt, &rank_idx, &data_idx);

  sp->id        = sp_cnt;
  *rank         = all_ranks[rank_idx];
  *rf_mgmt_data = (rank_idx == 0)?
    LBF_RF_MGMT_VAL(all_lbf_data_rank_1[data_idx], 0):
    LBF_RF_MGMT_VAL(0, all_lbf_data_rank_2[data_idx]);

  return MTLK_ERR_OK;
}

static int __MTLK_IFUNC
_mtlk_rf_mgmt_lbf_handle_spr (mtlk_handle_t                 ctx,
                              const void                   *peer_private_data,
                              const MTLK_VSAF_SPR_ITEM_HDR *spr_hdr,
                              const void                   *sp_data,
                              const void                   *metrics,
                              rf_mgmt_data_t               *rf_mgmt_data)
{
  int                               res = MTLK_ERR_UNKNOWN;
  struct mtlk_rf_mgmt_lbf_obj      *lbf = 
    HANDLE_T_PTR(struct mtlk_rf_mgmt_lbf_obj, ctx);
  RFM_SHRAM_METRIC_T               *m   = 
    (RFM_SHRAM_METRIC_T *)metrics;
  struct mtlk_rf_mgmt_lbf_sp        *sp = 
    (struct mtlk_rf_mgmt_lbf_sp *)sp_data;
  uint32                             esnr_db_for_average;
  struct mtlk_rf_mgmt_lbf_peer_info *peer_info = 
    (struct mtlk_rf_mgmt_lbf_peer_info *)peer_private_data;
  struct mtlk_rf_mgmt_lbf_data_info *data_info;
  uint16                             rank_idx;
  uint16                             data_idx;
  uint32                             esnr_val;
  uint8                              mcsf_val;

  MTLK_ASSERT(lbf != NULL);
  MTLK_ASSERT(peer_private_data != NULL);
  MTLK_ASSERT(spr_hdr != NULL);
  MTLK_ASSERT(sp_data != NULL);
  MTLK_ASSERT(metrics != NULL);
  MTLK_ASSERT(rf_mgmt_data != NULL);

  /* Check SPR Data version - we don't support G2 (raw metrics) */
  if (spr_hdr->u16Version != SPR_DATA_VERSION_2) {
    WLOG_DD("SPR of unsupported version received (%d != %d)",
            spr_hdr->u16Version,
            SPR_DATA_VERSION_2);
    goto end;
  }

  /* Check SP Data size (we know what structure we're sending ) */
  if (spr_hdr->u16SPDataSize != sizeof(*sp)) {
    ELOG_DD("Incorrect SP Data size (%d != %d)",
          spr_hdr->u16SPDataSize,
          sizeof(*sp));
    goto end;
  }

  /* Check Metrics size */
  if (spr_hdr->u16MetricsSize != sizeof(*m)) {
    ELOG_DD("Metrics of incorrect size (%u != %d)",
          spr_hdr->u16MetricsSize,
          sizeof(*m));
    goto end;
  }

  /* Parse PHY metrics received */
  esnr_val = MTLK_BFIELD_GET(m->effectiveSNR, PHY_ESNR_VAL);
  mcsf_val = (uint8)MTLK_BFIELD_GET(m->effectiveSNR, PHY_MCSF_VAL);

  /* Calculate ESNR DB for averaging */
  esnr_db_for_average = mtlk_get_esnr_db(esnr_val, mcsf_val);

  ILOG0_DDDD("SPR: id=%u esnr=%u, mcsf=%d esnr_db=%u",
       sp->id, esnr_val, mcsf_val, esnr_db_for_average);

  if (peer_info->in_use) {
    uint32 i;
    /* Handle SPR holes - mark such TX Sets as unusable */
    for (i = peer_info->last_received_sp_id + 1; i < sp->id; i++) {
      struct mtlk_rf_mgmt_lbf_data_info *data_info = 
        &peer_info->db[LBF_DB_IDX_BY_ID(i)];

      LBF_RANK_AND_DATA_IDXs_BY_ID(i, &rank_idx, &data_idx);
      data_info->in_use = 0;

      if (peer_info->best_data_idx[rank_idx] == data_idx) {
        peer_info->best_data_idx[rank_idx] = LBF_BEST_DATA_IDX_NONE;
      }
    }
  }
  else {
    uint32 i;
    peer_info->in_use = 1;
    for (i = 0; i < ARRAY_SIZE(peer_info->best_data_idx); i++) {
      peer_info->best_data_idx[i] = LBF_BEST_DATA_IDX_NONE;
    }
  }

  data_info = &peer_info->db[LBF_DB_IDX_BY_ID(sp->id)];
  if (data_info->in_use) {
    data_info->average_esnr_db =
      __rf_mgmt_aplha_filter(data_info->average_esnr_db, 
                             esnr_db_for_average, 
                             lbf->cfg->averaging_alpha);
  }
  else {
    data_info->average_esnr_db = esnr_db_for_average;
    data_info->in_use          = 1;
  }

  /* Set Last SP ID according to received SPR */
  peer_info->last_received_sp_id = sp->id;

  /* Get rank_idx and data_idx by SP ID */
  LBF_RANK_AND_DATA_IDXs_BY_ID(sp->id, &rank_idx, &data_idx);

  /* Re-calculate RF MGMT LBF data for the peer */
  *rf_mgmt_data = _lbf_recalc(lbf,
                              peer_info,
                              rank_idx,
                              data_idx,
                             *rf_mgmt_data);
  res = MTLK_ERR_OK;

end:
  return res;
}
static void __MTLK_IFUNC
_mtlk_rf_mgmt_lbf_log_peer (mtlk_handle_t ctx,
                            const void   *peer_private_data)
{
  struct mtlk_rf_mgmt_lbf_obj       *lbf = 
    HANDLE_T_PTR(struct mtlk_rf_mgmt_lbf_obj, ctx);
  struct mtlk_rf_mgmt_lbf_peer_info *peer_info = 
    (struct mtlk_rf_mgmt_lbf_peer_info *)peer_private_data;
  uint32                            i;

  MTLK_UNREFERENCED_PARAM(lbf);

  MTLK_ASSERT(lbf != NULL);
  MTLK_ASSERT(peer_private_data != NULL);
  MTLK_ASSERT(peer_info->in_use != 0);

  MTLK_ASSERT(ARRAY_SIZE(all_lbf_data) == RF_MGMT_NOF_RANKS);

  ILOG0_V("\n--------------------------------\n"
          "| RANK | DATA | ESNR_DB | BEST |\n"
          "--------------------------------\n");
  for (i = 0; i < ARRAY_SIZE(peer_info->db); i++) {
    struct mtlk_rf_mgmt_lbf_data_info 
                  *data_info = &peer_info->db[i];
    uint16         rank_idx;
    uint16         data_idx;
    rf_mgmt_data_t rf_mgmt_data;

    LBF_RANK_AND_DATA_IDXs_BY_ID(i, &rank_idx, &data_idx);

    rf_mgmt_data = (rank_idx == 0)?
      LBF_RF_MGMT_VAL(all_lbf_data_rank_1[data_idx], 0):
      LBF_RF_MGMT_VAL(0, all_lbf_data_rank_2[data_idx]);

    if (data_info->in_use) {
      ILOG0_DDDC("| %4u | 0x%02x | %+7d |    %c |",
           all_ranks[rank_idx],
           rf_mgmt_data,
           data_info->average_esnr_db - ESNR_DB_BASE,
           (peer_info->best_data_idx[rank_idx] == data_idx)?'V':' ');
    }
    else {
      ILOG0_DD("| %4u | 0x%02x |         |    X |",
          all_ranks[rank_idx],
          rf_mgmt_data);
    }
  }
  ILOG0_V("--------------------------------");
}
static void __MTLK_IFUNC
_mtlk_rf_mgmt_lbf_destructor (mtlk_handle_t ctx)
{
  struct mtlk_rf_mgmt_lbf_obj *lbf = 
    HANDLE_T_PTR(struct mtlk_rf_mgmt_lbf_obj, ctx);

  MTLK_ASSERT(lbf != NULL);

  _mtlk_rf_mgmt_lbf_cleanup(lbf);
  mtlk_osal_mem_free(lbf);
}

static const mtlk_rf_mgmt_obj_vft_t mtlk_rf_mgmt_lbf_obj_vft =
{
  _mtlk_rf_mgmt_lbf_get_sp_size,
  _mtlk_rf_mgmt_lbf_get_peer_private_data_size,
  _mtlk_rf_mgmt_lbf_constructor,
  _mtlk_rf_mgmt_lbf_fill_sp,
  _mtlk_rf_mgmt_lbf_handle_spr,
  _mtlk_rf_mgmt_lbf_log_peer,
  _mtlk_rf_mgmt_lbf_destructor,
};
/*****************************************************************************/
#endif /* LBF_ON */

#ifdef ASEL_ON
/*****************************************************************************
* BF MGMT MTLK ASEL related definitions
******************************************************************************/
static const uint8 all_g2_antenna_sets[] = 
  MTLK_ASEL_G2_AVAILABLE_ASETS;  

#define ASEL_NOF_ASETS            ARRAY_SIZE(all_g2_antenna_sets)

#define ASEL_NOF_TX_SETS          (RF_MGMT_NOF_RANKS * ASEL_NOF_ASETS)

#define ASEL_TX_SET_IDX_BY_ID(id) ((id) % ASEL_NOF_TX_SETS)
#define ASEL_RANK_IDX_BY_ID(id)   (ASEL_TX_SET_IDX_BY_ID(id) / ASEL_NOF_ASETS)
#define ASEL_DATA_IDX_BY_ID(id)   (ASEL_TX_SET_IDX_BY_ID(id) % ASEL_NOF_ASETS)

/* According to the IEEE P801.11n/D2.05, Tables 20-33 and 20-34 */
static const uint16 rate_by_mcs[] =
{
  /* Rank1: MCS  [0..7] */
  135,
  270,
  405,
  540,
  810,
  1080,
  1215,
  1350, 
  /* Rank2: MCS [8..15] */
  270,
  540,
  810,
  1080,
  1620,
  2160,
  2430,
  2700
};

struct mtlk_rf_mgmt_asel_sp
{
  uint32 id;
  uint8  data_idx;
  uint8  rank_idx;
};

struct mtlk_rf_mgmt_asel_data_info
{
  /* Metrics related Data Members */
  uint32 average_esnr; /* Average Effective SNR */
  uint32 average_mcsf; /* Average multiplied MCS Feedback */
  uint32 margin;       /* Margin is a metrics reliability factor.
                        * Metrics are unreliable when the margin is
                        * below the pre-configured threshold.
                        */
  /* Internal Data Members */
  int    in_use;       /* Active/inactive flag */
  uint32 normal_mcsf;  /* normalized and rounded average_mcsf */
};

#define ASEL_BEST_DATA_IDX_NONE ((uint16)-1)

struct mtlk_rf_mgmt_asel_peer_info
{
  struct mtlk_rf_mgmt_asel_data_info db[RF_MGMT_NOF_RANKS][ASEL_NOF_ASETS]; /* MAX number of different combinations */
  uint32                             last_received_sp_id;
  uint16                             best_data_idx[RF_MGMT_NOF_RANKS];
  uint16                             current_data_idx;
  int                                in_use; /* Active/inactive flag */
};

struct mtlk_rf_mgmt_asel_obj
{
  const struct mtlk_rf_mgmt_cfg *cfg;
  MTLK_DECLARE_INIT_STATUS;
};

#define ASEL_G2_MCS_FEEDBACK_FACTOR 100 /* MCS is small number, so the averaging will
                                         * not be so good because of integral division.
                                         * This factor is used to improve the MCS Feedback 
                                         * averaging accuracy.
                                         */

static __INLINE uint32
__asel_get_mcs_for_averaging (uint32 mcs_feedback)
{
  /* Multiplication - for better averaging */
  return (mcs_feedback * ASEL_G2_MCS_FEEDBACK_FACTOR);
}

static __INLINE uint32
__asel_get_mcs_by_average (uint32 mcs_average)
{
  /* Division + correct round */
  return (mcs_average + (ASEL_G2_MCS_FEEDBACK_FACTOR / 2)) / ASEL_G2_MCS_FEEDBACK_FACTOR;
}

static BOOL
_asel_is_all_ranks_best (struct mtlk_rf_mgmt_asel_obj       *asel, 
                         struct mtlk_rf_mgmt_asel_peer_info *peer_info,
                         uint16                              rank_idx)
{
  BOOL   res                  = FALSE;
  uint32 s_rank_idx           = _RF_MGMT_GET_S_RANK_IDX(rank_idx);
  const struct mtlk_rf_mgmt_asel_data_info 
    *best_data_into           = NULL;
  const struct mtlk_rf_mgmt_asel_data_info
    *s_best_data_into         = NULL;
  uint16 best_expected_rate   = 0;
  uint16 s_best_expected_rate = 0;

  if (peer_info->best_data_idx[s_rank_idx] == ASEL_BEST_DATA_IDX_NONE) {
    res = TRUE;
    goto end;
  }

  best_data_into = 
    &peer_info->db[rank_idx][peer_info->best_data_idx[rank_idx]];
  s_best_data_into = 
    &peer_info->db[s_rank_idx][peer_info->best_data_idx[s_rank_idx]];

  best_expected_rate =
    rate_by_mcs[best_data_into->normal_mcsf];

  s_best_expected_rate =  
    rate_by_mcs[s_best_data_into->normal_mcsf];

  if (best_expected_rate > s_best_expected_rate) {
    res = TRUE;
  }
  else if (best_expected_rate == s_best_expected_rate &&
           best_data_into->margin > s_best_data_into->margin) {
      res = TRUE;
  }

end:
  return res;
}

static BOOL
_asel_greater_op_sup_esnr (struct mtlk_rf_mgmt_asel_obj       *asel, 
                           struct mtlk_rf_mgmt_asel_peer_info *peer_info,
                           uint16                              rank_idx,
                           uint16                              data_idx_1,
                           uint16                              data_idx_2)
{
  uint16                                    s_rank_idx = _RF_MGMT_GET_S_RANK_IDX(rank_idx);
  const struct mtlk_rf_mgmt_asel_data_info *s_data_info_1;
  const struct mtlk_rf_mgmt_asel_data_info *s_data_info_2;

  s_data_info_1 = 
    &peer_info->db[s_rank_idx][data_idx_1];
  s_data_info_2 = 
    &peer_info->db[s_rank_idx][data_idx_2];

  return (!s_data_info_1->in_use ||
          !s_data_info_2->in_use ||
          s_data_info_1->average_esnr > s_data_info_2->average_esnr);
}

enum _asel_gt_decision_code {
  RF_MGMT_GTDC_MCSF,
  RF_MGMT_GTDC_MARGIN_THRESHOLD,
  RF_MGMT_GTDC_SUP_ESNR,
  RF_MGMT_GTDC_LAST  
};

/* 
* This function returns 
*      (TX_SET(rank_idx, aset_idx_1) > TX_SET(rank_idx, aset_idx_2)).
* I.e. it returns TRUE if TX_SET(rank_idx, aset_idx_1) is better then 
* TX_SET(rank_idx, aset_idx_2) 
*/
static BOOL
_asel_greater_op (struct mtlk_rf_mgmt_asel_obj       *asel, 
                  struct mtlk_rf_mgmt_asel_peer_info *peer_info,
                  uint16                              rank_idx,
                  uint16                              data_idx_1,
                  uint16                              data_idx_2,
                  enum _asel_gt_decision_code        *_dcode)
{
  BOOL                                      res      = FALSE;
  enum _asel_gt_decision_code               dcode    = RF_MGMT_GTDC_LAST;
  const struct mtlk_rf_mgmt_asel_data_info *data_info_1;
  const struct mtlk_rf_mgmt_asel_data_info *data_info_2;

  data_info_1 = &peer_info->db[rank_idx][data_idx_1];
  data_info_2 = &peer_info->db[rank_idx][data_idx_2];

  if (data_info_2->average_esnr >= data_info_1->average_esnr) {
    /* Set2_Effective_SNR >= Set1_Effective_SNR, i.e. Set2 >= Set1 */
    ILOG4_DDDDD("R=%d A1=%d A2=%d: ESNR2 (%d) >= ESNR1 (%d):",
      rank_idx, data_idx_1, data_idx_2, 
      data_info_2->average_esnr, data_info_1->average_esnr);
  }
  else if (data_info_2->normal_mcsf > data_info_1->normal_mcsf) {
    /* ERROR: Set2_MCS_Feedback > Set1_MCS_Feedback,
    * when the Set2_Effective_SNR < Set1_Effective_SNR
    */
    ELOG_DDDDDDD("R=%d A1=%d A2=%d: MCSF2 (%d, ESNR=%d) > MCSF1 (%d, ESNR=%d)!",
      rank_idx, data_idx_1, data_idx_2, 
      data_info_2->normal_mcsf, data_info_2->average_esnr,
      data_info_1->normal_mcsf, data_info_1->average_esnr);
  }
  else if (data_info_2->normal_mcsf < data_info_1->normal_mcsf) {
    /* New entry's MCS Feedback is higher then the best one */
    res   = TRUE;
    dcode = RF_MGMT_GTDC_MCSF;
  }
  else if (data_info_1->margin > asel->cfg->margin_threshold) {
    /* New entry's MCS Feedback is equal to then the best one and
    * the margin is above the threshold
    */
    res   = TRUE;
    dcode = RF_MGMT_GTDC_MARGIN_THRESHOLD;
  }
  else if (_asel_greater_op_sup_esnr(asel, 
                                     peer_info, 
                                     rank_idx, 
                                     data_idx_1,
                                     data_idx_2)) {
    /* Selected by Effective SNR on supplement rate */
    res   = TRUE;
    dcode = RF_MGMT_GTDC_SUP_ESNR;
  }

  if (_dcode) {
    *_dcode = dcode;
  }

  return res;
}

static uint16
_asel_get_rank_best_data_idx (struct mtlk_rf_mgmt_asel_obj       *asel, 
                              struct mtlk_rf_mgmt_asel_peer_info *peer_info,
                              uint16                              rank_idx)
{
  uint32 i   = 0;
  uint32 res = peer_info->best_data_idx[rank_idx];

  /* Pass through the rank to find the best ASet using bubble sort */
  for (i = 0; i < ASEL_NOF_ASETS; ++i) {
    if (!peer_info->db[rank_idx][i].in_use) {
      continue;
    }

    if (res == ASEL_BEST_DATA_IDX_NONE ||
        _asel_greater_op(asel,
                         peer_info,
                         rank_idx,
                         i,
                         res,
                         NULL)) {
        res = i;
    }
  }

  MTLK_ASSERT(res != ASEL_BEST_DATA_IDX_NONE);

  return res;
}

static rf_mgmt_data_t
_asel_recalc (struct mtlk_rf_mgmt_asel_obj       *asel, 
              struct mtlk_rf_mgmt_asel_peer_info *peer_info,
              uint16                              rank_idx,
              uint16                              data_idx,
              rf_mgmt_data_t                      rf_mgmt_data)
{
  BOOL                        best_in_rank     = FALSE;
  enum _asel_gt_decision_code dcode            = RF_MGMT_GTDC_LAST;

  if (peer_info->best_data_idx[rank_idx] == ASEL_BEST_DATA_IDX_NONE) {
    /* There was no previous best on rank => enum rank for best Aset */
    data_idx     = _asel_get_rank_best_data_idx(asel, 
                                                peer_info,
                                                rank_idx);
    best_in_rank = TRUE;
    ILOG1_D("Best on rank (enum): %d", data_idx);
  }
  else {
    /* There was a previous best on rank => compare Data of arrived SPR with it */
    best_in_rank = _asel_greater_op(asel,
                                    peer_info,
                                    rank_idx,
                                    data_idx,
                                    peer_info->best_data_idx[rank_idx],
                                    &dcode);
  }

  if (best_in_rank) {
    ILOG1_DDD("R=%d A=%d: Best on Rank (Aprev=%d)",
      rank_idx, data_idx, peer_info->best_data_idx[rank_idx]);
#ifdef MTCFG_DEBUG
    if (debug) {
      const struct mtlk_rf_mgmt_asel_data_info *best_rank_data_info = 
        &peer_info->db[rank_idx][peer_info->best_data_idx[rank_idx]];
      const struct mtlk_rf_mgmt_asel_data_info *new_data_info = 
        &peer_info->db[rank_idx][data_idx];

      MTLK_UNREFERENCED_PARAM(best_rank_data_info);
      MTLK_UNREFERENCED_PARAM(new_data_info);

      switch (dcode) {
        case RF_MGMT_GTDC_MCSF:
          ILOG1_DD("  by MCSF (%d < %d)",
            best_rank_data_info->normal_mcsf, new_data_info->normal_mcsf);
          break;
        case RF_MGMT_GTDC_MARGIN_THRESHOLD:
          ILOG1_DD("  by Margin Threshold (%d > %d)",
            new_data_info->margin, RF_MGMT_GTDC_MARGIN_THRESHOLD);
          break;
        case RF_MGMT_GTDC_SUP_ESNR:
          ILOG1_V("  by Sup ESNR");
          break;
        case RF_MGMT_GTDC_LAST:
        default:
          break;
      }
    }
#endif

    peer_info->best_data_idx[rank_idx] = data_idx;

    if (_asel_is_all_ranks_best(asel, peer_info, rank_idx)) {
      ILOG1_DDD("R=%d D=%d: Best (Aprev=%d)",  rank_idx, all_g2_antenna_sets[data_idx], rf_mgmt_data);
      peer_info->current_data_idx = data_idx;
    }
    else {
      uint16 s_rank_idx = _RF_MGMT_GET_S_RANK_IDX(rank_idx);
      ILOG1_DD("Sup R=%d D=%d: Best", s_rank_idx, all_g2_antenna_sets[peer_info->best_data_idx[s_rank_idx]]);
      peer_info->current_data_idx = peer_info->best_data_idx[s_rank_idx];
    }
  }

  return all_g2_antenna_sets[peer_info->current_data_idx];
}

MTLK_INIT_STEPS_LIST_BEGIN(asel_obj)
MTLK_INIT_INNER_STEPS_BEGIN(asel_obj)
MTLK_INIT_STEPS_LIST_END(asel_obj)

static void
_mtlk_rf_mgmt_asel_cleanup (struct mtlk_rf_mgmt_asel_obj *asel)
{
  MTLK_CLEANUP_BEGIN(asel_obj, MTLK_OBJ_PTR(asel))
  MTLK_CLEANUP_END(asel_obj, MTLK_OBJ_PTR(asel))
}

static int
_mtlk_rf_mgmt_asel_init (struct mtlk_rf_mgmt_asel_obj *asel,
                         const struct mtlk_rf_mgmt_cfg *cfg)
{
  MTLK_INIT_TRY(asel_obj, MTLK_OBJ_PTR(asel))
    asel->cfg = cfg;
  MTLK_INIT_FINALLY(asel_obj, MTLK_OBJ_PTR(asel))
  MTLK_INIT_RETURN(asel_obj, MTLK_OBJ_PTR(asel), _mtlk_rf_mgmt_asel_cleanup, (asel))
}

static uint16 __MTLK_IFUNC
_mtlk_rf_mgmt_asel_get_sp_size (void)
{
  return (uint16)sizeof(struct mtlk_rf_mgmt_asel_sp);
}
static uint16 __MTLK_IFUNC
_mtlk_rf_mgmt_asel_get_peer_private_data_size (void)
{
  return (uint16)sizeof(struct mtlk_rf_mgmt_asel_peer_info);
}
static mtlk_handle_t __MTLK_IFUNC
_mtlk_rf_mgmt_asel_constructor (const struct mtlk_rf_mgmt_cfg *cfg)
{
  struct mtlk_rf_mgmt_asel_obj *asel = 
    (struct mtlk_rf_mgmt_asel_obj *)mtlk_osal_mem_alloc(sizeof(*asel),
                                                        MTLK_MEM_TAG_RFMGMT);

  MTLK_ASSERT(cfg != NULL);
  MTLK_ASSERT(cfg->irba != NULL);

  if (asel) {
    int res;

    memset(asel, 0, sizeof(*asel));

    res = _mtlk_rf_mgmt_asel_init(asel, cfg);
    if (res != MTLK_ERR_OK) {
      ELOG_D("Can't init ASEL object (err=%d)", res);
      mtlk_osal_mem_free(asel);
      asel = NULL;
    }
  }

  return HANDLE_T(asel);
}

static int __MTLK_IFUNC
_mtlk_rf_mgmt_asel_fill_sp (mtlk_handle_t   ctx, 
                            void           *sp_buff,
                            uint32          sp_cnt,
                            uint8          *rank,
                            rf_mgmt_data_t *rf_mgmt_data)
{
  struct mtlk_rf_mgmt_asel_obj *asel = 
    HANDLE_T_PTR(struct mtlk_rf_mgmt_asel_obj, ctx);
  struct mtlk_rf_mgmt_asel_sp  *sp   = 
    (struct mtlk_rf_mgmt_asel_sp *)sp_buff;

  MTLK_UNREFERENCED_PARAM(asel);

  MTLK_ASSERT(asel != NULL);
  MTLK_ASSERT(sp != NULL);
  MTLK_ASSERT(rank != NULL);
  MTLK_ASSERT(rf_mgmt_data != NULL);

  /* Fill the SP data */
  sp->id       = sp_cnt;
  sp->data_idx = ASEL_DATA_IDX_BY_ID(sp_cnt);
  sp->rank_idx = ASEL_RANK_IDX_BY_ID(sp_cnt);

  /* Fill the return values */
  *rank         = all_ranks[sp->rank_idx];
  *rf_mgmt_data = all_g2_antenna_sets[sp->data_idx];

  return MTLK_ERR_OK;
}

static int __MTLK_IFUNC
_mtlk_rf_mgmt_asel_handle_spr (mtlk_handle_t                 ctx,
                               const void                   *peer_private_data,
                               const MTLK_VSAF_SPR_ITEM_HDR *spr_hdr,
                               const void                   *sp_data,
                               const void                   *metrics,
                               rf_mgmt_data_t               *rf_mgmt_data)
{
  int                                res = MTLK_ERR_UNKNOWN;
  struct mtlk_rf_mgmt_asel_obj      *asel = 
    HANDLE_T_PTR(struct mtlk_rf_mgmt_asel_obj, ctx);
  ASL_SHRAM_METRIC_T                *m  = 
    (ASL_SHRAM_METRIC_T *)metrics;
  struct mtlk_rf_mgmt_asel_sp        *sp = 
    (struct mtlk_rf_mgmt_asel_sp *)sp_data;
  struct mtlk_rf_mgmt_asel_peer_info *peer_info = 
    (struct mtlk_rf_mgmt_asel_peer_info *)peer_private_data;
  struct mtlk_rf_mgmt_asel_data_info *data_info;
  uint32                              esnr;
  uint8                               mcsf;
  uint32                              short_cp_metric;
  uint32                              mcsf_for_average;

  MTLK_ASSERT(asel != NULL);
  MTLK_ASSERT(peer_private_data != NULL);
  MTLK_ASSERT(spr_hdr != NULL);
  MTLK_ASSERT(sp_data != NULL);
  MTLK_ASSERT(metrics != NULL);
  MTLK_ASSERT(rf_mgmt_data != NULL);

  /* Check SPR Data version - we do support G2 (raw metrics) */
  if (spr_hdr->u16Version != SPR_DATA_VERSION_1) {
      WLOG_D("SPR of unsupported version received (%d)",
        spr_hdr->u16Version);
      goto end;
  }

  /* Check SP Data size (we know what structure we're sending ) */
  if (spr_hdr->u16SPDataSize != sizeof(*sp)) {
    ELOG_DD("Incorrect SP Data size (%d != %d)",
      spr_hdr->u16SPDataSize,
      sizeof(*sp));
    goto end;
  }

  /* Check Metrics size */
  if (spr_hdr->u16MetricsSize != sizeof(*m)) {
    ELOG_DD("Metrics of incorrect size (%u != %d)",
      spr_hdr->u16MetricsSize,
      sizeof(*m));
    goto end;
  }

  esnr = mtlk_metrics_calc_effective_snr(m, &mcsf, &short_cp_metric);

  ILOG0_DDDDDD("SPR: id=%u data_idx=%d rank_idx=%d esnr=%u, mcsf=%d shcp=%u",
        sp->id, sp->data_idx, sp->rank_idx, esnr, mcsf, short_cp_metric);
  
  mcsf_for_average = __asel_get_mcs_for_averaging(mcsf);
  
  if (peer_info->in_use) {
    uint32 i;
    /* Handle SPR holes - mark such TX Sets as unusable */
    for (i = peer_info->last_received_sp_id + 1; i < sp->id; i++) {
      uint16 rank_idx = ASEL_RANK_IDX_BY_ID(i);
      uint16 data_idx = ASEL_DATA_IDX_BY_ID(i);

      peer_info->db[rank_idx][data_idx].in_use = 0;

      if (peer_info->best_data_idx[rank_idx] == data_idx) {
        peer_info->best_data_idx[rank_idx] = ASEL_BEST_DATA_IDX_NONE;
      }
    }
  }
  else {
    uint32 i;
    peer_info->in_use = 1;
    for (i = 0; i < ARRAY_SIZE(peer_info->best_data_idx); i++) {
      peer_info->best_data_idx[i] = ASEL_BEST_DATA_IDX_NONE;
    }
    peer_info->current_data_idx = ASEL_BEST_DATA_IDX_NONE;
  }

  data_info = &peer_info->db[sp->rank_idx][sp->data_idx];
  if (data_info->in_use) {
    data_info->average_esnr = 
      __rf_mgmt_aplha_filter(data_info->average_esnr, esnr, 
                             asel->cfg->averaging_alpha);
    data_info->average_mcsf  = 
      __rf_mgmt_aplha_filter(data_info->average_mcsf, 
                             mcsf_for_average, 
                             asel->cfg->averaging_alpha);
  }
  else {
    data_info->in_use       = 1;
    data_info->average_esnr = esnr;
    data_info->average_mcsf = mcsf_for_average;
  }

  data_info->normal_mcsf = 
    __asel_get_mcs_by_average(data_info->average_mcsf);
  data_info->margin = 
    mtlk_calculate_effective_snr_margin(data_info->average_esnr,
                                        data_info->normal_mcsf);

  /* Set Last SP ID according to received SPR */
  peer_info->last_received_sp_id = sp->id;

  /* Re-calculate RF MGMT ASel data for the peer */
  *rf_mgmt_data = _asel_recalc(asel,
                               peer_info,
                               sp->rank_idx,
                               sp->data_idx,
                              *rf_mgmt_data);
  res = MTLK_ERR_OK;

end:
  return res;
}

static void __MTLK_IFUNC
_mtlk_rf_mgmt_asel_log_peer (mtlk_handle_t ctx,
                             const void   *peer_private_data)
{
  static const struct mtlk_rf_mgmt_asel_data_info 
    inactive_data_info = {0};

  struct mtlk_rf_mgmt_asel_obj       *asel = 
    HANDLE_T_PTR(struct mtlk_rf_mgmt_asel_obj, ctx);
  struct mtlk_rf_mgmt_asel_peer_info *peer_info = 
    (struct mtlk_rf_mgmt_asel_peer_info *)peer_private_data;
  uint16 i;
  char   str[512];
  int    size   = sizeof(str);
  int    offset = 0;
  int    res    = 0;

  MTLK_UNREFERENCED_PARAM(asel);

  MTLK_ASSERT(asel != NULL);
  MTLK_ASSERT(peer_private_data != NULL);
  MTLK_ASSERT(peer_info->in_use != 0);

  memset(str, 0, sizeof(str));
  memset(str, '=', 8 /* RF MGMT Data column width */ + 
    27 * RF_MGMT_NOF_RANKS /* per-Rank data columns width */);
  ILOG0_S("%s", str);

  res = snprintf(str, size, "| Data |");
  MTLK_ASSERT(res > 0 && res < size);
  offset += res;
  size   -= res;

  for (i = 0; i < RF_MGMT_NOF_RANKS; ++i) {
    res = snprintf(str + offset, size, " ESNR#%d | MCSF#%d | MARG#%d |", 
      all_ranks[i], all_ranks[i], all_ranks[i]);
    MTLK_ASSERT(res > 0 && res < size);
    offset += res;
    size   -= res;
  }
  ILOG0_S("%s", str);

  for (i = 0; i < ASEL_NOF_ASETS; ++i) {
    int j;

    offset = 0;
    size   = sizeof(str);

    res = snprintf(str + offset, size, "| 0x%02x |", all_g2_antenna_sets[i]);

    MTLK_ASSERT(res > 0 && res < size);
    offset += res;
    size   -= res;

    for (j = 0; j < RF_MGMT_NOF_RANKS; ++j) {
      const struct mtlk_rf_mgmt_asel_data_info *data_info =
        &peer_info->db[j][i];

      if (!data_info->in_use) {
        data_info = &inactive_data_info;
      }

      res = snprintf(str + offset, size, " %6u | %6u | %6d |", 
        data_info->average_esnr, data_info->average_mcsf, data_info->margin);

      MTLK_ASSERT(res > 0 && res < size);
      offset += res;
      size   -= res;
    }

    for (j = 0; j < RF_MGMT_NOF_RANKS; ++j) {
      if (i == peer_info->best_data_idx[j]) {
        res = snprintf(str + offset, size, " b#%d", all_ranks[j]);
        MTLK_ASSERT(res > 0 && res < size);
        offset += res;
        size   -= res;        
      }
    }

    if (i == peer_info->current_data_idx) {
      res = snprintf(str + offset, size, " B");
      MTLK_ASSERT(res > 0 && res < size);
      offset += res;
      size   -= res;        
    }

    ILOG0_S("%s", str);
  }

  memset(str, 0, sizeof(str));
  memset(str, '=', 8 /* RF MGMT Data column width */ + 
    27 * RF_MGMT_NOF_RANKS /* per-Rank data columns width */);
  ILOG0_S("%s", str);
}

static void __MTLK_IFUNC
_mtlk_rf_mgmt_asel_destructor (mtlk_handle_t ctx)
{
  struct mtlk_rf_mgmt_asel_obj *asel = 
    HANDLE_T_PTR(struct mtlk_rf_mgmt_asel_obj, ctx);

  MTLK_ASSERT(asel != NULL);

  _mtlk_rf_mgmt_asel_cleanup(asel);
  mtlk_osal_mem_free(asel);
}

static const mtlk_rf_mgmt_obj_vft_t mtlk_rf_mgmt_asel_obj_vft =
{
  _mtlk_rf_mgmt_asel_get_sp_size,
  _mtlk_rf_mgmt_asel_get_peer_private_data_size,
  _mtlk_rf_mgmt_asel_constructor,
  _mtlk_rf_mgmt_asel_fill_sp,
  _mtlk_rf_mgmt_asel_handle_spr,
  _mtlk_rf_mgmt_asel_log_peer,
  _mtlk_rf_mgmt_asel_destructor,
};
/*****************************************************************************/
#endif /* ASEL_ON */

#define   MTLK_IDEFS_ON
#include "mtlkidefs.h"

struct eth_addr
{
  uint8 addr[ETH_ALEN];
};

MTLK_HASH_DECLARE_ENTRY_T(rf_mgmt, struct eth_addr);

struct mtlk_rf_mgmt_peer_node
{
  mtlk_osal_timestamp_t          ts;
  rf_mgmt_data_t                 current_data;
  MTLK_HASH_ENTRY_T(rf_mgmt)     hentry;
  uint8                          private_data[1];
};

MTLK_HASH_DECLARE_INLINE(rf_mgmt, struct eth_addr);

#define   MTLK_IDEFS_OFF
#include "mtlkidefs.h"

#define RF_MGMT_SPR_HASH_MAX_IDX 10

static __INLINE uint32
rf_mgmt_hash_hash_func (const struct eth_addr *key, uint32 nof_buckets)
{
  MTLK_UNREFERENCED_PARAM(nof_buckets);

  return key->addr[ETH_ALEN - 1] % RF_MGMT_SPR_HASH_MAX_IDX;
}

static __INLINE int
rf_mgmt_hash_key_cmp_func (const struct eth_addr *key1,
                           const struct eth_addr *key2)
{
  return mtlk_osal_compare_eth_addresses(key1->addr, key2->addr);
}

MTLK_HASH_DEFINE_INLINE(rf_mgmt,
                        struct eth_addr,
                        rf_mgmt_hash_hash_func,
                        rf_mgmt_hash_key_cmp_func);

struct mtlk_rf_mgmt
{
  struct mtlk_rf_mgmt_cfg       cfg;
  mtlk_osal_event_t             evt;
  int                           stop_signaled;
  int                           ch_switched;
  mtlk_atomic_t                 spr_arrived;
  mtlk_osal_thread_t            thread;
  const mtlk_rf_mgmt_obj_vft_t *api;
  mtlk_handle_t                 api_ctx;
  mtlk_hash_t                   peers;
  uint32                        sp_id;       /* SP packet ID */
  uint8                        *sp_buff;
  uint8                        *spr_buff;
  mtlk_irba_handle_t           *spr_irb_ctx;
  MTLK_DECLARE_START_STATUS;
};

static int
_rf_mgmt_set_peer_data (struct mtlk_rf_mgmt    *rf_mgmt, 
                        const struct eth_addr  *spr_src_addr,
                        rf_mgmt_data_t          desired_rf_mgmt_data)
{
  int                               res = MTLK_ERR_UNKNOWN;
  struct mtlk_rf_mgmt_evt_peer_data peer_data;

  memcpy(peer_data.mac, spr_src_addr->addr, ETH_ALEN);
  peer_data.rf_mgmt_data = desired_rf_mgmt_data;
  peer_data.result       = MTLK_ERR_UNKNOWN;

  res = mtlk_irba_call_drv(rf_mgmt_cfg.irba, &IRBE_RF_MGMT_SET_PEER_DATA, &peer_data, sizeof(peer_data));
  if (res != MTLK_ERR_OK)  {
    WLOG_D("Can't set peer RF Mgmt Data (res=%d)", res);
  }
  else if (peer_data.result != MTLK_ERR_OK) {
    WLOG_D("Can't set peer RF Mgmt Dat (irb_res=%d)", peer_data.result);
    res = (int)peer_data.result;
  }

  return res;
}

static void
_rf_mgmt_check_inactivity (struct mtlk_rf_mgmt *rf_mgmt)
{
  mtlk_hash_enum_t            ctx;
  MTLK_HASH_ENTRY_T(rf_mgmt) *h;

  /* Enumerate the DB */
  h = mtlk_hash_enum_first_rf_mgmt(&rf_mgmt->peers, &ctx);
  while (h) {
    struct mtlk_rf_mgmt_peer_node *peer_node = 
      MTLK_CONTAINER_OF(h, struct mtlk_rf_mgmt_peer_node, hentry);

    /* Check Inactivity */
    if (rf_mgmt->cfg.keep_alive_tmout_sec && /* 0 - means disabled */
      mtlk_osal_timestamp() - peer_node->ts >= 
      mtlk_osal_ms_to_timestamp(rf_mgmt->cfg.keep_alive_tmout_sec * 1000)) {
        /* If inactive entry - delete it from DB and free memory */
        mtlk_hash_remove_rf_mgmt(&rf_mgmt->peers, &peer_node->hentry);
        mtlk_osal_mem_free(peer_node);
    }

    h = mtlk_hash_enum_next_rf_mgmt(&rf_mgmt->peers, &ctx);
  }
}

static void
_rf_mgmt_empty_hash (struct mtlk_rf_mgmt *rf_mgmt)
{
  mtlk_hash_enum_t            ctx;
  MTLK_HASH_ENTRY_T(rf_mgmt) *h;

  /* Enumerate the DB */
  h = mtlk_hash_enum_first_rf_mgmt(&rf_mgmt->peers, &ctx);
  while (h) {
    struct mtlk_rf_mgmt_peer_node *peer_node = 
      MTLK_CONTAINER_OF(h, struct mtlk_rf_mgmt_peer_node, hentry);

    mtlk_hash_remove_rf_mgmt(&rf_mgmt->peers, &peer_node->hentry);
    mtlk_osal_mem_free(peer_node);

    h = mtlk_hash_enum_next_rf_mgmt(&rf_mgmt->peers, &ctx);
  }
}

static __INLINE void
_rf_mgmt_prepare_metrics (uint32 *metrics,
                          uint16  size)
{
#ifdef __BIG_ENDIAN
  uint16 nof_u32 = size / sizeof(uint32);
  while (nof_u32) {
    *metrics = MAC_TO_HOST32(*metrics);
    metrics++;
    nof_u32--;
  }
#endif
}

static __INLINE uint16
__rf_mgmt_sizeof_spr_evt_data_buff (struct mtlk_rf_mgmt *rf_mgmt)
{
  return (uint16)
    (sizeof(MTLK_VSAF_SPR_ITEM_HDR) + 
     rf_mgmt->api->get_peer_private_data_size() + 
     MAX_METRICS_SIZE);
}

static __INLINE uint16
__rf_mgmt_sizeof_spr_evt_buff (struct mtlk_rf_mgmt *rf_mgmt)
{
  return (uint16)
    (sizeof(struct mtlk_rf_mgmt_evt_get_spr) +
    __rf_mgmt_sizeof_spr_evt_data_buff(rf_mgmt));
}

static __INLINE uint16
__rf_mgmt_sizeof_sp_evt_buff (struct mtlk_rf_mgmt *rf_mgmt)
{
  return (uint16)
    (sizeof(struct mtlk_rf_mgmt_evt_send_sp) +
    rf_mgmt->api->get_sp_size());
}

static void 
_rf_mgmt_on_ch_switched (struct mtlk_rf_mgmt *rf_mgmt)
{
  MTLK_UNREFERENCED_PARAM(rf_mgmt);
  MTLK_ASSERT(!"Not implemented");
}

static void
_rf_mgmt_parse_spr (struct mtlk_rf_mgmt             *rf_mgmt,
                    struct mtlk_rf_mgmt_evt_get_spr *spr_info)
{
  int                            res;
  MTLK_HASH_ENTRY_T(rf_mgmt)    *h;
  struct mtlk_rf_mgmt_peer_node *peer_node;
  uint8                         *spr_data;
  MTLK_VSAF_SPR_ITEM_HDR        *spr_hdr;
  void                          *sp_data;
  uint32                        *metrics;
  rf_mgmt_data_t                 new_rf_mgmt_data;

  spr_data = (uint8 *)mtlk_rf_mgmt_evt_get_spr_data(spr_info);
  spr_hdr  = (MTLK_VSAF_SPR_ITEM_HDR *)spr_data;

  spr_hdr->u16Version       = WLAN_TO_HOST16(spr_hdr->u16Version);
  spr_hdr->u16SPDataOffset  = WLAN_TO_HOST16(spr_hdr->u16SPDataOffset);
  spr_hdr->u16SPDataSize    = WLAN_TO_HOST16(spr_hdr->u16SPDataSize);
  spr_hdr->u16MetricsOffset = WLAN_TO_HOST16(spr_hdr->u16MetricsOffset);
  spr_hdr->u16MetricsSize   = WLAN_TO_HOST16(spr_hdr->u16MetricsSize);

  sp_data  = (spr_data + sizeof(*spr_hdr) + spr_hdr->u16SPDataOffset);
  metrics  = (uint32 *)(spr_data + sizeof(*spr_hdr) + spr_hdr->u16MetricsOffset);

  h = mtlk_hash_find_rf_mgmt(&rf_mgmt->peers, (struct eth_addr *)&spr_info->mac);
  if (h) {
    peer_node = MTLK_CONTAINER_OF(h, struct mtlk_rf_mgmt_peer_node, hentry);
  }
  else {
    uint32 peer_node_size = 
      MTLK_OFFSET_OF(struct mtlk_rf_mgmt_peer_node, private_data) + 
      rf_mgmt->api->get_peer_private_data_size();
      
    peer_node = 
      (struct mtlk_rf_mgmt_peer_node *)mtlk_osal_mem_alloc(peer_node_size, 
                                                           MTLK_MEM_TAG_RFMGMT);
    if (!peer_node) {
      ELOG_V("Can't allocate RF MGMT peer node!");
      goto end;
    }

    memset(peer_node, 0, peer_node_size);

    peer_node->current_data = MTLK_RF_MGMT_DATA_DEFAULT;

    /* Add peer to DB */
    mtlk_hash_insert_rf_mgmt(&rf_mgmt->peers, (struct eth_addr *)&spr_info->mac, &peer_node->hentry);
  }

  peer_node->ts = mtlk_osal_timestamp();

  _rf_mgmt_prepare_metrics(metrics, spr_hdr->u16MetricsSize);

  new_rf_mgmt_data = peer_node->current_data;
  res = rf_mgmt->api->handle_spr(rf_mgmt->api_ctx,
                                 &peer_node->private_data,
                                 spr_hdr,
                                 sp_data,
                                 metrics,
                                 &new_rf_mgmt_data);
  if (res != MTLK_ERR_OK) {
    ELOG_D("SPR handling failed (err=%d)", res);
    goto end;
  }

  if (new_rf_mgmt_data == peer_node->current_data) {
    if (debug) {
      rf_mgmt->api->log_peer(rf_mgmt->api_ctx,
                             &peer_node->private_data);
    }
    goto end; /* no change is required */
  }

  res = _rf_mgmt_set_peer_data(rf_mgmt,
                               (struct eth_addr *)&spr_info->mac,
                               new_rf_mgmt_data);
  if (res != MTLK_ERR_OK) {
    ELOG_D("Can't set peer RF MGMT data (err=%d)", res);
  }

  ILOG0_YDD("Peer (%Y) : RF Mgmt data changed: 0x%02x => 0x%02x",
       spr_info->mac,
       (unsigned)peer_node->current_data, 
       (unsigned)new_rf_mgmt_data);
  peer_node->current_data = new_rf_mgmt_data;
  rf_mgmt->api->log_peer(rf_mgmt->api_ctx,
                         &peer_node->private_data);

end:
  return;
}

static void
_rf_mgmt_get_and_handle_spr (struct mtlk_rf_mgmt *rf_mgmt)
{
  int  res = MTLK_ERR_UNKNOWN;
  struct mtlk_rf_mgmt_evt_get_spr *spr_info = 
    (struct mtlk_rf_mgmt_evt_get_spr *)rf_mgmt->spr_buff;

  spr_info->buffer_size = __rf_mgmt_sizeof_spr_evt_data_buff(rf_mgmt);
  spr_info->result      = MTLK_ERR_UNKNOWN;

  res = mtlk_irba_call_drv(rf_mgmt_cfg.irba, &IRBE_RF_MGMT_GET_SPR, 
                          spr_info, 
                          __rf_mgmt_sizeof_spr_evt_buff(rf_mgmt));
  if (res != MTLK_ERR_OK || spr_info->result != MTLK_ERR_OK) {
    WLOG_DD("Can't get SPR (res=%d, irb_res=%d)", res, spr_info->result);
    goto end;
  }

  ILOG2_V("SPR data received");

  _rf_mgmt_parse_spr(rf_mgmt, spr_info);

end:
  return;
}

static void
_rf_mgmt_handle_irb_notification (struct mtlk_rf_mgmt *rf_mgmt)
{
  /* Get & handle SPRs arrived */
  while (mtlk_osal_atomic_get(&rf_mgmt->spr_arrived)) {
    ILOG2_V("SPR");
    _rf_mgmt_get_and_handle_spr(rf_mgmt);
    mtlk_osal_atomic_dec(&rf_mgmt->spr_arrived);
  }

  /* Handle channel switch */
  if (rf_mgmt->ch_switched) {
    ILOG0_V("CH Switch");
    _rf_mgmt_on_ch_switched(rf_mgmt);
  }
}

static void
_rf_mgmt_send_sp (struct mtlk_rf_mgmt *rf_mgmt)
{
  int                              res = MTLK_ERR_UNKNOWN;
  struct mtlk_rf_mgmt_evt_send_sp *sp_info;
  void                            *sp_data; 
  rf_mgmt_data_t                   desired_rf_mgmt_data;
  uint8                            desired_rank;

  sp_info = (struct mtlk_rf_mgmt_evt_send_sp *)rf_mgmt->sp_buff;
  sp_data = mtlk_rf_mgmt_evt_send_sp_data(sp_info);

  /* Fill the SP data */
  res = rf_mgmt->api->fill_sp(rf_mgmt->api_ctx,
                              sp_data, 
                              rf_mgmt->sp_id,
                              &desired_rank,
                              &desired_rf_mgmt_data);
  
  /* Fill the SP Send event structure */
  sp_info->rf_mgmt_data = desired_rf_mgmt_data;
  sp_info->rank         = desired_rank;
  sp_info->data_size    = rf_mgmt->api->get_sp_size();
  sp_info->result       = MTLK_ERR_UNKNOWN;

  ILOG0_DDD("SP: id=%d aset=0x%02x rank=%d",
    rf_mgmt->sp_id, sp_info->rf_mgmt_data, sp_info->rank);

  ++rf_mgmt->sp_id;

  res = mtlk_irba_call_drv(rf_mgmt_cfg.irba, 
                          &IRBE_RF_MGMT_SEND_SP, 
                          sp_info, 
                          __rf_mgmt_sizeof_sp_evt_buff(rf_mgmt));
  if (res != MTLK_ERR_OK || sp_info->result != MTLK_ERR_OK) {
    WLOG_DD("Can't send SP (res=%d, irb_res=%d)", res, sp_info->result);
  }
}

#ifdef CLI_DBG_ON
static int
_rf_mgmt_get_type (uint8 *type)
{
  int                          res = MTLK_ERR_UNKNOWN;
  struct mtlk_rf_mgmt_evt_type evt; 

  MTLK_ASSERT(type != NULL);

  evt.result = MTLK_ERR_UNKNOWN;

  res = mtlk_irba_call_drv(rf_mgmt_cfg.irba, &IRBE_RF_MGMT_GET_TYPE, &evt, sizeof(evt));
  evt.type.u16Status = MAC_TO_HOST16(evt.type.u16Status);
  if (res != MTLK_ERR_OK) {
    WLOG_D("Can't get type (res=%d)", res);
  }
  else if (evt.result != MTLK_ERR_OK) {
    WLOG_D("Can't get type (irb_res=%d)", (int)evt.result);
    res = (int)evt.result;
  }
  else if (evt.type.u16Status != UMI_OK) {
    WLOG_D("Can't Enable (mac_res=%d)", (int)evt.type.u16Status);
    res = MTLK_ERR_MAC;
  }
  else {
    *type = evt.type.u8RFMType;
  }

  return res;
}
#endif /* #ifdef CLI_DBG_ON */

static int
_rf_mgmt_set_type (uint8 type)
{
  int                          res = MTLK_ERR_UNKNOWN;
  struct mtlk_rf_mgmt_evt_type evt; 

  evt.result = MTLK_ERR_UNKNOWN;

  evt.type.u8RFMType = type;
  evt.spr_queue_size = (type == MTLK_RF_MGMT_TYPE_OFF)?0:RF_MGMT_DEF_SPR_QUEUE_SIZE;

  ILOG0_D("Sending Type (%d)", type);

  res = mtlk_irba_call_drv(rf_mgmt_cfg.irba, &IRBE_RF_MGMT_SET_TYPE, &evt, sizeof(evt));
  evt.type.u16Status = MAC_TO_HOST16(evt.type.u16Status);
  if (res != MTLK_ERR_OK) {
    WLOG_D("Can't send Enable (res=%d)", res);
  }
  else if (evt.result != MTLK_ERR_OK) {
    WLOG_D("Can't Enable (irb_res=%d)", (int)evt.result);
    res = (int)evt.result;
  }
  else if (evt.type.u16Status != UMI_OK) {
    WLOG_D("Can't Enable (mac_res=%d)", (int)evt.type.u16Status);
    res = MTLK_ERR_MAC;
  }

  return res;
}

#ifdef CLI_DBG_ON
static int
_rf_mgmt_get_def_data (uint8 *rf_mgmt_data)
{
  int                              res = MTLK_ERR_UNKNOWN;
  struct mtlk_rf_mgmt_evt_def_data evt; 

  MTLK_ASSERT(rf_mgmt_data != NULL);

  evt.result = MTLK_ERR_UNKNOWN;

  res = mtlk_irba_call_drv(rf_mgmt_cfg.irba, &IRBE_RF_MGMT_GET_DEF_DATA, &evt, sizeof(evt));
  if (res != MTLK_ERR_OK) {
    WLOG_D("Can't get def data (res=%d)", res);
  }
  else if (evt.result != MTLK_ERR_OK) {
    WLOG_D("Can't get def data (irb_res=%d)", (int)evt.result);
    res = (int)evt.result;
  }
  else if (evt.data.u8Status != UMI_OK) {
    WLOG_D("Can't get data (mac_res=%d)", (int)evt.data.u8Status);
    res = MTLK_ERR_MAC;
  }
  else {
    *rf_mgmt_data = evt.data.u8Data;
  }

  return res;
}

static int
_rf_mgmt_set_def_data (uint8 rf_mgmt_data)
{
  int                              res = MTLK_ERR_UNKNOWN;
  struct mtlk_rf_mgmt_evt_def_data evt; 

  evt.data.u8Data = rf_mgmt_data;
  evt.result      = MTLK_ERR_UNKNOWN;

  res = mtlk_irba_call_drv(rf_mgmt_cfg.irba, &IRBE_RF_MGMT_SET_DEF_DATA, &evt, sizeof(evt));
  if (res != MTLK_ERR_OK) {
    WLOG_D("Can't set def data (res=%d)", res);
  }
  else if (evt.result != MTLK_ERR_OK) {
    WLOG_D("Can't set def data (irb_res=%d)", (int)evt.result);
    res = (int)evt.result;
  }
  else if (evt.data.u8Status != UMI_OK) {
    WLOG_D("Can't set data (mac_res=%d)", (int)evt.data.u8Status);
    res = MTLK_ERR_MAC;
  }

  return res;
}
#endif /* #ifdef CLI_DBG_ON */

static int32
_rf_mgmt_thread_proc (mtlk_handle_t context)
{
  struct mtlk_rf_mgmt *rf_mgmt    = HANDLE_T_PTR(struct mtlk_rf_mgmt, context);
  uint32               wait_ms    = rf_mgmt->cfg.refresh_time_ms;
  uint32               last_sp_ms = 0;
  uint32               last_ic_ms = 0; /* inactivity check timestamp */
  BOOL                 error      = FALSE;
  BOOL                 rf_mgmt_on = FALSE;
  uint32               count      = 0;

  MTLK_ASSERT(rf_mgmt != NULL);
  MTLK_ASSERT(rf_mgmt->cfg.rf_mgmt_type != MTLK_RF_MGMT_TYPE_OFF);

  /* Enable the Antenna Selection in Driver and MAC */
  for (count = 0; count < RF_MGMT_MAX_ENABLE_ATTEMPTS; ++count) {
    if (rf_mgmt->stop_signaled) {
      ILOG0_V("Stop signalled! Exiting thread.");
      break;
    }

    if (_rf_mgmt_set_type(rf_mgmt->cfg.rf_mgmt_type) == MTLK_ERR_OK) {
        rf_mgmt_on = TRUE;
        break;
    }

    WLOG_DD("Can't enable RF MGMT (%u attempts of %u)...",
      count, RF_MGMT_MAX_ENABLE_ATTEMPTS);
    mtlk_osal_msleep(RF_MGMT_MAX_ENABLE_PERIOD);
  }

  if (count == RF_MGMT_MAX_ENABLE_ATTEMPTS) {
    ELOG_D("Can't enable RF MGMT (%u attempts made)", count);
    error = TRUE;
  }

  /* Pump the SP/SPR loop */
  last_sp_ms = mtlk_osal_timestamp_to_ms(mtlk_osal_timestamp());
  while (!error) {
    int do_refresh = 0;
    int wait_res   = mtlk_osal_event_wait(&rf_mgmt->evt, wait_ms);

    if (rf_mgmt->stop_signaled) {
      ILOG0_V("Stop signalled! Exiting thread.");
      break;
    }

    switch (wait_res) {
    case MTLK_ERR_OK: 
      {/* event signaled an IRB event arrived*/
        uint32 now_ms;

        mtlk_osal_event_reset(&rf_mgmt->evt);

        ILOG2_V("Event signalled");

        _rf_mgmt_handle_irb_notification(rf_mgmt);

        /***************************************************************
        * Re-calculate next wait to guaranty the SP sending period.
        * Send SP if required.
        ***************************************************************/
        now_ms = mtlk_osal_timestamp_to_ms(mtlk_osal_timestamp());
        if (now_ms - last_sp_ms >= rf_mgmt->cfg.refresh_time_ms) {
          do_refresh = 1;
        }
        else {
          wait_ms = last_sp_ms + rf_mgmt->cfg.refresh_time_ms - now_ms;
        }
        /***************************************************************/
      }
      break;
    case MTLK_ERR_TIMEOUT: /* timeout */
      do_refresh = 1; /* do refresh (send sp) */
      break;
    default:
      ELOG_D("Unexpected Evt wait result %d. Exiting thread.", wait_res);
      error = TRUE;
      break;
    }

    if (do_refresh) {
      /* Send next SP if required */
      _rf_mgmt_send_sp(rf_mgmt);
      wait_ms    = rf_mgmt->cfg.refresh_time_ms;
      last_sp_ms = mtlk_osal_timestamp_to_ms(mtlk_osal_timestamp());

      /* Check inactivity if required */
      if (last_sp_ms - last_ic_ms > RF_MGMT_INACTIVITY_CHECK_PEROD_MS(rf_mgmt)) {
        _rf_mgmt_check_inactivity(rf_mgmt);
        last_ic_ms = last_sp_ms;
      }
    }
  }

  /* Disable the Antenna Selection in Driver and MAC */
  if (rf_mgmt_on) {
    _rf_mgmt_set_type(MTLK_RF_MGMT_TYPE_OFF);
  }

  return 0;
}

static void __MTLK_IFUNC
_rf_mgmt_irbh_spr_arrived (mtlk_irba_t       *irba,
                           mtlk_handle_t      context,
                           const mtlk_guid_t *evt,
                           void              *buffer,
                           uint32            size)
{
  struct mtlk_rf_mgmt              *rf_mgmt = HANDLE_T_PTR(struct mtlk_rf_mgmt, context);
#ifdef MTCFG_DEBUG
  struct mtlk_rf_mgmt_evt_spr_arrived *ievt = 
    (struct mtlk_rf_mgmt_evt_spr_arrived *)buffer;
#endif

  MTLK_ASSERT(mtlk_guid_compare(&IRBE_RF_MGMT_SPR_ARRIVED, evt) == 0);
  MTLK_ASSERT(size == sizeof(*ievt));

  mtlk_osal_atomic_inc(&rf_mgmt->spr_arrived);
  mtlk_osal_event_set(&rf_mgmt->evt);
}

MTLK_START_STEPS_LIST_BEGIN(rf_mgmt)
  MTLK_START_STEPS_LIST_ENTRY(rf_mgmt, API_CONSTRUCT)
  MTLK_START_STEPS_LIST_ENTRY(rf_mgmt, EVT_INIT)
  MTLK_START_STEPS_LIST_ENTRY(rf_mgmt, HASH_INIT)
  MTLK_START_STEPS_LIST_ENTRY(rf_mgmt, HASH_FILL) /* dummy, for STOP only */
  MTLK_START_STEPS_LIST_ENTRY(rf_mgmt, THREAD_INIT)
  MTLK_START_STEPS_LIST_ENTRY(rf_mgmt, SPR_BUFF_ALLOC)
  MTLK_START_STEPS_LIST_ENTRY(rf_mgmt, SP_BUFF_ALLOC)
  MTLK_START_STEPS_LIST_ENTRY(rf_mgmt, IRB_REGISTER_ON_SPR)
  MTLK_START_STEPS_LIST_ENTRY(rf_mgmt, THREAD_RUN)
MTLK_START_INNER_STEPS_BEGIN(rf_mgmt)
MTLK_START_STEPS_LIST_END(rf_mgmt)

static void
_rf_mgmt_stop_thread (struct mtlk_rf_mgmt *rf_mgmt)
{
  /* Signal thread to stop */
  rf_mgmt->stop_signaled = 1;
  mtlk_osal_event_set(&rf_mgmt->evt);

  /* Wait until thread finishes */
  mtlk_osal_thread_wait(&rf_mgmt->thread, NULL);
}

static void
_rf_mgmt_cleanup (struct mtlk_rf_mgmt *rf_mgmt)
{
  MTLK_STOP_BEGIN(rf_mgmt,  MTLK_OBJ_PTR(rf_mgmt))
    MTLK_STOP_STEP(rf_mgmt, THREAD_RUN, MTLK_OBJ_PTR(rf_mgmt),
                   _rf_mgmt_stop_thread, (rf_mgmt));
    MTLK_STOP_STEP(rf_mgmt, IRB_REGISTER_ON_SPR, MTLK_OBJ_PTR(rf_mgmt),
                   mtlk_irba_unregister, (rf_mgmt_cfg.irba, HANDLE_T_PTR(mtlk_irba_handle_t, rf_mgmt->spr_irb_ctx)));
    MTLK_STOP_STEP(rf_mgmt, SP_BUFF_ALLOC, MTLK_OBJ_PTR(rf_mgmt),
                   mtlk_osal_mem_free, (rf_mgmt->sp_buff));
    MTLK_STOP_STEP(rf_mgmt, SPR_BUFF_ALLOC, MTLK_OBJ_PTR(rf_mgmt),
                   mtlk_osal_mem_free, (rf_mgmt->spr_buff));
    MTLK_STOP_STEP(rf_mgmt, THREAD_INIT, MTLK_OBJ_PTR(rf_mgmt),
                   mtlk_osal_thread_cleanup, (&rf_mgmt->thread));
    MTLK_STOP_STEP(rf_mgmt, HASH_FILL, MTLK_OBJ_PTR(rf_mgmt),
                   _rf_mgmt_empty_hash, (rf_mgmt));
    MTLK_STOP_STEP(rf_mgmt, HASH_INIT, MTLK_OBJ_PTR(rf_mgmt),
                   mtlk_hash_cleanup_rf_mgmt, (&rf_mgmt->peers));
    MTLK_STOP_STEP(rf_mgmt, EVT_INIT, MTLK_OBJ_PTR(rf_mgmt),
                   mtlk_osal_event_cleanup, (&rf_mgmt->evt));
    MTLK_STOP_STEP(rf_mgmt, API_CONSTRUCT, MTLK_OBJ_PTR(rf_mgmt),
                   rf_mgmt->api->destructor, (rf_mgmt->api_ctx));
  MTLK_STOP_END(rf_mgmt,  MTLK_OBJ_PTR(rf_mgmt))

  memset(rf_mgmt, 0, sizeof(*rf_mgmt));
}

static int
_rf_mgmt_init (struct mtlk_rf_mgmt           *rf_mgmt,
               const mtlk_rf_mgmt_obj_vft_t  *api,
               const struct mtlk_rf_mgmt_cfg *cfg)
{
  MTLK_ASSERT(rf_mgmt != NULL);
  MTLK_ASSERT(api != NULL);
  MTLK_ASSERT(cfg != NULL);

  rf_mgmt->api = api;
  rf_mgmt->cfg = *cfg;

  MTLK_START_TRY(rf_mgmt,  MTLK_OBJ_PTR(rf_mgmt))
    MTLK_START_STEP_EX(rf_mgmt, API_CONSTRUCT, MTLK_OBJ_PTR(rf_mgmt),
                       rf_mgmt->api->constructor, (&rf_mgmt->cfg),
                       rf_mgmt->api_ctx,
                       rf_mgmt->api_ctx != HANDLE_T(0),
                       MTLK_ERR_NO_RESOURCES);
    MTLK_START_STEP(rf_mgmt, EVT_INIT, MTLK_OBJ_PTR(rf_mgmt),
                    mtlk_osal_event_init, (&rf_mgmt->evt));
    MTLK_START_STEP(rf_mgmt, HASH_INIT, MTLK_OBJ_PTR(rf_mgmt),
                    mtlk_hash_init_rf_mgmt, (&rf_mgmt->peers, 
                                             RF_MGMT_SPR_HASH_MAX_IDX));
    MTLK_START_STEP_VOID(rf_mgmt, HASH_FILL, MTLK_OBJ_PTR(rf_mgmt),
                         MTLK_NOACTION, ());
    MTLK_START_STEP(rf_mgmt, THREAD_INIT, MTLK_OBJ_PTR(rf_mgmt),
                    mtlk_osal_thread_init, (&rf_mgmt->thread));
    MTLK_START_STEP_EX(rf_mgmt, SPR_BUFF_ALLOC, MTLK_OBJ_PTR(rf_mgmt),
                       mtlk_osal_mem_alloc, (__rf_mgmt_sizeof_spr_evt_buff(rf_mgmt),
                                             MTLK_MEM_TAG_RFMGMT),
                       rf_mgmt->spr_buff,
                       rf_mgmt->spr_buff != NULL,
                       MTLK_ERR_NO_MEM);
    MTLK_START_STEP_EX(rf_mgmt, SP_BUFF_ALLOC, MTLK_OBJ_PTR(rf_mgmt),
                       mtlk_osal_mem_alloc, (__rf_mgmt_sizeof_sp_evt_buff(rf_mgmt),
                                             MTLK_MEM_TAG_RFMGMT),
                       rf_mgmt->sp_buff,
                       rf_mgmt->sp_buff != NULL,
                       MTLK_ERR_NO_MEM);
    MTLK_START_STEP_EX(rf_mgmt, IRB_REGISTER_ON_SPR, MTLK_OBJ_PTR(rf_mgmt),
                       mtlk_irba_register, (rf_mgmt_cfg.irba,
                                            &IRBE_RF_MGMT_SPR_ARRIVED, 
                                            1,
                                            _rf_mgmt_irbh_spr_arrived, 
                                            HANDLE_T(rf_mgmt)),
                       rf_mgmt->spr_irb_ctx,
                       rf_mgmt->spr_irb_ctx != NULL,
                       MTLK_ERR_NO_RESOURCES);
    MTLK_START_STEP(rf_mgmt, THREAD_RUN, MTLK_OBJ_PTR(rf_mgmt),
                    mtlk_osal_thread_run, (&rf_mgmt->thread,
                                           _rf_mgmt_thread_proc,
                                           HANDLE_T(rf_mgmt)));
  MTLK_START_FINALLY(rf_mgmt,  MTLK_OBJ_PTR(rf_mgmt))
  MTLK_START_RETURN(rf_mgmt,  MTLK_OBJ_PTR(rf_mgmt), _rf_mgmt_cleanup, (rf_mgmt))
}

static void
_rf_mgmt_delete (struct mtlk_rf_mgmt *rf_mgmt)
{
  MTLK_ASSERT(rf_mgmt != NULL);

  _rf_mgmt_cleanup(rf_mgmt);
   mtlk_osal_mem_free(rf_mgmt);
}

static struct mtlk_rf_mgmt *
_rf_mgmt_create (const mtlk_rf_mgmt_obj_vft_t  *api,
                 const struct mtlk_rf_mgmt_cfg *cfg)
{
  struct mtlk_rf_mgmt *rf_mgmt = mtlk_osal_mem_alloc(sizeof(*rf_mgmt),
                                                     MTLK_MEM_TAG_RFMGMT);
  if (rf_mgmt != NULL) {
    memset(rf_mgmt, 0, sizeof(*rf_mgmt));

    if (_rf_mgmt_init(rf_mgmt, api, cfg) == MTLK_ERR_OK) {
      ILOG0_D("RF MGMT (type#%d) initialized", (int)cfg->rf_mgmt_type);
    }
    else {
      ELOG_D("Can't start RF MGMT (type#%d)", (int)cfg->rf_mgmt_type);
      mtlk_osal_mem_free(rf_mgmt);
      rf_mgmt = NULL;
    }
  }
  else {
    ELOG_DD("Can't allocate RF MGMT object (type#%d) of %d bytes",
          (int)cfg->rf_mgmt_type, sizeof(*rf_mgmt));
  }

  return rf_mgmt;
}

enum rf_mgmt_mode
{
  RF_MGMT_MODE_ENGINE, /* This module provides RF MGMT selection engine */
  RF_MGMT_MODE_SWITCH, /* This module only switches RF MGMT on/off      */
  RF_MGMT_MODE_STUB,   /* This module doesn't deal with RF MGMT at all  */
#ifdef CLI_DBG_ON
  RF_MGMT_MODE_DEBUG   /* This module provides RF MGMT debug CLI        */
#endif
};

static enum rf_mgmt_mode mode = RF_MGMT_MODE_DEBUG;

#ifdef CLI_DBG_ON

static mtlk_atomic_t debug_sp_id;

extern mtlk_cli_srv_t *app_cli_srv;

static int
__rf_mgmt_dbg_cli_check_int_params (const mtlk_cli_srv_cmd_t *cmd,
                                    mtlk_cli_srv_rsp_t       *rsp,
                                    int32                    *vals,
                                    uint32                    nof_vals)
{
  int    res        = MTLK_ERR_UNKNOWN;
  uint32 nof_params = mtlk_cli_srv_cmd_get_nof_params(cmd);
  uint32 i;

  MTLK_ASSERT(nof_vals == 0 || vals != NULL);

  if (nof_params != nof_vals) {
    ELOG_DD("Wrong number of parameters: %u (%u expected)", nof_params, nof_vals);
    res = MTLK_ERR_PARAMS;
    goto end;
  }

  for (i = 0; i < nof_vals; i++) {
    vals[i] = mtlk_cli_srv_cmd_get_param_int(cmd, i, &res);
    if (res != MTLK_ERR_OK) {
      ELOG_DS("Invalid parameter#%u (%s)",
            i, mtlk_cli_srv_cmd_get_param(cmd, i));
      res = MTLK_ERR_PARAMS;
      goto end;
    }
  }

  res = MTLK_ERR_OK;

end:
  return res;
}

static int
__rf_mgmt_dbg_cli_check_int_params_ex (const mtlk_cli_srv_cmd_t *cmd,
                                       mtlk_cli_srv_rsp_t       *rsp,
                                       int32                    *vals,
                                       uint32                    nof_vals,
                                       int32                     min_val,
                                       int32                     max_val)
{
  int res = __rf_mgmt_dbg_cli_check_int_params(cmd, rsp, vals, nof_vals);
  int i;

  if (res != MTLK_ERR_OK) {
    goto end;
  }

  for (i = 0; i < nof_vals; i++) {
    if (vals[i] < min_val || vals[i] > max_val) {
      ELOG_DDD("Parameter#%u not in range [%d...%d]", i, min_val, max_val);
      res = MTLK_ERR_PARAMS;
      goto end;
    }
  }

  res = MTLK_ERR_OK;

end:
  return res;
}



static void __MTLK_IFUNC
_rf_mgmt_dbg_cli_cmd_clb_set_type (mtlk_cli_srv_t           *srv,
                                   const mtlk_cli_srv_cmd_t *cmd,
                                   mtlk_cli_srv_rsp_t       *rsp,
                                   mtlk_handle_t             ctx)
{
  int    res = MTLK_ERR_UNKNOWN;
  int32  val;

  res = __rf_mgmt_dbg_cli_check_int_params_ex(cmd, rsp, &val,
                                              1, 0, MAX_UINT8);
  if (res != MTLK_ERR_OK) {
    goto end;
  }
  
  res = _rf_mgmt_set_type((uint8)val);

end:
  if (res != MTLK_ERR_OK) {
    mtlk_cli_srv_rsp_set_error(rsp, res);
  }
}

static void __MTLK_IFUNC
_rf_mgmt_dbg_cli_cmd_clb_get_type (mtlk_cli_srv_t           *srv,
                                   const mtlk_cli_srv_cmd_t *cmd,
                                   mtlk_cli_srv_rsp_t       *rsp,
                                   mtlk_handle_t             ctx)
{
  int    res        = MTLK_ERR_UNKNOWN;
  uint8  val = 0;

  res = __rf_mgmt_dbg_cli_check_int_params(cmd, rsp, NULL, 0);
  if (res != MTLK_ERR_OK) {
    goto end;
  }

  res = _rf_mgmt_get_type(&val);
  if (res != MTLK_ERR_OK) {
    goto end;
  }

  res = mtlk_cli_srv_rsp_add_strf(rsp, "%u", (unsigned)val);
  if (res != MTLK_ERR_OK) {
    ELOG_V("Can't add response string");
    goto end;
  }

  res = MTLK_ERR_OK;

end:
  if (res != MTLK_ERR_OK) {
    mtlk_cli_srv_rsp_set_error(rsp, res);
  }
}

static void __MTLK_IFUNC
_rf_mgmt_dbg_cli_cmd_clb_set_def_data (mtlk_cli_srv_t           *srv,
                                       const mtlk_cli_srv_cmd_t *cmd,
                                       mtlk_cli_srv_rsp_t       *rsp,
                                       mtlk_handle_t             ctx)
{
  int    res = MTLK_ERR_UNKNOWN;
  int32  val;

  res = __rf_mgmt_dbg_cli_check_int_params_ex(cmd, rsp, &val,
                                              1, 0, MAX_UINT8);
  if (res != MTLK_ERR_OK) {
    goto end;
  }

  res = _rf_mgmt_set_def_data((uint8)val);

end:
  if (res != MTLK_ERR_OK) {
    mtlk_cli_srv_rsp_set_error(rsp, res);
  }
}

static void __MTLK_IFUNC
_rf_mgmt_dbg_cli_cmd_clb_get_def_data (mtlk_cli_srv_t           *srv,
                                       const mtlk_cli_srv_cmd_t *cmd,
                                       mtlk_cli_srv_rsp_t       *rsp,
                                       mtlk_handle_t             ctx)
{
  int    res = MTLK_ERR_UNKNOWN;
  uint8  val = 0;

  res = __rf_mgmt_dbg_cli_check_int_params(cmd, rsp, NULL, 0);
  if (res != MTLK_ERR_OK) {
    goto end;
  }

  res = _rf_mgmt_get_def_data(&val);
  if (res != MTLK_ERR_OK) {
    goto end;
  }

  res = mtlk_cli_srv_rsp_add_strf(rsp, "0x%02x", (unsigned)val);
  if (res != MTLK_ERR_OK) {
    ELOG_V("Can't add response string");
    goto end;
  }

  res = MTLK_ERR_OK;

end:
  if (res != MTLK_ERR_OK) {
    mtlk_cli_srv_rsp_set_error(rsp, res);
  }
}

struct rf_mgmt_dbg_sp_data
{
  uint32 id;
  uint8  rank;
  uint8  rf_mgmt_data;
};

struct rf_mgmt_dbg_sp
{
  struct mtlk_rf_mgmt_evt_send_sp hdr;
  struct rf_mgmt_dbg_sp_data      data;
};

static void __MTLK_IFUNC
_rf_mgmt_dbg_cli_cmd_clb_send_sp (mtlk_cli_srv_t           *srv,
                                  const mtlk_cli_srv_cmd_t *cmd,
                                  mtlk_cli_srv_rsp_t       *rsp,
                                  mtlk_handle_t             ctx)
{
  int                   res = MTLK_ERR_UNKNOWN;
  int32                 vals[2];
  struct rf_mgmt_dbg_sp dbg_sp;

  res = __rf_mgmt_dbg_cli_check_int_params_ex(cmd, rsp, vals,
                                              ARRAY_SIZE(vals), 0, MAX_UINT8);
  if (res != MTLK_ERR_OK) {
    goto end;
  }

  dbg_sp.data.id           = mtlk_osal_atomic_inc(&debug_sp_id);
  dbg_sp.data.rf_mgmt_data =(uint8)vals[0];
  dbg_sp.data.rank         =(uint8)vals[1];
  
  dbg_sp.hdr.rf_mgmt_data = dbg_sp.data.rf_mgmt_data;
  dbg_sp.hdr.rank         = dbg_sp.data.rank;
  dbg_sp.hdr.data_size    = sizeof(dbg_sp.data);
  dbg_sp.hdr.result       = MTLK_ERR_UNKNOWN;

  res = mtlk_irba_call_drv(rf_mgmt_cfg.irba, 
                           &IRBE_RF_MGMT_SEND_SP, 
                           &dbg_sp, 
                           sizeof(dbg_sp));
  if (res != MTLK_ERR_OK) {
    ELOG_DD("Can't send SP (res=%d, irb_res=%d)", res, dbg_sp.hdr.result);
  }
  else if (dbg_sp.hdr.result != MTLK_ERR_OK) {
    ELOG_DD("Can't send SP (res=%d, irb_res=%d)", res, dbg_sp.hdr.result);
    res = (int)dbg_sp.hdr.result;
  }

end:
  if (res != MTLK_ERR_OK) {
    mtlk_cli_srv_rsp_set_error(rsp, res);
  }
}

struct rf_mgmt_spr
{
  struct mtlk_rf_mgmt_evt_get_spr hdr;
  uint8                           data[sizeof(MTLK_VSAF_SPR_ITEM_HDR) + 
                                       sizeof(struct rf_mgmt_dbg_sp_data) + 
                                       MAX_METRICS_SIZE];
};

static void __MTLK_IFUNC
_rf_mgmt_dbg_cli_cmd_clb_get_sprs (mtlk_cli_srv_t           *srv,
                                   const mtlk_cli_srv_cmd_t *cmd,
                                   mtlk_cli_srv_rsp_t       *rsp,
                                   mtlk_handle_t             ctx)
{
  int                         res = MTLK_ERR_UNKNOWN;
  struct rf_mgmt_spr          dbg_spr;
  MTLK_VSAF_SPR_ITEM_HDR     *spr_hdr;
  struct rf_mgmt_dbg_sp_data *sp_data;
  uint32                     *metrics;
  uint16                      nof_u32;
  uint32                      effective_snr   = 0;
  uint8                       mcs_feedback    = 0;
  uint32                      short_cp_metric = 0;

  res = __rf_mgmt_dbg_cli_check_int_params(cmd, rsp, NULL, 0);
  if (res != MTLK_ERR_OK) {
    goto end;
  }

  dbg_spr.hdr.buffer_size = sizeof(dbg_spr.data);
  dbg_spr.hdr.result      = MTLK_ERR_UNKNOWN;

  res = mtlk_irba_call_drv(rf_mgmt_cfg.irba,
                           &IRBE_RF_MGMT_GET_SPR, 
                           &dbg_spr, 
                           sizeof(dbg_spr));
  if (res != MTLK_ERR_OK) {
    WLOG_D("Can't get SPR (res=%d)", res);
    goto end;
  }

  if (dbg_spr.hdr.result != MTLK_ERR_OK) {
    WLOG_D("Can't get SPR (irb_res=%d)", (int)dbg_spr.hdr.result);
    res = (int)dbg_spr.hdr.result;
    goto end;
  }

  spr_hdr  = (MTLK_VSAF_SPR_ITEM_HDR *)dbg_spr.data;

  spr_hdr->u16Version       = WLAN_TO_HOST16(spr_hdr->u16Version);
  spr_hdr->u16SPDataOffset  = WLAN_TO_HOST16(spr_hdr->u16SPDataOffset);
  spr_hdr->u16SPDataSize    = WLAN_TO_HOST16(spr_hdr->u16SPDataSize);
  spr_hdr->u16MetricsOffset = WLAN_TO_HOST16(spr_hdr->u16MetricsOffset);
  spr_hdr->u16MetricsSize   = WLAN_TO_HOST16(spr_hdr->u16MetricsSize);

  sp_data  = (struct rf_mgmt_dbg_sp_data *)(dbg_spr.data + 
                                            sizeof(*spr_hdr) + 
                                            spr_hdr->u16SPDataOffset);
  metrics  = (uint32 *)(dbg_spr.data + sizeof(*spr_hdr) + spr_hdr->u16MetricsOffset);

  nof_u32  = spr_hdr->u16MetricsSize/sizeof(uint32);
  _rf_mgmt_prepare_metrics(metrics, spr_hdr->u16MetricsSize/nof_u32);

  /* Check SPR Data version */
  switch (spr_hdr->u16Version)
  {
  case SPR_DATA_VERSION_1:
    if (spr_hdr->u16MetricsSize == sizeof(ASL_SHRAM_METRIC_T)) {
      effective_snr = mtlk_metrics_calc_effective_snr((ASL_SHRAM_METRIC_T *)metrics, 
                                                      &mcs_feedback,
                                                      &short_cp_metric);
    }
    else {
      WLOG_DD("Metrics of incorrect size (%u != %d)",
              spr_hdr->u16MetricsSize,
              sizeof(ASL_SHRAM_METRIC_T));
    }
    break;
  case SPR_DATA_VERSION_2:
    if (spr_hdr->u16MetricsSize == sizeof(RFM_SHRAM_METRIC_T)) {
      RFM_SHRAM_METRIC_T *m = (RFM_SHRAM_METRIC_T *)metrics;

      effective_snr = MTLK_BFIELD_GET(m->effectiveSNR, PHY_ESNR_VAL);
      mcs_feedback  = (uint8)MTLK_BFIELD_GET(m->effectiveSNR, PHY_MCSF_VAL);
    }
    else {
      WLOG_DD("Metrics of incorrect size (%u != %d)",
        spr_hdr->u16MetricsSize,
        sizeof(RFM_SHRAM_METRIC_T));
    }
    break;
  default:
    WLOG_D("SPR of unsupported version received (%d)",
            spr_hdr->u16Version);
    return;  
  }

  mtlk_cli_srv_rsp_add_strf(rsp, MAC_PRINTF_FMT ",%u,%u,%u,%u,%u,%u,%u,%u,%u",
                            MAC_PRINTF_ARG(dbg_spr.hdr.mac),
                            sp_data->id,
                            sp_data->rf_mgmt_data,
                            sp_data->rank,
                            spr_hdr->u8SPIsCB,
                            spr_hdr->u8SPMcs,
                            effective_snr,
                            mcs_feedback,
                            short_cp_metric,
                            nof_u32);

end:
  if (res != MTLK_ERR_OK) {
    mtlk_cli_srv_rsp_set_error(rsp, res);
  }
}

struct rf_mgmt_dbg_cli_cmd
{
  const char            *name;
  mtlk_cli_srv_cmd_clb_f clb;
};

static const struct rf_mgmt_dbg_cli_cmd 
rf_mgmt_dbg_cli_cmd[] = {
  { "sRFMgmtType",    _rf_mgmt_dbg_cli_cmd_clb_set_type     },
  { "gRFMgmtType",    _rf_mgmt_dbg_cli_cmd_clb_get_type     },
  { "sRFMgmtDefData", _rf_mgmt_dbg_cli_cmd_clb_set_def_data },
  { "gRFMgmtDefData", _rf_mgmt_dbg_cli_cmd_clb_get_def_data },
  { "sRFMgmtSP",      _rf_mgmt_dbg_cli_cmd_clb_send_sp      },
  { "gRFMgmtSPRs",    _rf_mgmt_dbg_cli_cmd_clb_get_sprs     },
};

MTLK_INIT_STEPS_LIST_BEGIN(rf_mgmt_dbg)
  MTLK_INIT_STEPS_LIST_ENTRY(rf_mgmt_dbg, CLI_SRV_REG_CMDS)
MTLK_INIT_INNER_STEPS_BEGIN(rf_mgmt_dbg)
MTLK_INIT_STEPS_LIST_END(rf_mgmt_dbg)

struct mtlk_rf_mgmt_dbg
{
  mtlk_cli_srv_clb_t *clb_handle[ARRAY_SIZE(rf_mgmt_dbg_cli_cmd)];
  MTLK_DECLARE_INIT_STATUS;
  MTLK_DECLARE_INIT_LOOP(CLI_SRV_REG_CMDS);
};

static void
_rf_mgmt_dbg_cleanup (struct mtlk_rf_mgmt_dbg *rf_mgmt_dbg)
{
  int i;
  MTLK_CLEANUP_BEGIN(rf_mgmt_dbg, MTLK_OBJ_PTR(rf_mgmt_dbg))
    for (i = 0; MTLK_CLEANUP_ITERATONS_LEFT(MTLK_OBJ_PTR(rf_mgmt_dbg), CLI_SRV_REG_CMDS) > 0; i++) {
      MTLK_ASSERT(NULL != rf_mgmt_dbg->clb_handle[i]);
      MTLK_CLEANUP_STEP_LOOP(
        rf_mgmt_dbg, CLI_SRV_REG_CMDS, MTLK_OBJ_PTR(rf_mgmt_dbg),
        mtlk_cli_srv_unregister_cmd_clb, (rf_mgmt_dbg->clb_handle[i])
      );
    }
  MTLK_CLEANUP_END(rf_mgmt_dbg, MTLK_OBJ_PTR(rf_mgmt_dbg))
}

static int
_rf_mgmt_dbg_init (struct mtlk_rf_mgmt_dbg *rf_mgmt_dbg)
{
  int i;
  MTLK_INIT_TRY(rf_mgmt_dbg, MTLK_OBJ_PTR(rf_mgmt_dbg))
    for (i = 0; i < ARRAY_SIZE(rf_mgmt_dbg->clb_handle); i++) {
      MTLK_INIT_STEP_LOOP_EX(
        rf_mgmt_dbg, CLI_SRV_REG_CMDS, MTLK_OBJ_PTR(rf_mgmt_dbg),
        mtlk_cli_srv_register_cmd_clb, (app_cli_srv,
                                        rf_mgmt_dbg_cli_cmd[i].name,
                                        rf_mgmt_dbg_cli_cmd[i].clb,
                                        HANDLE_T(0)),
        rf_mgmt_dbg->clb_handle[i],
        rf_mgmt_dbg->clb_handle[i] != NULL,
        MTLK_ERR_NO_MEM
      );
    }
  MTLK_INIT_FINALLY(rf_mgmt_dbg, MTLK_OBJ_PTR(rf_mgmt_dbg))
  MTLK_INIT_RETURN(rf_mgmt_dbg, MTLK_OBJ_PTR(rf_mgmt_dbg), _rf_mgmt_dbg_cleanup, (rf_mgmt_dbg))
}

static void
_rf_mgmt_dbg_delete (struct mtlk_rf_mgmt_dbg *rf_mgmt_dbg)
{
  MTLK_ASSERT(rf_mgmt_dbg != NULL);

  _rf_mgmt_dbg_cleanup(rf_mgmt_dbg);
  mtlk_osal_mem_free(rf_mgmt_dbg);
}

static struct mtlk_rf_mgmt_dbg *
_rf_mgmt_dbg_create (void)
{
  struct mtlk_rf_mgmt_dbg *rf_mgmt_dbg = 
    mtlk_osal_mem_alloc(sizeof(*rf_mgmt_dbg),
                        MTLK_MEM_TAG_RFMGMT);

  if (rf_mgmt_dbg != NULL) {
    memset(rf_mgmt_dbg, 0, sizeof(*rf_mgmt_dbg));

    if (_rf_mgmt_dbg_init(rf_mgmt_dbg) == MTLK_ERR_OK) {
      ILOG0_V("DBG RF MGMT initialized!");
    }
    else {
      ELOG_V("Can't start DBG RF MGMT");
      mtlk_osal_mem_free(rf_mgmt_dbg);
      rf_mgmt_dbg = NULL;
    }
  }
  else {
    ELOG_D("Can't allocate DBG RF MGMT object of %d bytes", sizeof(*rf_mgmt_dbg));
  }

  return rf_mgmt_dbg;
}
#endif /* #ifdef CLI_DBG_ON */

static mtlk_handle_t
rf_mgmt_start (void)
{
  mtlk_handle_t                  res = HANDLE_T(0);
  const mtlk_rf_mgmt_obj_vft_t  *api = NULL;
  const struct mtlk_rf_mgmt_cfg *cfg = &rf_mgmt_cfg;

  switch (cfg->rf_mgmt_type) {
  case MTLK_RF_MGMT_TYPE_OFF:
    mode = RF_MGMT_MODE_DEBUG;
    break;
#ifdef ASEL_ON
  case MTLK_RF_MGMT_TYPE_MTLK_ASEL:
    api  = &mtlk_rf_mgmt_asel_obj_vft;
    mode = RF_MGMT_MODE_ENGINE;
    break;
#endif
#ifdef LBF_ON
  case MTLK_RF_MGMT_TYPE_MTLK_LBF:
    api  = &mtlk_rf_mgmt_lbf_obj_vft;
    mode = RF_MGMT_MODE_ENGINE;
    break;
#endif
  case MTLK_RF_MGMT_TYPE_LBF:
    mode = RF_MGMT_MODE_SWITCH;
    break;
  case MTLK_RF_MGMT_TYPE_AIRGAIN_ASEL:
    mode = RF_MGMT_MODE_STUB;
    break;
  default:
    WLOG_D("Unsupported RF Management type: %d. Forcing TYPE_OFF",
            (int)cfg->rf_mgmt_type);
    break;
  }

  switch (mode) {
  case RF_MGMT_MODE_ENGINE:
    {
      struct mtlk_rf_mgmt *rf_mgmt = _rf_mgmt_create(api, cfg);

      res = HANDLE_T(rf_mgmt);
    }
    break;
  case RF_MGMT_MODE_SWITCH:
    if (_rf_mgmt_set_type(cfg->rf_mgmt_type) == MTLK_ERR_OK) {
      res = HANDLE_T(TRUE);
    }
    break;
  case RF_MGMT_MODE_STUB:
    res = HANDLE_T(TRUE);
    break;
#ifdef CLI_DBG_ON
  case RF_MGMT_MODE_DEBUG:
    {
      struct mtlk_rf_mgmt_dbg *rf_mgmt_dbg = _rf_mgmt_dbg_create();

      res = HANDLE_T(rf_mgmt_dbg);
    }
    break;
#endif
  default:
    break;
  }

  return res;
}

static void
rf_mgmt_stop (mtlk_handle_t ctx)
{
  MTLK_ASSERT(ctx != HANDLE_T(0));

  switch (mode) {
  case RF_MGMT_MODE_ENGINE:
    {
      struct mtlk_rf_mgmt *rf_mgmt = HANDLE_T_PTR(struct mtlk_rf_mgmt, ctx);

      _rf_mgmt_delete(rf_mgmt);
    }
    break;
  case RF_MGMT_MODE_SWITCH:
    _rf_mgmt_set_type(MTLK_RF_MGMT_TYPE_OFF);
    break;
  case RF_MGMT_MODE_STUB:
    break;
#ifdef CLI_DBG_ON
  case RF_MGMT_MODE_DEBUG:
    {
      struct mtlk_rf_mgmt_dbg *rf_mgmt_dbg = HANDLE_T_PTR(struct mtlk_rf_mgmt_dbg, ctx);

      _rf_mgmt_dbg_delete(rf_mgmt_dbg);
    }
    break;
#endif
  default:
    MTLK_ASSERT("Invalid mode");
    break;
  }
}

const mtlk_component_api_t
rf_mgmt_api = {
  rf_mgmt_start,
  NULL,
  rf_mgmt_stop
};

struct mtlk_rf_mgmt_cfg 
  rf_mgmt_cfg = 
{ /* default values */
  MTLK_RF_MGMT_TYPE_OFF, /* rf_mgmt_type                  */
  1000,  /* refresh_time_ms                               */
  60,    /* keep_alive_tmout_sec                          */
  40,    /* averaging_alpha (0..ASEL_MAX_AVERAGING_ALPHA] */
  1100,  /* margin_threshold                              */
  NULL
};

