#include "mtlkinc.h"
#include "mtlk_analyzer.h"

#define LOG_LOCAL_GID   GID_ANALYZER
#define LOG_LOCAL_FID   1

#define _MTLK_PEER_SHORT_TERM_RSSI_PACKETS              (8)
#define _MTLK_PEER_LONG_TERM_RSSI_MS                    (30 * MTLK_OSAL_MSEC_IN_SEC)
#define _MTLK_PEER_SHORT_THROUGHPUT_AVERAGE_SEC         (1)
#define _MTLK_PEER_LONG_THROUGHPUT_AVERAGE_SEC          (30)
#define _MTLK_PEER_RETRANSMITTED_NUMBER_SHORT_PACKETS   (100)

static __INLINE void
_mtlk_time_averager_process_sample(_mtlk_time_averager_t* averager, uint32 sample_value)
{
  averager->summator += sample_value;
  averager->samples_counter++;
  if(mtlk_osal_time_passed(averager->counting_start_timestamp) > averager->averaging_time)
  {
    averager->last_value = averager->summator /
                           averager->samples_counter;

    averager->summator = 0;
    averager->samples_counter = 0;
    averager->counting_start_timestamp = mtlk_osal_timestamp();
  }
}

static __INLINE uint32
_mtlk_time_averager_get_value(const _mtlk_time_averager_t* averager)
{
  return averager->last_value;
}

static void
_mtlk_time_averager_init(_mtlk_time_averager_t* averager, uint32 averaging_time_ms)
{
  averager->averaging_time = mtlk_osal_ms_to_timestamp(averaging_time_ms);
  averager->counting_start_timestamp = mtlk_osal_timestamp();
  averager->last_value = 0;
  averager->samples_counter = 0;
  averager->summator = 0;
}

static __INLINE void
_mtlk_count_aggregator_process_sample(_mtlk_count_aggregator_t* aggregator, uint32 sample_value)
{
  aggregator->summator += sample_value;
  if(0 == (++aggregator->samples_counter) % aggregator->samples_to_aggregate)
  {
    aggregator->last_sum = aggregator->summator;
    aggregator->summator = 0;
  }
}

static __INLINE uint32
_mtlk_count_aggregator_get_value(const _mtlk_count_aggregator_t* aggregator)
{
  return aggregator->last_sum;
}

static void
_mtlk_count_aggregator_init(_mtlk_count_aggregator_t* aggregator, uint32 aggregating_samples_num)
{
  aggregator->summator = 0;
  aggregator->last_sum = 0;
  aggregator->samples_counter = 0;
  aggregator->samples_to_aggregate = aggregating_samples_num;
}

static __INLINE void
_mtlk_count_averager_process_sample(_mtlk_count_averager_t* averager, uint32 sample_value)
{
  return _mtlk_count_aggregator_process_sample(&averager->aggregator, sample_value);
}

static __INLINE uint32
_mtlk_count_averager_get_value(const _mtlk_count_averager_t* averager)
{
  return _mtlk_count_aggregator_get_value(&averager->aggregator) / averager->samples_to_average;
}

static void
_mtlk_count_averager_init(_mtlk_count_averager_t* averager, uint32 averaging_samples_num)
{
  MTLK_ASSERT(0 != averaging_samples_num);

  _mtlk_count_aggregator_init(&averager->aggregator, averaging_samples_num);
  averager->samples_to_average = averaging_samples_num;
}

static __INLINE void
_mtlk_per_second_averager_process_sample(_mtlk_per_second_averager_t* averager, uint32 sample_value)
{
  averager->summator += sample_value;
  if(mtlk_osal_time_passed(averager->counting_start_timestamp) > averager->averaging_time)
  {
    averager->last_sum = averager->summator;

    averager->summator = 0;
    averager->counting_start_timestamp = mtlk_osal_timestamp();
  }
}

static __INLINE uint32
_mtlk_per_second_averager_get_value(const _mtlk_per_second_averager_t* averager)
{
  return averager->last_sum / averager->averaging_time_sec;
}

