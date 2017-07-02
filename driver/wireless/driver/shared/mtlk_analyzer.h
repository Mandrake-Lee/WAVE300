#ifndef  __MTLK_ANALYZER__
#define  __MTLK_ANALYZER__

typedef struct __mtlk_time_averager_t
{
  uint32 averaging_time;
  uint32 last_value;
  uint32 counting_start_timestamp;
  uint32 samples_counter;
  uint32 summator;
} _mtlk_time_averager_t;

typedef struct __mtlk_count_aggregator_t
{
  uint32 samples_counter;
  uint32 summator;
  uint32 last_sum;
  uint32 samples_to_aggregate;
} _mtlk_count_aggregator_t;

typedef struct __mtlk_count_averager_t
{
  _mtlk_count_aggregator_t aggregator;
  uint32 samples_to_average;
} _mtlk_count_averager_t;

typedef struct __mtlk_per_second_averager_t
{
  uint32 averaging_time;
  uint32 averaging_time_sec;
  uint32 last_sum;
  uint32 counting_start_timestamp;
  uint32 summator;
} _mtlk_per_second_averager_t;

typedef struct _mtlk_peer_analyzer_t
{
  mtlk_osal_spinlock_t analyzer_lock;

  _mtlk_count_averager_t      short_rssi_average;
  _mtlk_time_averager_t       long_rssi_average;

  _mtlk_per_second_averager_t short_tx_throughput_average;
  _mtlk_per_second_averager_t long_tx_throughput_average;
  _mtlk_per_second_averager_t short_rx_throughput_average;
  _mtlk_per_second_averager_t long_rx_throughput_average;

  _mtlk_count_aggregator_t    retransmitted_number_short;
} mtlk_peer_analyzer_t;

int __MTLK_IFUNC
mtlk_peer_analyzer_init(mtlk_peer_analyzer_t *peer_analyzer);

void __MTLK_IFUNC
mtlk_peer_analyzer_cleanup(mtlk_peer_analyzer_t *peer_analyzer);

uint32 __MTLK_IFUNC
mtlk_peer_analyzer_get_long_term_rssi(const mtlk_peer_analyzer_t *peer_analyzer);

uint32 __MTLK_IFUNC
mtlk_peer_analyzer_get_short_term_rssi(const mtlk_peer_analyzer_t *peer_analyzer);

void __MTLK_IFUNC
mtlk_peer_analyzer_process_rssi_sample(mtlk_peer_analyzer_t *peer_analyzer, uint8 rssi_sample);

uint32 __MTLK_IFUNC
mtlk_peer_analyzer_get_short_term_rx(const mtlk_peer_analyzer_t *peer_analyzer);

uint32 __MTLK_IFUNC
mtlk_peer_analyzer_get_short_term_tx(const mtlk_peer_analyzer_t *peer_analyzer);

uint32 __MTLK_IFUNC
mtlk_peer_analyzer_get_long_term_rx(const mtlk_peer_analyzer_t *peer_analyzer);

uint32 __MTLK_IFUNC
mtlk_peer_analyzer_get_long_term_tx(const mtlk_peer_analyzer_t *peer_analyzer);

uint32 __MTLK_IFUNC
mtlk_peer_analyzer_get_retransmissions_number_short(const mtlk_peer_analyzer_t *peer_analyzer);

void __MTLK_IFUNC
mtlk_peer_analyzer_process_rx_packet(mtlk_peer_analyzer_t *peer_analyzer, uint32 data_size);

void __MTLK_IFUNC
mtlk_peer_analyzer_process_tx_packet(mtlk_peer_analyzer_t *peer_analyzer, uint32 data_size, uint32 retransmissions);


#endif /* __MTLK_ANALYZER__ */
