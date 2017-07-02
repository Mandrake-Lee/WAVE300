#include "mtlkinc.h"
#include "mtlkmetrics.h"

#define LOG_LOCAL_GID   GID_MTLKMETRICS
#define LOG_LOCAL_FID   1

#define NUMBER_OF_BETA_VALUES 4
#define NUMBER_OF_RANKS       2
#define SCALE_FACTOR_BITS     2
#define SCALE_FACTOR_MASK     0x3
#define LN2_INV               0x5c6
#define RESULT_FLOOR_BIT_NUM  11			//how many bits to discard from the final metric

//These effective snr calibration was updated at 8/7/09 - the values are optimized for channel C_NLOS
static const uint32 s_beta_inverse_value[NUMBER_OF_RANKS][NUMBER_OF_BETA_VALUES] = {
  { 0x000001ff, 0x000000ab, 0x00000044, 0x0000000f },//beta1=1,3,7.5,35, 
  { 0x000001ff, 0x000000cd, 0x00000033, 0x0000000b } //beta2=1,2.5,10,45
};

static const uint32 s_beta_div_log2e[NUMBER_OF_RANKS][NUMBER_OF_BETA_VALUES] = {
  {  0x0000002c,  0x00000084, 0x0000014c, 0x00000610 },
  {  0x0000002c,  0x0000006e, 0x000001ba, 0x000007cc }
};

static const uint32 s_beta_thresholds[NUMBER_OF_RANKS][NUMBER_OF_BETA_VALUES*2] = {
  { 0x00920, 0x0065b, 0x0092f, 0x00780, 0x00c2a, 0x00785, 0x009b0, 0x00beb },
  { 0x006c4, 0x00566, 0x0082c, 0x0057f, 0x008b7, 0x00644, 0x007b5, 0x009b4 }
};

static const uint8 mcs_to_beta_index[NUMBER_OF_BETA_VALUES*2]= {0, 1, 1, 2, 2, 3, 3, 3};

/*****************************************************************************
** DESCRIPTION: Returns the number of bins according to channel bonding and
** MCS used			
******************************************************************************/
static __INLINE uint32
get_number_of_bins_for_packet (uint8 i_is_cb)
{
	if (i_is_cb==1)
		return 108;
	else
		return 52;
}

/*****************************************************************************
** DESCRIPTION: Returns value of a bit field in a 32 bit value
******************************************************************************/
static __INLINE uint32
trim (uint32 i_value, uint32 i_bits_start, uint32 i_bits_end )
{
    uint32 bit_index;
    uint32 mask = 0;

    for( bit_index = 0; bit_index< (i_bits_end - i_bits_start + 1) ; bit_index ++)
    {
        mask |= 1<<(bit_index + i_bits_start);
    }

    return ( (i_value & mask)>>i_bits_start );
}
/*****************************************************************************
** DESCRIPTION: Divides a number by 4
*****************************************************************************/
static __INLINE uint32
divide4 ( uint32 i_x )
{
    return (i_x>>2);
}
/*****************************************************************************
** DESCRIPTION
 Returns the modulo 4 of a number
*****************************************************************************/
static __INLINE uint32
modulo4 (uint32 i_x)
{
    return i_x -( ( i_x>>2 )<<2 );
}

/*****************************************************************************
** DESCRIPTION: Returns the scaling factor for a bin number
*****************************************************************************/
static __INLINE uint32
extract_scaling_factor ( uint32 i_bin_index, uint32 * i_scale_array )
{
    uint32 scale_value;
    uint32 scaling_factor;

    scale_value = i_scale_array[(i_bin_index/4)];
    
    scaling_factor = ( scale_value >> (SCALE_FACTOR_BITS*(i_bin_index%4)) & SCALE_FACTOR_MASK);
/*
    scale_value = i_scale_array[ divide4( i_bin_index ) ];

    scaling_factor = ( scale_value >>
                      ( SCALE_FACTOR_BITS * modulo4( i_bin_index ) ) ) &
                      SCALE_FACTOR_MASK;
*/
    return scaling_factor;
}