static void
_mtlk_per_second_averager_init(_mtlk_per_second_averager_t* averager, uint32 averaging_time_sec)
{
  averager->averaging_time = mtlk_osal_ms_to_timestamp(averaging_time_sec * MTLK_OSAL_MSEC_IN_SEC);
  averager->averaging_time_sec = averaging_time_sec;
  averager->counting_start_timestamp = mtlk_osal_timestamp();
  averager->last_sum = 0;
  averager->summator = 0;
}

int __MTLK_IFUNC
mtlk_peer_analyzer_init(mtlk_peer_analyzer_t *peer_analyzer)
{
  _mtlk_time_averager_init(&peer_analyzer->long_rssi_average, _MTLK_PEER_LONG_TERM_RSSI_MS);
  _mtlk_count_averager_init(&peer_analyzer->short_rssi_average, _MTLK_PEER_SHORT_TERM_RSSI_PACKETS);
  _mtlk_per_second_averager_init(&peer_analyzer->short_tx_throughput_average, _MTLK_PEER_SHORT_THROUGHPUT_AVERAGE_SEC);
  _mtlk_per_second_averager_init(&peer_analyzer->long_tx_throughput_average, _MTLK_PEER_LONG_THROUGHPUT_AVERAGE_SEC);
  _mtlk_per_second_averager_init(&peer_analyzer->short_rx_throughput_average, _MTLK_PEER_SHORT_THROUGHPUT_AVERAGE_SEC);
  _mtlk_per_second_averager_init(&peer_analyzer->long_rx_throughput_average, _MTLK_PEER_LONG_THROUGHPUT_AVERAGE_SEC);
  _mtlk_count_aggregator_init(&peer_analyzer->retransmitted_number_short, _MTLK_PEER_RETRANSMITTED_NUMBER_SHORT_PACKETS);

  return mtlk_osal_lock_init(&peer_analyzer->analyzer_lock);
}

void __MTLK_IFUNC
mtlk_peer_analyzer_cleanup(mtlk_peer_analyzer_t *peer_analyzer)
{
  mtlk_osal_lock_cleanup(&peer_analyzer->analyzer_lock);
}

void __MTLK_IFUNC
mtlk_peer_analyzer_process_rssi_sample(mtlk_peer_analyzer_t *peer_analyzer, uint8 rssi_sample)
{
  mtlk_osal_lock_acquire(&peer_analyzer->analyzer_lock);

  _mtlk_count_averager_process_sample(&peer_analyzer->short_rssi_average, rssi_sample);
  _mtlk_time_averager_process_sample(&peer_analyzer->long_rssi_average, rssi_sample);

  mtlk_osal_lock_release(&peer_analyzer->analyzer_lock);
}

uint32 __MTLK_IFUNC
mtlk_peer_analyzer_get_long_term_rssi(const mtlk_peer_analyzer_t *peer_analyzer)
{
  uint32 res;

  mtlk_osal_lock_acquire((mtlk_osal_spinlock_t*) &peer_analyzer->analyzer_lock);
  res = _mtlk_time_averager_get_value(&peer_analyzer->long_rssi_average);
  mtlk_osal_lock_release((mtlk_osal_spinlock_t*) &peer_analyzer->analyzer_lock);
  return res;
}

uint32 __MTLK_IFUNC
mtlk_peer_analyzer_get_short_term_rssi(const mtlk_peer_analyzer_t *peer_analyzer)
{
  uint32 res;

  mtlk_osal_lock_acquire((mtlk_osal_spinlock_t*) &peer_analyzer->analyzer_lock);
  res = _mtlk_count_averager_get_value(&peer_analyzer->short_rssi_average);
  mtlk_osal_lock_release((mtlk_osal_spinlock_t*) &peer_analyzer->analyzer_lock);
  return res;
}

