/*
===============================================================================
 Name        : main.c
 Author      : $(author)
 Version     :
 Copyright   : $(copyright)
 Description : main definition
===============================================================================
*/

#ifdef __USE_CMSIS
#include "LPC43xx.h"
#endif

#include <lpc43xx.h>
#include <lpc43xx_gpio.h>
#include <lpc43xx_cgu.h>
#include <lpc43xx_gpdma.h>
#include <lpc43xx_scu.h>
#include <lpc43xx_rgu.h>
#include "lpc43xx_i2s.h"
#include "lpc43xx_i2c.h"

#include <cr_section_macros.h>

#include <stdio.h>
#include <limits.h>
#include <arm_math.h>

#include "hsadctest.h"
#include "meas.h"


// TODO: insert other include files here

#define VADC_DMA_WRITE  7
#define VADC_DMA_READ   8
#define VADC_DMA_READ_SRC  (LPC_VADC_BASE + 512)  /* VADC FIFO */
#define FIFO_SIZE       8

#define DMA_NUM_LLI_TO_USE    16
static GPDMA_LLI_Type DMA_Stuff[DMA_NUM_LLI_TO_USE];

// PLL0AUDIO: 40MHz = (12MHz / 15) * (400 * 2) / (8 * 2)
#define PLL0_MSEL	400
#define PLL0_NSEL	15
#define PLL0_PSEL	8
//#define ADCCLK_MATCHVALUE	(2 - 1)  // 40MHz / 2 = 20MHz
#define ADCCLK_MATCHVALUE	(4 - 1)  // 40MHz / 4 = 10MHz
//#define ADCCLK_MATCHVALUE	(8 - 1)  // 40MHz / 8 = 5MHz
//#define ADCCLK_MATCHVALUE	(16 - 1)  // 40MHz / 16 = 2.5MHz
#define ADCCLK_DGECI 0

#define SETTINGS_GPIO_IN    (PUP_DISABLE | PDN_DISABLE | SLEWRATE_SLOW | INBUF_ENABLE  | FILTER_ENABLE)
#define SETTINGS_GPIO_OUT   (PUP_DISABLE | PDN_DISABLE | SLEWRATE_SLOW |                 FILTER_ENABLE)
#define SETTINGS_SGPIO      (PDN_DISABLE | PUP_DISABLE |                 INBUF_ENABLE                 )
#define SETTINGS_SPIFI      (PUP_DISABLE | PDN_DISABLE | SLEWRATE_SLOW | INBUF_ENABLE  | FILTER_ENABLE)
#define SETTINGS_SSP        (PUP_DISABLE | PDN_DISABLE | SLEWRATE_SLOW | INBUF_ENABLE  | FILTER_ENABLE)

// ------------------------------------------------------------------------------------------------
// -----                                         VADC                                         -----
// ------------------------------------------------------------------------------------------------
/**
  * @brief Product name title=UM????? Chapter title=?????? Modification date=12/11/2012 Major revision=? Minor revision=?  (VADC)
    0x400F0000
  */
typedef struct {                            /*!< (@ 0x400F0000) VADC Structure         */
  __O  uint32_t FLUSH;                      /*!< (@ 0x400F0000) Flushes FIFO */
  __IO uint32_t DMA_REQ;                    /*!< (@ 0x400F0004) Set or clear DMA write request */
  __I  uint32_t FIFO_STS;                   /*!< (@ 0x400F0008) Indicates FIFO fullness status */
  __IO uint32_t FIFO_CFG;                   /*!< (@ 0x400F000C) Configures FIFO fullness level that triggers interrupt and packing 1 or 2 samples per word. */
  __O  uint32_t TRIGGER;                    /*!< (@ 0x400F0010) Enable software trigger to start descriptor processing */
  __IO uint32_t DSCR_STS;                   /*!< (@ 0x400F0014) Indicates active descriptor table and descriptor entry */
  __IO uint32_t POWER_DOWN;                 /*!< (@ 0x400F0018) Set or clear power down mode */
  __IO uint32_t CONFIG;                     /*!< (@ 0x400F001C) Configures external trigger mode, store channel ID in FIFO and wakeup recovery time from power down. */
  __IO uint32_t THR_A;                      /*!< (@ 0x400F0020) Configures window comparator A levels. */
  __IO uint32_t THR_B;                      /*!< (@ 0x400F0024) Configures window comparator B levels. */
  __I  uint32_t LAST_SAMPLE[6];             /*!< (@ 0x400F0028)	Contains last converted sample of input M [M=0..5) and result of window comparator. */
  __I  uint32_t RESERVED0[48];
  __IO uint32_t ADC_DEBUG;                  /*!< (@ 0x400F0100) Reserved  (ADC Debug pin inputs) */
  __IO uint32_t ADC_SPEED;                  /*!< (@ 0x400F0104) ADC speed control */
  __IO uint32_t POWER_CONTROL;              /*!< (@ 0x400F0108) Configures ADC power vs. speed, DC-in biasing, output format and power gating. */
  __I  uint32_t RESERVED1[61];
  __I  uint32_t FIFO_OUTPUT[16];            /*!< (@ 0x400F0200 - 0x400F023C) FIFO output mapped to 16 consecutive address locations. An output contains the value and input channel ID of one or two converted samples  */
  __I  uint32_t RESERVED2[48];
  __IO uint32_t DESCRIPTOR_0[8];            /*!< (@ 0x400F0300) Table0  descriptor n, n= 0 to 7  */
  __IO uint32_t DESCRIPTOR_1[8];            /*!< (@ 0x400F0320) Table1  descriptor n, n= 0 to 7  */
  __I  uint32_t RESERVED3[752];
  __O  uint32_t CLR_EN0;                    /*!< (@ 0x400F0F00) Interrupt0 clear mask */
  __O  uint32_t SET_EN0;                    /*!< (@ 0x400F0F04) Interrupt0 set mask */
  __I  uint32_t MASK0;                      /*!< (@ 0x400F0F08) Interrupt0 mask */
  __I  uint32_t STATUS0;                    /*!< (@ 0x400F0F0C) Interrupt0 status. Interrupt0 contains FIFO fullness, descriptor status and ADC range under/overflow */
  __O  uint32_t CLR_STAT0;                  /*!< (@ 0x400F0F10) Interrupt0 clear status  */
  __O  uint32_t SET_STAT0;                  /*!< (@ 0x400F0F14) Interrupt0 set status  */
  __I  uint32_t RESERVED4[2];
  __O  uint32_t CLR_EN1;                    /*!< (@ 0x400F0F20) Interrupt1 mask clear enable.  */
  __O  uint32_t SET_EN1;                    /*!< (@ 0x400F0F24) Interrupt1 mask set enable  */
  __I  uint32_t MASK1;                      /*!< (@ 0x400F0F28) Interrupt1 mask */
  __I  uint32_t STATUS1;                    /*!< (@ 0x400F0F2C) Interrupt1 status. Interrupt1 contains window comparator results and register last LAST_SAMPLE[M] overrun. */
  __O  uint32_t CLR_STAT1;                  /*!< (@ 0x400F0F30) Interrupt1 clear status  */
  __O  uint32_t SET_STAT1;                  /*!< (@ 0x400F0F34) Interrupt1 set status  */
} LPC_VADC_Type;