/*****************************************************************************
** DESCRIPTION: Slices bits off a number
*****************************************************************************/
static __INLINE uint32
slicebits (uint32 x, int start, int count)
{
    return (x>>start)&((1<<count)-1);
}

/*****************************************************************************
** DESCRIPTION: Locates MSB of a number
*****************************************************************************/
static __INLINE int
locate_msbit (int i_x)
{
    int pos = -1;
    int x = i_x;
    while( ( pos<=32 ) && ( x>0 ) )
    {
        pos = pos+1;
        x>>=1;
    }
    return pos;
}


/*****************************************************************************
** DESCRIPTION: Fixed point arithmetic function to calculate log2 of a number
*****************************************************************************/

static __INLINE int
get_precalculated_log_2_n (uint32 i_x)
{
    switch( i_x )
    {
        case 108:
            return 0x1982;
		case 52:
			return 0x1766;
        default:
            return 0;
    }
}



static __INLINE int
calculate_log_2_n (uint32 i_x)
{
    int int_part,frac_part;

    if ( 0==i_x) return 0;
    int_part = locate_msbit(i_x);
    frac_part = slicebits(i_x, int_part-4, 4);
    return (int_part << 4) | frac_part;
}


/*****************************************************************************
** DESCRIPTION: Rounding of a fixed point number to nearest whole number
*****************************************************************************/
static __INLINE uint32
bias_round ( uint32 i_x, uint32 i_bits_after_decimal_point)
{
    return i_x+(1<<(i_bits_after_decimal_point-1));
}


static __INLINE uint32
negate (uint32 i_number,uint32 i_bits)
{
    uint32 full_value = 1<<i_bits;
    return full_value - i_number;
}

/*****************************************************************************
**  Metric calculation
**  The following functions were taken from Antenna Selection Spec. It is a
**  translation to C of Gen 3 antenna selection VLSI operations.
*****************************************************************************/
static __INLINE uint32
calculate_effective_const( uint32 in_inverse_noise,
                           uint32 in_inverse_beta )
{
    uint32 effective_const = trim( in_inverse_noise, 0, 15 ) *
                             trim( in_inverse_beta, 0, 8 );

    return trim( effective_const, 5, 18 );
}



static __INLINE uint32
calculate_exponent_coefficient (uint32 i_effective_const, 
                                uint32 i_r,
                                uint32 i_scaling_factor)
{
    uint32 r_int_part, r_sqr;
    uint32 tmp,tmp1,tmp2,tmp3;

    r_int_part = trim( bias_round( i_r, 6 ), 6, 10 );
    r_sqr = r_int_part * r_int_part;

    tmp = trim( r_sqr, 0, 9 ) * trim( i_effective_const, 0, 13 );
    tmp1 = trim( bias_round( tmp, 5 ), 5, 22 );//Originaly 6, need to ask Avi

    tmp2 = trim( tmp1, 0, 16 )<<6;

    tmp3 = tmp2>>( 2 * i_scaling_factor );

    return trim( bias_round( tmp3, 2 ), 2, 16 );
}


static __INLINE uint32
calculate_exponent ( uint32 i_exponent_coefficient )
{
    uint32 tmp4, tmp6, tmp8, x_div_ln2;
    uint32 tmp5, K_tmp, K;
    uint32 tmp5_abs;

    tmp4 = i_exponent_coefficient * LN2_INV;

    x_div_ln2 =  trim( bias_round ( tmp4, 10 ), 10, 25 );

    tmp5 = negate( x_div_ln2, 17 );

    K_tmp = trim( (uint32)tmp5, 6, 16 );

    K = negate( K_tmp, 11 );

    tmp6 = ( ( trim((uint32)K, 0, 10 ) + 1 ) << 6 );

    tmp5_abs = negate(tmp5,17);
    tmp6 = tmp6 - tmp5_abs;
    tmp8 = trim( tmp6, 0, 6 ) << 5;

    if ( K > 12 )
    {
        K=12;
    }

    return( tmp8 >> K );
}