uint32 __MTLK_IFUNC
mtlk_peer_analyzer_get_short_term_rx(const mtlk_peer_analyzer_t *peer_analyzer)
{
  uint32 res;

  mtlk_osal_lock_acquire((mtlk_osal_spinlock_t*) &peer_analyzer->analyzer_lock);
  res = _mtlk_per_second_averager_get_value(&peer_analyzer->short_rx_throughput_average);
  mtlk_osal_lock_release((mtlk_osal_spinlock_t*) &peer_analyzer->analyzer_lock);
  return res;
}

uint32 __MTLK_IFUNC
mtlk_peer_analyzer_get_short_term_tx(const mtlk_peer_analyzer_t *peer_analyzer)
{
  uint32 res;

  mtlk_osal_lock_acquire((mtlk_osal_spinlock_t*) &peer_analyzer->analyzer_lock);
  res = _mtlk_per_second_averager_get_value(&peer_analyzer->short_tx_throughput_average);
  mtlk_osal_lock_release((mtlk_osal_spinlock_t*) &peer_analyzer->analyzer_lock);
  return res;
}

uint32 __MTLK_IFUNC
mtlk_peer_analyzer_get_long_term_rx(const mtlk_peer_analyzer_t *peer_analyzer)
{
  uint32 res;

  mtlk_osal_lock_acquire((mtlk_osal_spinlock_t*) &peer_analyzer->analyzer_lock);
  res = _mtlk_per_second_averager_get_value(&peer_analyzer->long_rx_throughput_average);
  mtlk_osal_lock_release((mtlk_osal_spinlock_t*) &peer_analyzer->analyzer_lock);
  return res;
}

uint32 __MTLK_IFUNC
mtlk_peer_analyzer_get_long_term_tx(const mtlk_peer_analyzer_t *peer_analyzer)
{
  uint32 res;

  mtlk_osal_lock_acquire((mtlk_osal_spinlock_t*) &peer_analyzer->analyzer_lock);
  res = _mtlk_per_second_averager_get_value(&peer_analyzer->long_tx_throughput_average);
  mtlk_osal_lock_release((mtlk_osal_spinlock_t*) &peer_analyzer->analyzer_lock);
  return res;
}

uint32 __MTLK_IFUNC
mtlk_peer_analyzer_get_retransmissions_number_short(const mtlk_peer_analyzer_t *peer_analyzer)
{
  uint32 res;

  mtlk_osal_lock_acquire((mtlk_osal_spinlock_t*) &peer_analyzer->analyzer_lock);
  res = _mtlk_count_aggregator_get_value(&peer_analyzer->retransmitted_number_short);
  mtlk_osal_lock_release((mtlk_osal_spinlock_t*) &peer_analyzer->analyzer_lock);
  return res;
}

void __MTLK_IFUNC
mtlk_peer_analyzer_process_rx_packet(mtlk_peer_analyzer_t *peer_analyzer, uint32 data_size)
{
  mtlk_osal_lock_acquire((mtlk_osal_spinlock_t*) &peer_analyzer->analyzer_lock);

  _mtlk_per_second_averager_process_sample(&peer_analyzer->short_rx_throughput_average, data_size);
  _mtlk_per_second_averager_process_sample(&peer_analyzer->long_rx_throughput_average, data_size);

  mtlk_osal_lock_release((mtlk_osal_spinlock_t*) &peer_analyzer->analyzer_lock);
}

void __MTLK_IFUNC
mtlk_peer_analyzer_process_tx_packet(mtlk_peer_analyzer_t *peer_analyzer, uint32 data_size, uint32 retransmissions)
{
  mtlk_osal_lock_acquire((mtlk_osal_spinlock_t*) &peer_analyzer->analyzer_lock);

  _mtlk_per_second_averager_process_sample(&peer_analyzer->short_tx_throughput_average, data_size);
  _mtlk_per_second_averager_process_sample(&peer_analyzer->long_tx_throughput_average, data_size);

  _mtlk_count_aggregator_process_sample(&peer_analyzer->retransmitted_number_short, !!retransmissions);

  mtlk_osal_lock_release((mtlk_osal_spinlock_t*) &peer_analyzer->analyzer_lock);
}