#define LPC_VADC_BASE             0x400F0000
#define LPC_VADC                  ((LPC_VADC_Type           *) LPC_VADC_BASE)

#define STATUS0_FIFO_FULL_MASK      (1<<0)
#define STATUS0_FIFO_EMPTY_MASK     (1<<1)
#define STATUS0_FIFO_OVERFLOW_MASK  (1<<2)
#define STATUS0_DESCR_DONE_MASK     (1<<3)
#define STATUS0_DESCR_ERROR_MASK    (1<<4)
#define STATUS0_ADC_OVF_MASK        (1<<5)
#define STATUS0_ADC_UNF_MASK        (1<<6)

#define STATUS0_CLEAR_MASK          0x7f

#define STATUS1_THCMP_BRANGE(__ch)  ((1<<0) << (5 * (__ch)))
#define STATUS1_THCMP_ARANGE(__ch)  ((1<<1) << (5 * (__ch)))
#define STATUS1_THCMP_DCROSS(__ch)  ((1<<2) << (5 * (__ch)))
#define STATUS1_THCMP_UCROSS(__ch)  ((1<<3) << (5 * (__ch)))
#define STATUS1_THCMP_OVERRUN(__ch) ((1<<4) << (5 * (__ch)))

#define STATUS1_CLEAR_MASK          0x1fffffff

#define CGU_BASE_VADC CGU_BASE_ENET_CSR
#define VADC_IRQn RESERVED7_IRQn

int32_t capture_count;


//#define CAPTUREBUFFER		((uint8_t*)0x20000000)
//#define CAPTUREBUFFER		((uint8_t*)0x10080000)
#define CAPTUREBUFFER		((uint8_t*)0x20000000)
#define CAPTUREBUFFER_SIZE	0x10000

#define NCO_SIN_BUFFER		((uint8_t*)0x1008F000)
#define NCO_COS_BUFFER		((uint8_t*)0x1008F800)
#define NCO_BUFFER_SIZE		0x800
#define NCO_SAMPLES			1024
#define NCO_AMPL			(SHRT_MAX / 16)
//#define NCO_AMPL			(SHRT_MAX / 4)

#define DECIMATE			32

#define I_FIR_STATE			((q15_t*)0x10080000)
#define I_FIR_BUFFER		((q15_t*)0x10080040)
#define Q_FIR_STATE			((q15_t*)0x10084040)
#define Q_FIR_BUFFER		((q15_t*)0x10084080)
/*  0x10000 / 2 / 32 */
#define FIR_BUFFER_SIZE		0x400
#define FIR_STATE_SIZE		0x40

//#define FIR_GAIN			(16-8)
#define FIR_GAIN			(16-4)
//#define FIR_GAIN			(16)

#define DEMOD_BUFFER 		((q15_t*)0x10088000)
#define DEMOD_BUFFER_SIZE	0x800
#define RESAMPLE_STATE 		((q15_t*)0x10089000)
#define RESAMPLE_STATE_SIZE	0x100
#define RESAMPLE_BUFFER 	((q15_t*)0x10089100)
#define RESAMPLE_BUFFER_SIZE 0x400
#define AUDIO_BUFFER 		((q15_t*)0x1008A000)
#define AUDIO_BUFFER_SIZE	0x2000
#define AUDIO_BUFFER2 		((q15_t*)0x1008C000)




/*
 * DSP Processing
 */

#define NCO_CYCLE 1024
#define NCO_SAMPLES 1024
#define NCO_COS_OFFSET (NCO_CYCLE/4)

static void ConfigureNCOTable(int freq)
{
	int i;
	int16_t *costbl = (int16_t*)NCO_SIN_BUFFER;
	int16_t *sintbl = (int16_t*)NCO_COS_BUFFER;
	for (i = 0; i < NCO_SAMPLES; i++) {
		float32_t phase = 2*PI*freq*(i+0.5)/NCO_CYCLE;
		costbl[i] = (int16_t)(arm_cos_f32(phase) * NCO_AMPL);
		sintbl[i] = (int16_t)(arm_sin_f32(phase) * NCO_AMPL);
	}
}

#define FIR_NUM_TAPS			32

q15_t fir_coeff[FIR_NUM_TAPS] = {
			-204,   -42,   328,   144,  -687,  -430,  1301,  1060, -2162,
		   -2298,  3208,  4691, -4150, -9707,  3106, 22273, 22273,  3106,
		   -9707, -4150,  4691,  3208, -2298, -2162,  1060,  1301,  -430,
			-687,   144,   328,   -42,  -204
};

typedef struct {
	int32_t s0;
	int32_t s1;
	int32_t s2;
	int32_t d0;
	int32_t d1;
	int32_t d2;
	int32_t dest;
} CICState;

static CICState cic_i;
static CICState cic_q;

static void cic_decimate_i(CICState *cic, uint8_t *buf, int len)
{
	const uint32_t offset = 0x08000800;
	int16_t *const result = (int16_t*)I_FIR_BUFFER;
	uint32_t *capture = (uint32_t*)buf;
	const uint32_t *nco_base = (uint32_t*)NCO_SIN_BUFFER;
	const uint32_t *nco = nco_base;

	int32_t s0 = cic->s0;
	int32_t s1 = cic->s1;
	int32_t s2 = cic->s2;
	int32_t d0 = cic->d0;
	int32_t d1 = cic->d1;
	int32_t d2 = cic->d2;
	int32_t e0, e1, e2;
	uint32_t f;
	uint32_t x;
	int i, j, k, l;

	l = cic->dest;
	for (i = 0; i < len / 4; ) {
		nco = nco_base;
		for (j = 0; j < NCO_SAMPLES/2; ) {
			for (k = 0; k < DECIMATE / 2; k++) {
				x = capture[i++];
				f = *nco++;
				x = __SSUB16(x, offset);
				s0 = __SMLAD(x, f, s0);
				s1 += s0;
				s2 += s1;
				j++;
			}
			e0 = d0 - s2;
			d0 = s2;
			e1 = d1 - e0;
			d1 = e0;
			e2 = d2 - e1;
			d2 = e1;
			result[l++] = e2 >> FIR_GAIN;
			l %=  FIR_BUFFER_SIZE/2;
		}
	}
	cic->dest = l;
	cic->s0 = s0;
	cic->s1 = s1;
	cic->s2 = s2;
	cic->d0 = d0;
	cic->d1 = d1;
	cic->d2 = d2;
}