static __INLINE uint32
calculate_result (uint32 i_log_2_n, uint32 i_sum, uint32 i_rank )
{
    uint32 sum;
    int mean_exp_result;
    int sum_log2;
    uint32 a;

    sum = trim( i_sum, 5, 19 );
    sum_log2 = calculate_log_2_n( sum );

    a = trim( sum_log2, 0, 7 ) << 5;

    mean_exp_result = trim(i_log_2_n,0,12) + ( ( i_rank - 1 ) << 9 ) - a;
    mean_exp_result = trim(mean_exp_result,0,12);

    return mean_exp_result;
}


static __INLINE uint32
get_next_beta_index (uint32 i_effective_snr, uint32 i_beta_index, uint32 i_rank, uint8 * mcs_feedback)
{
    uint32 return_value = 0;

    switch( i_beta_index ) 
    {
        case 3: if( i_effective_snr >= s_beta_thresholds[i_rank-1][7] )
                {
                        *mcs_feedback = 7;
                }
                else if( i_effective_snr >= s_beta_thresholds[i_rank-1][6] ) 
                {
                        *mcs_feedback = 6;
                }
                else if( i_effective_snr >= s_beta_thresholds[i_rank-1][5] ) 
                {
                        *mcs_feedback = 5;
                }
                else
                {
                        *mcs_feedback = 4;
                }
                return_value = 3;
                break;

        case 2: if( i_effective_snr >= s_beta_thresholds[i_rank-1][4] )                     
                {
                        return_value = 3;
                }
                else if( i_effective_snr >= s_beta_thresholds[i_rank-1][3] ) 
                {
                        *mcs_feedback = 3;
                        return_value  = 2;
                }
                else
                {
                    return_value = 1;
                }
                break;

        case 1: if ( i_effective_snr >= s_beta_thresholds[i_rank-1][2] ) 
                {
                        *mcs_feedback = 2;
                        return_value  = 1;
                }
                else if ( i_effective_snr >= s_beta_thresholds[i_rank-1][1] )
                {
                        *mcs_feedback = 1;
                        return_value  = 1;
                }
                else
                {
                    return_value = 0;
                }
                break;

        case 0: *mcs_feedback = 0;
                return_value  = 0;
                break;

        default:    MTLK_ASSERT( 0 );


    }
    *mcs_feedback += i_rank==2 ? 8 : 0;
    return return_value;
}       

static __INLINE uint32
calculate_short_cp_metric(mtlk_g2_rx_metrics_t *metrics)
{
	uint32 bin_index;
	uint32 number_of_bins;
	uint32 scaling_factor;
	uint32 r_inp, r_inp_old;
	uint32 sum_filter,sum_r_sqr;
	uint32 tmp;


	number_of_bins = get_number_of_bins_for_packet(metrics->sMtlkHeader.isCB)-1;
	scaling_factor = extract_scaling_factor( 0, metrics->au32scaleFactor );
	r_inp_old = metrics->au32metric[0][0] << (3-scaling_factor);
	sum_filter=0;
	sum_r_sqr=0;
	for( bin_index =  1 ; bin_index<number_of_bins ; bin_index++) 
    {
		scaling_factor = extract_scaling_factor( bin_index,
                                                 metrics->au32scaleFactor );
		r_inp = metrics->au32metric[0][bin_index] << (3-scaling_factor);
		tmp = r_inp - r_inp_old;
		sum_filter = sum_filter + tmp*tmp;
		sum_r_sqr = sum_r_sqr + r_inp*r_inp;
		r_inp_old = r_inp;
	}
	sum_r_sqr = sum_r_sqr >> 10;	
	return  sum_filter / sum_r_sqr;

}