static void cic_decimate_q(CICState *cic, uint8_t *buf, int len)
{
	const uint32_t offset = 0x08000800;
	int16_t *const result = (int16_t*)Q_FIR_BUFFER;
	uint32_t *capture = (uint32_t*)buf;
	const uint32_t *nco_base = (uint32_t*)NCO_COS_BUFFER;
	const uint32_t *nco = nco_base;

	int32_t s0 = cic->s0;
	int32_t s1 = cic->s1;
	int32_t s2 = cic->s2;
	int32_t d0 = cic->d0;
	int32_t d1 = cic->d1;
	int32_t d2 = cic->d2;
	int32_t e0, e1, e2;
	uint32_t f;
	uint32_t x;
	int i, j, k, l;

	l = cic->dest;
	for (i = 0; i < len / 4; ) {
		nco = nco_base;
		for (j = 0; j < NCO_SAMPLES/2; ) {
			for (k = 0; k < 16; k++) {
				x = capture[i++];
				f = *nco++;
				x = __SSUB16(x, offset);
				s0 = __SMLAD(x, f, s0);
				s1 += s0;
				s2 += s1;
				j++;
			}
			e0 = d0 - s2;
			d0 = s2;
			e1 = d1 - e0;
			d1 = e0;
			e2 = d2 - e1;
			d2 = e1;
			result[l++] = e2 >> FIR_GAIN;
			l %=  FIR_BUFFER_SIZE/2;
		}
	}
	cic->dest = l;
	cic->s0 = s0;
	cic->s1 = s1;
	cic->s2 = s2;
	cic->d0 = d0;
	cic->d1 = d1;
	cic->d2 = d2;
}

#if 0
void fir_filter_i()
{
	const uint32_t *coeff = (uint32_t*)fir_coeff;
	const uint32_t *in_i = (const uint32_t *)I_FIR_STATE;
	int32_t length = FIR_BUFFER_SIZE / sizeof(uint32_t);
	uint32_t *dest = (uint32_t *)DEMOD_BUFFER;
	int i, j;

	for (i = 0; i < length; i++) {
		q31_t acc0_i = 0;
		q31_t acc1_i = 0;
		uint32_t x0 = in_i[0];
		for (j = 0; j < FIR_NUM_TAPS / 2; ) {
			uint32_t c0 = coeff[j];
			uint32_t x2 = in_i[++j];
			acc0_i = __SMLAD(x0, c0, acc0_i);
			uint32_t x1 = __PKHBT(x2, x0, 0);
			acc1_i = __SMLADX(x1, c0, acc1_i);
			x0 = x2;
		}
		dest[i] = __PKHBT(__SSAT((acc0_i >> 15), 16), __SSAT((acc1_i >> 15), 16), 16);
		in_i++;
	}

	uint32_t *state_i = (uint32_t *)I_FIR_STATE;
	for (i = 0; i < FIR_STATE_SIZE / sizeof(uint32_t); i++) {
		//*state_i++ = *in_i++;
	    __asm__ volatile ("ldr r0, [%0], #+4\n" : : "r" (in_i) : "r0");
	    __asm__ volatile ("str r0, [%0], #+4\n" : : "r" (state_i));
	}
}
#endif

void fir_filter_iq()
{
	const uint32_t *coeff = (uint32_t*)fir_coeff;
	const uint32_t *in_i = (const uint32_t *)I_FIR_STATE;
	const uint32_t *in_q = (const uint32_t *)Q_FIR_STATE;
	int32_t length = FIR_BUFFER_SIZE / sizeof(uint32_t);
	uint32_t *dest = (uint32_t *)DEMOD_BUFFER;
	int i, j;

	for (i = 0; i < length; i++) {
		q31_t acc0_i = 0;
		q31_t acc1_i = 0;
		q31_t acc0_q = 0;
		q31_t acc1_q = 0;
		uint32_t x0 = in_i[0];
		uint32_t y0 = in_q[0];
		for (j = 0; j < FIR_NUM_TAPS / 2; ) {
			uint32_t c0 = coeff[j++];
			uint32_t x2 = in_i[j];
			uint32_t y2 = in_q[j];
			acc0_i = __SMLAD(x0, c0, acc0_i);
			acc0_q = __SMLAD(y0, c0, acc0_q);
			acc1_i = __SMLADX(__PKHBT(x2, x0, 0), c0, acc1_i);
			acc1_q = __SMLADX(__PKHBT(y2, y0, 0), c0, acc1_q);
			x0 = x2;
			y0 = y2;
		}
		dest[i*2] = __PKHBT(__SSAT((acc0_i >> 15), 16), __SSAT((acc0_q >> 15), 16), 16);
		dest[i*2+1] = __PKHBT(__SSAT((acc1_i >> 15), 16), __SSAT((acc1_q >> 15), 16), 16);
		in_i++;
		in_q++;
	}

	uint32_t *state_i = (uint32_t *)I_FIR_STATE;
	for (i = 0; i < FIR_STATE_SIZE; i += 4) {
		//*state_i++ = *in_i++;
	    __asm__ volatile ("ldr r0, [%0, %2]\n"
	    				  "str r0, [%1, %2]\n" :: "l"(in_i), "l"(state_i), "X"(i): "r0");
	}
	uint32_t *state_q = (uint32_t *)Q_FIR_STATE;
	for (i = 0; i < FIR_STATE_SIZE; i += 4) {
		//*state_q++ = *in_q++;
	    __asm__ volatile ("ldr r0, [%0, %2]\n"
	    				  "str r0, [%1, %2]\n" :: "l"(in_q), "l"(state_q), "X"(i): "r0");
	}
}

struct {
	uint32_t last;
} fm_demod_state;