uint32 __MTLK_IFUNC
mtlk_metrics_calc_effective_snr (mtlk_g2_rx_metrics_t *metrics,
                                 uint8                *mcs_feedback,
                                 uint32               *short_cp_metric)
{
    uint32 beta_index;
    uint32 number_of_bins;
    uint32 log_2_n_subcarrier;
    uint32 return_value = 0;
    uint32 return_value1 = 0;
    uint32 next_beta;
	
    MTLK_ASSERT(metrics->sMtlkHeader.rank == 1 || metrics->sMtlkHeader.rank == 2);

    number_of_bins = get_number_of_bins_for_packet(metrics->sMtlkHeader.isCB);

    log_2_n_subcarrier = get_precalculated_log_2_n(number_of_bins);

    beta_index = 2;

    while (1) 
    {
        uint32 effective_const;
        uint32 bin_index;
        uint32 beta_inverse_value = s_beta_inverse_value[metrics->sMtlkHeader.rank-1][beta_index];
        uint32 sum_of_exponents = 0;
        uint32 effective_snr_for_beta = 0;
		
		

        effective_const = calculate_effective_const( metrics->u32NoiseFunction,
                                                     beta_inverse_value );

        for( bin_index =  0 ; bin_index< number_of_bins; bin_index++) 
        {
            uint32 rank_index;
      
            uint32 scaling_factor = extract_scaling_factor( bin_index,
                                                            metrics->au32scaleFactor );

            for( rank_index=0; rank_index < metrics->sMtlkHeader.rank; rank_index++ ) 
            {
                uint32 exponent_result;
                uint32 exponent_coefficient = calculate_exponent_coefficient(
                                                                             effective_const,
                                                                             metrics->au32metric[rank_index][bin_index],
                                                                             scaling_factor );

                exponent_result = calculate_exponent( exponent_coefficient );
                sum_of_exponents += exponent_result;
                sum_of_exponents = trim( sum_of_exponents, 0, 19 );
            }
	    }

        effective_snr_for_beta = calculate_result( log_2_n_subcarrier,
                                                   sum_of_exponents,
                                                   metrics->sMtlkHeader.rank );

        next_beta = get_next_beta_index(effective_snr_for_beta, beta_index, metrics->sMtlkHeader.rank, mcs_feedback);
        
        if( beta_index == next_beta ) 
        {
            return_value1 = effective_snr_for_beta * s_beta_div_log2e[metrics->sMtlkHeader.rank-1][beta_index];
			return_value = trim(return_value1, RESULT_FLOOR_BIT_NUM, 25); //(10,14)2^-4
			//*margin = (1000*effective_snr_for_beta)/s_beta_thresholds[metrics->sHeader.rank-1][*mcs_feedback -(metrics->sHeader.rank-1)*8];
            break;
        }
        else 
        {
          beta_index = next_beta;
        }
    }
	if (metrics->sMtlkHeader.rank==2)
		*short_cp_metric = calculate_short_cp_metric( metrics );	
	else
		*short_cp_metric = 0;

    return return_value;
}

uint32 __MTLK_IFUNC
mtlk_calculate_effective_snr_margin(uint32 metric, uint8 mcs_feedback)
{
	uint8 beta_index;
	uint32 effective_snr_for_beta;
	uint32 margin;
	uint8 rank=1;
	if (mcs_feedback > 7)
	{
		rank=2;
		mcs_feedback = mcs_feedback -8;
	}

	beta_index = mcs_to_beta_index[mcs_feedback];
	
	effective_snr_for_beta=(metric << RESULT_FLOOR_BIT_NUM) / s_beta_div_log2e[rank-1][beta_index];
	margin = (1000*effective_snr_for_beta)/s_beta_thresholds[rank-1][mcs_feedback];

	return margin;
}

#ifndef __KERNEL__

#include <math.h>

uint32 __MTLK_IFUNC
mtlk_get_esnr_db (uint32 metric, uint8 mcs_feedback)
{
  uint32 res;
  uint8  beta_index;
  uint8  rank=1;
  double log_arg;
  
  if (mcs_feedback > 7){
    rank=2;
    mcs_feedback = mcs_feedback -8;
  }

  beta_index = mcs_to_beta_index[mcs_feedback];
  log_arg    = (double)s_beta_div_log2e[rank-1][beta_index] * metric;

  res = (uint32)((log10(log_arg)/* - 4.5 - TODO: restore in final code */) * 10 * ESNR_DB_RESOLUTION);

  ILOG2_DDDDDDDD("esnr=%u mcsf=%u rank=%u idx=%u beta=0x%08x arg=%d log=%d res=%u",
       metric, mcs_feedback, rank, beta_index, s_beta_div_log2e[rank-1][beta_index], (uint32)log_arg, (uint32)log10(log_arg), res);

  return res;
}

#endif