void fm_demod()
{
	uint32_t *src = (uint32_t *)DEMOD_BUFFER;
	int16_t *dest = (int16_t *)RESAMPLE_BUFFER;
	int32_t length = DEMOD_BUFFER_SIZE / sizeof(uint32_t);
	int i;

	uint32_t x0 = fm_demod_state.last;
	for (i = 0; i < length; i++) {
		uint32_t x1 = src[i];
		int32_t d = __SMUSDX(__SSUB16(x1, x0), x1);
		int32_t n = __SMUAD(x1, x1) >> 12;
		int16_t y = d / n;
		dest[i] = y;
		x0 = x1;
	}
	fm_demod_state.last = x0;
}

#define RESAMPLE_NUM_TAPS	128

q15_t resample_fir_coeff_even[RESAMPLE_NUM_TAPS] = {
		  0,   0,   0,  -1,  -2,  -3,  -4,  -5,  -6,  -8, -10, -12, -14,
		       -16, -18, -21, -23, -26, -28, -30, -32, -33, -35, -35, -35, -34,
		       -33, -31, -27, -23, -17, -10,  -2,   7,  18,  30,  44,  59,  75,
		        93, 112, 132, 153, 175, 197, 220, 244, 268, 291, 315, 338, 361,
		       383, 404, 424, 442, 459, 474, 487, 498, 508, 514, 519, 521, 521,
		       519, 514, 508, 498, 487, 474, 459, 442, 424, 404, 383, 361, 338,
		       315, 291, 268, 244, 220, 197, 175, 153, 132, 112,  93,  75,  59,
		        44,  30,  18,   7,  -2, -10, -17, -23, -27, -31, -33, -34, -35,
		       -35, -35, -33, -32, -30, -28, -26, -23, -21, -18, -16, -14, -12,
		       -10,  -8,  -6,  -5,  -4,  -3,  -2,  -1,   0,   0,   0};
q15_t resample_fir_coeff_odd[RESAMPLE_NUM_TAPS] = {
		  0,   0,  -1,  -1,  -2,  -3,  -4,  -6,  -7,  -9, -11, -13, -15,
		       -17, -20, -22, -24, -27, -29, -31, -33, -34, -35, -35, -35, -34,
		       -32, -29, -25, -20, -14,  -6,   2,  12,  24,  37,  51,  67,  84,
		       102, 122, 142, 164, 186, 209, 232, 256, 280, 303, 327, 350, 372,
		       394, 414, 433, 451, 467, 481, 493, 503, 511, 517, 521, 522, 521,
		       517, 511, 503, 493, 481, 467, 451, 433, 414, 394, 372, 350, 327,
		       303, 280, 256, 232, 209, 186, 164, 142, 122, 102,  84,  67,  51,
		        37,  24,  12,   2,  -6, -14, -20, -25, -29, -32, -34, -35, -35,
		       -35, -34, -33, -31, -29, -27, -24, -22, -20, -17, -15, -13, -11,
		        -9,  -7,  -6,  -4,  -3,  -2,  -1,  -1,   0,   0};

// 312.5kHz * 2/13 -> 48.076923kHz

struct {
	int index;
} resample_state;

struct {
	int write_current;
	int write_total;
	int read_total;
	int read_current;
} audio_state;

void resample_fir_filter2()
{
	const uint32_t *coeff;
	const uint16_t *src = (const uint16_t *)RESAMPLE_STATE;
	const uint32_t *s;
	int32_t tail = RESAMPLE_BUFFER_SIZE;
	int idx = resample_state.index;
	int32_t acc;
	int i, j;
	int cur = audio_state.write_current;
	uint16_t *dest = (uint16_t *)AUDIO_BUFFER;

	while (idx < tail) {
		if (idx & 0x1)
			coeff = (uint32_t*)resample_fir_coeff_odd;
		else
			coeff = (uint32_t*)resample_fir_coeff_even;

		acc = 0;
		s = (const uint32_t*)&src[idx >> 1];
		for (j = 0; j < RESAMPLE_NUM_TAPS / 2; j++) {
			acc = __SMLAD(*s++, *coeff++, acc);
		}
		dest[cur++] = __SSAT((acc >> 12), 16);
		cur %= AUDIO_BUFFER_SIZE / 2;
		audio_state.write_total++;
		//dest[cur++] = __PKHBT(__SSAT((acc0 >> 15), 16), __SSAT((acc1 >> 15), 16), 16);
		idx += 13;
		//idx += 59;
	}

	audio_state.write_current = cur;
	resample_state.index = idx - tail;
	uint32_t *state = (uint32_t *)RESAMPLE_STATE;
	src = &src[tail / sizeof(*src)];
	for (i = 0; i < RESAMPLE_STATE_SIZE / sizeof(uint32_t); i++) {
		//*state++ = *src++;
	    __asm__ volatile ("ldr r0, [%0], #+4\n" : : "r" (src) : "r0");
	    __asm__ volatile ("str r0, [%0], #+4\n" : : "r" (state));
	}
}
#if 0
void resample_fir_filter()
{
	const uint32_t *coeff;
	const uint32_t *src = (const uint32_t *)RESAMPLE_STATE;
	int32_t tail = RESAMPLE_BUFFER_SIZE;
	int idx = resample_state.index;
	uint32_t acc;
	uint32_t x0, c0, x1, x2;
	int i, j;
	int cur = resample_state.current;
	uint16_t *dest = (uint16_t *)AUDIO_BUFFER;

	while (idx < tail) {
		int cls = idx & 0x3;
		i = idx >> 2;

		switch (cls) {
		case 0:
		case 2:
			coeff = (uint32_t*)resample_fir_coeff_even;
			break;
		case 1:
		case 3:
			coeff = (uint32_t*)resample_fir_coeff_odd;
			break;
		}

		acc = 0;
		switch (cls) {
		case 0:
		case 1:
			for (j = 0; j < RESAMPLE_NUM_TAPS / 2; ) {
				x0 = src[i+j];
				c0 = coeff[j++];
				acc = __SMLAD(x0, c0, acc);
			}
			break;
		case 2:
		case 3:
			x0 = src[i];
			for (j = 0; j < RESAMPLE_NUM_TAPS / 2; ) {
				c0 = coeff[j++];
				x2 = src[i+j];
				x1 = __PKHBT(x2, x0, 0);
				acc = __SMLADX(x1, c0, acc);
				x0 = x2;
			}
			break;
		}
		dest[cur++] = __SSAT((acc >> 15), 16);
		cur %= AUDIO_BUFFER_SIZE / 2;
		//dest[cur++] = __PKHBT(__SSAT((acc0 >> 15), 16), __SSAT((acc1 >> 15), 16), 16);
		idx += 13;
	}

	resample_state.current = cur;
	resample_state.index = idx - tail;
	uint32_t *state = (uint32_t *)RESAMPLE_STATE;
	src = &src[tail / sizeof(*src)];
	for (i = 0; i < RESAMPLE_STATE_SIZE / sizeof(uint32_t); i++) {
		//*state++ = *src++;
	    __asm__ volatile ("ldr r0, [%0], #+4\n" : : "r" (src) : "r0");
	    __asm__ volatile ("str r0, [%0], #+4\n" : : "r" (state));
	}
}
#endif

void DMA_IRQHandler (void)
{
  if (LPC_GPDMA->INTERRSTAT & 1)
  {
    LPC_GPDMA->INTERRCLR = 1;
  }

//#define MEMCPY __aeabi_memcpy8

  if (LPC_GPDMA->INTTCSTAT & 1)
  {
	LPC_GPDMA->INTTCCLEAR = 1;
	//LPC_GPDMA->C0CONFIG |= (1 << 18); //halt further requests

	//TOGGLE_MEAS_PIN_3();
    const int length = CAPTUREBUFFER_SIZE / 2;
	SET_MEAS_PIN_3();
    if ((capture_count & 1) == 0) {
    	//MEMCPY(DEST_BUFFER, CAPTUREBUFFER, length);
    	cic_decimate_i(&cic_i, CAPTUREBUFFER, length);
    	cic_decimate_q(&cic_q, CAPTUREBUFFER, length);
    } else {
    	//MEMCPY(DEST_BUFFER + length, CAPTUREBUFFER + length, length);
    	//MEMCPY(DEST_BUFFER, CAPTUREBUFFER + length, length);
    	cic_decimate_i(&cic_i, CAPTUREBUFFER + length, length);
    	cic_decimate_q(&cic_q, CAPTUREBUFFER + length, length);
    }
	fir_filter_iq();
	fm_demod();
	resample_fir_filter2();
    CLR_MEAS_PIN_3();
    capture_count ++;
  }
}

static void VADC_SetupDMA(void)
{
  int i;
  uint32_t transSize;

  NVIC_DisableIRQ(DMA_IRQn);
  LPC_GPDMA->C0CONFIG = 0;

  /* clear all interrupts on channel 0 */
  LPC_GPDMA->INTTCCLEAR = 0x01;
  LPC_GPDMA->INTERRCLR = 0x01;

  /* Setup the DMAMUX */
  LPC_CREG->DMAMUX &= ~(0x3<<(VADC_DMA_WRITE*2));
  LPC_CREG->DMAMUX |= 0x3<<(VADC_DMA_WRITE*2);  /* peripheral 7 vADC Write(0x3) */
  LPC_CREG->DMAMUX &= ~(0x3<<(VADC_DMA_READ*2));
  LPC_CREG->DMAMUX |= 0x3<<(VADC_DMA_READ*2);  /* peripheral 8 vADC read(0x3) */

  LPC_GPDMA->CONFIG = 0x01;  /* Enable DMA channels, little endian */
  while ( !(LPC_GPDMA->CONFIG & 0x01) );

  // The size of the transfer is in multiples of 32bit copies (hence the /4)
  // and must be even multiples of FIFO_SIZE.
  transSize = CAPTUREBUFFER_SIZE / DMA_NUM_LLI_TO_USE / 4;

  for (i = 0; i < DMA_NUM_LLI_TO_USE; i++)
  {
    DMA_Stuff[i].SrcAddr = VADC_DMA_READ_SRC;
    DMA_Stuff[i].DstAddr = ((uint32_t)CAPTUREBUFFER) + transSize*4*i;
    DMA_Stuff[i].NextLLI = (uint32_t)(&DMA_Stuff[(i+1)%DMA_NUM_LLI_TO_USE]);
    DMA_Stuff[i].Control = (transSize << 0) |      // Transfersize (does not matter when flow control is handled by peripheral)
                           (0x2 << 12)  |          // Source Burst Size
                           (0x2 << 15)  |          // Destination Burst Size
                           //(0x0 << 15)  |          // Destination Burst Size
                           (0x2 << 18)  |          // Source width // 32 bit width
                           (0x2 << 21)  |          // Destination width   // 32 bits
                           (0x1 << 24)  |          // Source AHB master 0 / 1
                           (0x0 << 25)  |          // Dest AHB master 0 / 1
                           (0x0 << 26)  |          // Source increment(LAST Sample)
                           (0x1 << 27)  |          // Destination increment
                           (0x0UL << 31);          // Terminal count interrupt disabled
    //log_i("DMA_Stuff[%d] on address %#x, destination %#x, transfer size %#x (%d)\r\n", i, (uint32_t)&DMA_Stuff[i], DMA_Stuff[i].DstAddr, transSize, transSize);
  }

  // Let the last LLI in the chain cause a terminal count interrupt to
  // notify when the capture buffer is completely filled
  DMA_Stuff[DMA_NUM_LLI_TO_USE/2 - 1].Control |= (0x1UL << 31); // Terminal count interrupt enabled
  DMA_Stuff[DMA_NUM_LLI_TO_USE - 1].Control |= (0x1UL << 31); // Terminal count interrupt enabled

  LPC_GPDMA->C0SRCADDR = DMA_Stuff[0].SrcAddr;
  LPC_GPDMA->C0DESTADDR = DMA_Stuff[0].DstAddr;
  LPC_GPDMA->C0CONTROL = DMA_Stuff[0].Control;
  LPC_GPDMA->C0LLI     = (uint32_t)(&DMA_Stuff[1]); // must be pointing to the second LLI as the first is used when initializing
  LPC_GPDMA->C0CONFIG  =  (0x1)        |          // Enable bit
                          (VADC_DMA_READ << 1) |  // SRCPERIPHERAL - set to 8 - VADC
                          (0x0 << 6)   |          // Destination peripheral - memory - no setting
                          (0x2 << 11)  |          // Flow control - peripheral to memory - DMA control
                          (0x1 << 14)  |          // Int error mask
                          (0x1 << 15);            // ITC - term count error mask

  NVIC_EnableIRQ(DMA_IRQn);
}

#define RGU_SIG_VADC 60

static void VADC_Init(void)
{
  CGU_EntityConnect(CGU_CLKSRC_PLL0_AUDIO, CGU_BASE_VADC);
  CGU_EnableEntity(CGU_BASE_VADC, ENABLE);

  // Reset the VADC block
  RGU_SoftReset(RGU_SIG_VADC);
  while(RGU_GetSignalStatus(RGU_SIG_VADC));

  // Clear FIFO
  LPC_VADC->FLUSH = 1;

  // Disable the VADC interrupt
  NVIC_DisableIRQ(VADC_IRQn);
  LPC_VADC->CLR_EN0 = STATUS0_CLEAR_MASK;         // disable interrupt0
  LPC_VADC->CLR_STAT0 = STATUS0_CLEAR_MASK;       // clear interrupt status
  while(LPC_VADC->STATUS0 & 0x7d);  // wait for status to clear, have to exclude FIFO_EMPTY (bit 1)
  LPC_VADC->CLR_EN1 = STATUS1_CLEAR_MASK;          // disable interrupt1
  LPC_VADC->CLR_STAT1 = STATUS1_CLEAR_MASK;  // clear interrupt status
  while(LPC_VADC->STATUS1);         // wait for status to clear

  // Make sure the VADC is not powered down
  LPC_VADC->POWER_DOWN =
    (0<<0);        /* PD_CTRL:      0=disable power down, 1=enable power down */

  // Clear FIFO
  LPC_VADC->FLUSH = 1;

  // FIFO Settings
  LPC_VADC->FIFO_CFG =
    (1<<0) |         /* PACKED_READ:      0= 1 sample packed into 32 bit, 1= 2 samples packed into 32 bit */
    (FIFO_SIZE<<1);  /* FIFO_LEVEL:       When FIFO contains this or more samples raise FIFO_FULL irq and DMA_Read_Req, default is 8 */

  // Descriptors:
  if (ADCCLK_MATCHVALUE == 0)
  {
    // A matchValue of 0 requires special handling to prevent a automatic start.
    // For more information see the "Appendix A Errata" of the VADC manual.
    LPC_VADC->DSCR_STS =
      (1<<0) |       /* ACT_TABLE:        0=table 0 is active, 1=table 1 is active */
      (0<<1);        /* ACT_DESCRIPTOR:   ID of the descriptor that is active */

    LPC_VADC->DESCRIPTOR_1[0] =
      (0<<0) |       /* CHANNEL_NR:    0=convert input 0, 1=convert input 1, ..., 5=convert input 5 */
      (0<<3) |       /* HALT:          0=continue with next descriptor after this one, 1=halt after this and restart at a new trigger */
      (0<<4) |       /* INTERRUPT:     1=raise interrupt when ADC result is available */
      (0<<5) |       /* POWER_DOWN:    1=power down after this conversion */
      (2<<6) |       /* BRANCH:        0=continue with next descriptor (wraps around after top) */
                     /*                1=branch to the first descriptor in this table */
                     /*                2=swap tables and branch to the first descriptor of the new table */
                     /*                3=reserved (do not store sample). continue with next descriptor (wraps around the top) */
      (1<<8)  |      /* MATCH_VALUE:   Evaluate this desciptor when descriptor timer value is equal to match value */
      (0<<22) |      /* THRESHOLD_SEL: 0=no comparison, 1=THR_A, 2=THR_B */
      (1<<24) |      /* RESET_TIME:    1=reset descriptor timer */
      (1UL<<31);     /* UPDATE_TABLE:  1=update table with all 8 descriptors of this table */
  }
  else
  {
    LPC_VADC->DSCR_STS =
      (0<<0) |       /* ACT_TABLE:        0=table 0 is active, 1=table 1 is active */
      (0<<1);        /* ACT_DESCRIPTOR:   ID of the descriptor that is active */
  }

  LPC_VADC->CONFIG = /* configuration register */
    (1<<0) |        /* TRIGGER_MASK:     0=triggers off, 1=SW trigger, 2=EXT trigger, 3=both triggers */
    (0<<2) |        /* TRIGGER_MODE:     0=rising, 1=falling, 2=low, 3=high external trigger */
    (0<<4) |        /* TRIGGER_SYNC:     0=no sync, 1=sync external trigger input */
    (0<<5) |        /* CHANNEL_ID_EN:    0=don't add, 1=add channel id to FIFO output data */
    (0x90<<6);      /* RECOVERY_TIME:    ADC recovery time from power down, default is 0x90 */

  {
    LPC_VADC->DESCRIPTOR_0[0] =
      (0<<0) |       /* CHANNEL_NR:    0=convert input 0, 1=convert input 1, ..., 5=convert input 5 */
      (0<<3) |       /* HALT:          0=continue with next descriptor after this one, 1=halt after this and restart at a new trigger */
      (0<<4) |       /* INTERRUPT:     1=raise interrupt when ADC result is available */
      (0<<5) |       /* POWER_DOWN:    1=power down after this conversion */
      (1<<6) |       /* BRANCH:        0=continue with next descriptor (wraps around after top) */
                     /*                1=branch to the first descriptor in this table */
                     /*                2=swap tables and branch to the first descriptor of the new table */
                     /*                3=reserved (do not store sample). continue with next descriptor (wraps around the top) */
      (ADCCLK_MATCHVALUE<<8)  |    /* MATCH_VALUE:   Evaluate this desciptor when descriptor timer value is equal to match value */
      (0<<22) |      /* THRESHOLD_SEL: 0=no comparison, 1=THR_A, 2=THR_B */
      (1<<24) |      /* RESET_TIME:    1=reset descriptor timer */
      (1UL<<31);       /* UPDATE_TABLE:  1=update table with all 8 descriptors of this table */
  }

  LPC_VADC->ADC_SPEED =
    ADCCLK_DGECI;   /* DGECx:      For CRS=3 all should be 0xF, for CRS=4 all should be 0xE, */
                       /*             for all other cases it should be 0 */

  LPC_VADC->POWER_CONTROL =
    (0 /*crs*/ << 0) |    /* CRS:          current setting for power versus speed programming */
    (1 << 4) |      /* DCINNEG:      0=no dc bias, 1=dc bias on vin_neg slide */
    (0 << 10) |     /* DCINPOS:      0=no dc bias, 1=dc bias on vin_pos slide */
    (0 << 16) |     /* TWOS:         0=offset binary, 1=two's complement */
    (1 << 17) |     /* POWER_SWITCH: 0=ADC is power gated, 1=ADC is active */
    (1 << 18);      /* BGAP_SWITCH:  0=ADC bandgap reg is power gated, 1=ADC bandgap is active */

//  LPC_VADC->SET_EN0 = STATUS0_FIFO_FULL_MASK;// only care about FIFO_FULL

  // Enable interrupts
  //NVIC_EnableIRQ(VADC_IRQn);

  VADC_SetupDMA();
}

static void VADC_Start(void)
{
	capture_count = 0;
	LPC_VADC->TRIGGER = 1;
}

static void VADC_Stop(void)
{
  NVIC_DisableIRQ(DMA_IRQn);
  NVIC_DisableIRQ(VADC_IRQn);

  // disable DMA
  LPC_GPDMA->C0CONFIG |= (1 << 18); //halt further requests
/*
  // power down VADC
  LPC_VADC->POWER_CONTROL = 0;

  // Clear FIFO
  LPC_VADC->FLUSH = 1;

  // Reset the VADC block
  RGU_SoftReset(RGU_SIG_VADC);
  while(RGU_GetSignalStatus(RGU_SIG_VADC));
*/
}

static void priorityConfig()
{
  // High - Copying of samples
  NVIC_SetPriority(DMA_IRQn,   ((0x01<<3)|0x01));

  // Low - Communication
  NVIC_SetPriority(USB0_IRQn, ((0x03<<3)|0x01));
  NVIC_SetPriority(USB1_IRQn, ((0x03<<3)|0x01));
  NVIC_SetPriority(I2C0_IRQn, ((0x03<<3)|0x01));
}


static int I2CWrite(uint8_t addr, uint8_t data0, uint8_t data1)
{
	I2C_M_SETUP_Type txsetup;
	uint8_t buf[2];
	txsetup.sl_addr7bit = addr;
	txsetup.tx_data = buf;
	txsetup.tx_length = sizeof buf;
	txsetup.rx_data = NULL;
	txsetup.rx_length = 0;
	txsetup.retransmissions_max = 3;
	buf[0] = data0;
	buf[1] = data1;
	if (I2C_MasterTransferData(LPC_I2C0, &txsetup, I2C_TRANSFER_POLLING) == SUCCESS){
		return (0);
	} else {
		return (-1);
	}
}

static void ConfigureTLV320(uint32_t rate)
{
    I2S_CFG_Type i2sCfg;
    I2S_MODEConf_Type i2sMode;

	// Initialize I2C peripheral
	I2C_Init(LPC_I2C0, 100000);

    /* Enable I2C0 operation */
	I2C_Cmd(LPC_I2C0, ENABLE);
	I2CWrite(0x18, 0x00, 0x00); /* Initialize to Page 0 */
	I2CWrite(0x18, 0x01, 0x01); /* Initialize the device through software reset */
	I2CWrite(0x18, 0x05, 0x91); /* Power up PLL, P=1,R=1 */
	I2CWrite(0x18, 0x06, 0x07); /* J=7 */
	I2CWrite(0x18, 0x07, 6);    /* D=(6 <<8) + 144 */
	I2CWrite(0x18, 0x08, 144);
	I2CWrite(0x18, 0x0b, 0x82); /* Power up the NDAC divider with value 2 */
	I2CWrite(0x18, 0x0c, 0x87); /* Power up the MDAC divider with value 7 */
	I2CWrite(0x18, 0x0d, 0x00); /* Program the OSR of DAC to 128 */
	I2CWrite(0x18, 0x0e, 0x80);
	I2CWrite(0x18, 0x3c, 0x08); /* Set the DAC Mode to PRB_P8 */
	I2CWrite(0x18, 0x25, 0xee); /* DAC power up */
	I2CWrite(0x18, 0x00, 0x01); /* Select Page 1 */
	I2CWrite(0x18, 0x01, 0x08); /* Disable Internal Crude AVdd in presence of external AVdd supply or before powering up internal AVdd LDO*/
	I2CWrite(0x18, 0x02, 0x01); /* Enable Master Analog Power Control */
	I2CWrite(0x18, 0x7b, 0x01); /* Set the REF charging time to 40ms */
	I2CWrite(0x18, 0x14, 0x25); /* HP soft stepping settings for optimal pop performance at power up Rpop used is 6k with N = 6 and soft step = 20usec. This should work with 47uF coupling capacitor. Can try N=5,6 or 7 time constants as well. Trade-off delay vs “pop” sound. */
	I2CWrite(0x18, 0x0a, 0x00); /* Set the Input Common Mode to 0.9V and Output Common Mode for Headphone to Input Common Mode */
	I2CWrite(0x18, 0x0c, 0x08); /* Route Left DAC to HPL */
	I2CWrite(0x18, 0x0d, 0x08); /* Route Right DAC to HPR */
	I2CWrite(0x18, 0x03, 0x00); /* Set the DAC PTM mode to PTM_P3/4 */
	I2CWrite(0x18, 0x04, 0x00);
	I2CWrite(0x18, 0x10, 0x00); /* Set the HPL gain to 0dB */
	I2CWrite(0x18, 0x11, 0x00); /* Set the HPR gain to 0dB */
	I2CWrite(0x18, 0x09, 0x30); /* Power up HPL and HPR drivers */
	I2CWrite(0x18, 0x00, 0x00); /* Select Page 0 */
	I2CWrite(0x18, 0x3f, 0xd6); /* Power up the Left and Right DAC Channels with route the Left Audio digital data to Left Channel DAC and Right Audio digital data to Right Channel DAC */
	I2CWrite(0x18, 0x40, 0x00); /* Unmute the DAC digital volume control */

    // Configure I2S pins
    scu_pinmux(0x3, 0, MD_PLN_FAST, FUNC2);     // SCK
    //scu_pinmux(0x3, 0, MD_PLN_FAST, FUNC3);     // MCLK
    //scu_pinmux(0xC, 12, MD_PLN_FAST, FUNC6);    // SD
/**/scu_pinmux(0x3, 1, MD_PLN_FAST, FUNC0);     // WS
/**/scu_pinmux(0x3, 2, MD_PLN_FAST, FUNC0);    // SD
    scu_pinmux(0x3, 4, MD_PLN_FAST, FUNC5);     // WS
    scu_pinmux(0xF, 4, MD_PLN_FAST, FUNC6);    // MCLK

	// output clock to TP_CLK0 for diagnosis
	//LPC_CGU->BASE_OUT_CLK = CGU_CLKSRC_IRC << 24;
	LPC_CGU->BASE_OUT_CLK = CGU_CLKSRC_XTAL_OSC << 24;
	//LPC_CGU->BASE_OUT_CLK = CGU_CLKSRC_PLL0_AUDIO << 24;
	LPC_SCU->SFSCLK_0 = 0x1;

    // Initialize I2S
    I2S_Init(LPC_I2S0);

    // Configure I2S
    i2sCfg.wordwidth = I2S_WORDWIDTH_16;
    i2sCfg.mono      = I2S_MONO;
    i2sCfg.stop      = I2S_STOP_ENABLE;
    i2sCfg.reset     = I2S_RESET_ENABLE;
    i2sCfg.ws_sel    = I2S_MASTER_MODE;
    i2sCfg.mute      = I2S_MUTE_DISABLE;
    I2S_Config(LPC_I2S0, I2S_TX_MODE, &i2sCfg);

    // Configure operating mode
    i2sMode.clksel = I2S_CLKSEL_FRDCLK;
    i2sMode.fpin   = I2S_4PIN_DISABLE;
    //i2sMode.mcena  = I2S_MCLK_DISABLE;
    i2sMode.mcena  = I2S_MCLK_ENABLE;
    I2S_ModeConfig(LPC_I2S0, &i2sMode, I2S_TX_MODE);

    // Configure sampling frequency
    I2S_FreqConfig(LPC_I2S0, rate, I2S_TX_MODE);
    //FreqConfig(LPC_I2S0, rate, I2S_TX_MODE);

    I2S_Stop(LPC_I2S0, I2S_TX_MODE);

    I2S_IRQConfig(LPC_I2S0, I2S_TX_MODE, 4);
    I2S_IRQCmd(LPC_I2S0, I2S_TX_MODE, ENABLE);
    I2S_Start(LPC_I2S0);
    NVIC_EnableIRQ(I2S0_IRQn);
}

void I2S0_IRQHandler()
{
#if 1
	uint32_t txLevel = I2S_GetLevel(LPC_I2S0, I2S_TX_MODE);
	int rest = audio_state.write_total - audio_state.read_total;
	if (txLevel <= 4) {
		// Fill the remaining FIFO
		int cur = audio_state.read_current;
		int16_t *buffer = (int16_t*)AUDIO_BUFFER;
		int i;
		for (i = 0; i < (8 - txLevel); i++) {
			LPC_I2S0->TXFIFO = *(uint32_t *)&buffer[cur];
			cur += 2;
			cur %= AUDIO_BUFFER_SIZE / 2;
			audio_state.read_total += 2;
		}
		audio_state.read_current = cur;
	}
#endif
}

int main(void) {
	setup_systemclock();
    setup_pll0audio(PLL0_MSEL, PLL0_NSEL, PLL0_PSEL);
    priorityConfig();

    printf("Hello World\n");
    GPIO_SetDir(0,1<<8, 1);
	GPIO_ClearValue(0,1<<8);

	scu_pinmux(0x6, 11, SETTINGS_GPIO_OUT, FUNC0); //GPIO3[7], available on J7-14
	LPC_GPIO_PORT->DIR[3] |= (1UL << 7);
	LPC_GPIO_PORT->SET[3] |= (1UL << 7);

	//ConfigureNCOTable(2500000 / 20000); // 2.5MHz
	//ConfigureNCOTable(2500000 / 5000); // 2.5MHz
	//ConfigureNCOTable(1000000 / 5000); // 1MHz
	//ConfigureNCOTable(420000 / 5000); // 400kHz
	ConfigureNCOTable(420000 / 10000); // 400kHz
	//ConfigureNCOTable(0); // 0MHz
	memset(&cic_i, 0, sizeof cic_i);
	memset(&cic_q, 0, sizeof cic_q);
	//arm_fir_init_q15(&fir, FIR_NUM_TAPS, fir_coeff, fir_state, FIR_BLOCK_SIZE);

	ConfigureTLV320(48000);

	VADC_Init();

	/* wait 5 msec */
	emc_WaitUS(5000);

	capture_count = 0;
	VADC_Start();
    //volatile static int i = 0 ;

	int i;
	int16_t *buf = (int16_t*)AUDIO_BUFFER2;
	for (i = 0; i < 0x1000; i++) {
		float res = arm_sin_f32((float)i * 2 * PI * 440 / 48000);
		buf[i] = (int)(res * 20000.0);
	}

    while(1) {
        //i++ ;
        if ((capture_count % 2048) == 2047) {
        	//int i;
//        	LPC_GPDMA->C0CONFIG |= (1 << 18); //halt further requests
            printf("write:%d read:%d\n", audio_state.write_total, audio_state.read_total);
//        	GPIO_SetValue(0,1<<8);

        	//int length = CAPTUREBUFFER_SIZE / 2;
        	//memset(DEST_BUFFER, 0, DEST_BUFFER_SIZE);
            //cic_decimate(&cic1, CAPTUREBUFFER, length);

        	//GPIO_SetValue(0,1<<8);
        	//SET_MEAS_PIN_3();
        	//fir_filter_iq();
        	//fm_demod();
//        	resample_fir_filter2();
#if 0
    		q15_t *src = I_DEST_BUFFER;
    		q15_t *dst = I_FIR_BUFFER;
        	for (i = 0; i < FIR_NUM_BLOCKS; i++) {
        		arm_fir_fast_q15(&fir, src, dst, FIR_BLOCK_SIZE);
        		src += FIR_BLOCK_SIZE;
        		dst += FIR_BLOCK_SIZE;
        	}
#endif
        	//CLR_MEAS_PIN_3();
        } else if ((capture_count % 2048) < 1024) {
        	GPIO_SetValue(0,1<<8);
    	} else
        	GPIO_ClearValue(0,1<<8);
        //if ((i & 0x1fffff) == 0x100000)
        //	printf("%d\n", i);
#if 0
        {
        	uint32_t txLevel = I2S_GetLevel(LPC_I2S0, I2S_TX_MODE);
        	int rest = audio_state.write_total - audio_state.read_total;
        	if (txLevel <= 4) {
        		// Fill the remaining FIFO
        		int cur = audio_state.read_current;
        		int16_t *buffer = (int16_t*)AUDIO_BUFFER2;
        		//int16_t *buffer = (int16_t*)AUDIO_BUFFER;
        		int i;
        		for (i = 0; i < (8 - txLevel); i++) {
        			LPC_I2S0->TXFIFO = *(uint32_t *)&buffer[cur];
        			cur += 2;
        			cur %= AUDIO_BUFFER_SIZE / 2;
        			audio_state.read_total += 2;
        		}
        		audio_state.read_current = cur;
            }
        }
#endif
    }
	VADC_Stop();
    return 0 ;
}
