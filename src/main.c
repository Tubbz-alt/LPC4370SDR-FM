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
#include <arm_math.h>

#include "fmreceiver.h"


#define VADC_DMA_WRITE  7
#define VADC_DMA_READ   8
#define VADC_DMA_READ_SRC  (LPC_VADC_BASE + 512)  /* VADC FIFO */
#define FIFO_SIZE       8

#define DMA_NUM_LLI_TO_USE    16
static GPDMA_LLI_Type DMA_Stuff[DMA_NUM_LLI_TO_USE];

#if 1
// PLL0AUDIO: 39.936MHz = (12MHz / 25) * (416 * 2) / (5 * 2)
#define PLL0_MSEL	416
#define PLL0_NSEL	25
#define PLL0_PSEL	5
#else
// PLL0AUDIO: 40MHz = (12MHz / 15) * (400 * 2) / (8 * 2)
#define PLL0_MSEL	400
#define PLL0_NSEL	15
#define PLL0_PSEL	8
#endif
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

volatile int32_t capture_count;


static void VADC_SetupDMA(void)
{
  int i;
  uint32_t transfersize;
  uint32_t blocksize;
  uint8_t *buffer;

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
  buffer = CAPTUREBUFFER0;
  blocksize = CAPTUREBUFFER_SIZE / DMA_NUM_LLI_TO_USE;
  transfersize = blocksize / 4;

  for (i = 0; i < DMA_NUM_LLI_TO_USE; i++)
  {
	if (i == DMA_NUM_LLI_TO_USE / 2)
		buffer = CAPTUREBUFFER1;
    DMA_Stuff[i].SrcAddr = VADC_DMA_READ_SRC;
    DMA_Stuff[i].DstAddr = (uint32_t)buffer;
    DMA_Stuff[i].NextLLI = (uint32_t)(&DMA_Stuff[(i+1)%DMA_NUM_LLI_TO_USE]);
    DMA_Stuff[i].Control = (transfersize << 0) |      // Transfersize (does not matter when flow control is handled by peripheral)
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
    buffer += blocksize;
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
//                          (0x6 << 11)  |          // Flow control - peripheral to memory - peripheral control
                          (0x1 << 14)  |          // Int error mask
                          (0x1 << 15);            // ITC - term count error mask

  NVIC_EnableIRQ(DMA_IRQn);
}

#define RGU_SIG_VADC 60

static void VADC_Init(void)
{
  CGU_EntityConnect(CGU_CLKSRC_PLL0_AUDIO, CGU_BASE_VADC);
  CGU_EnableEntity(CGU_BASE_VADC, ENABLE);

//  RGU_SoftReset(RGU_SIG_DMA);
//  while(RGU_GetSignalStatus(RGU_SIG_DMA));

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
}

static void VADC_Start(void)
{
	capture_count = 0;
	LPC_VADC->TRIGGER = 1;
}

static void VADC_Stop(void)
{
  // disable DMA
  LPC_GPDMA->C0CONFIG |= (1 << 18); //halt further requests

  NVIC_DisableIRQ(I2S0_IRQn);
  NVIC_DisableIRQ(DMA_IRQn);
  //NVIC_DisableIRQ(VADC_IRQn);

  LPC_VADC->TRIGGER = 0;
  // Clear FIFO
  LPC_VADC->FLUSH = 1;
  // power down VADC
  LPC_VADC->POWER_CONTROL = 0;

  // Reset the VADC block
  RGU_SoftReset(RGU_SIG_VADC);
  while(RGU_GetSignalStatus(RGU_SIG_VADC));
}

static void priorityConfig()
{
  // High - Copying of samples
  NVIC_SetPriority(DMA_IRQn,   ((0x01<<3)|0x01));
  NVIC_SetPriority(I2S0_IRQn,  ((0x02<<3)|0x01));

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

// -7 ~ 29
void audio_set_gain(int gain)
{
	if (gain < -6)
		gain = 0x40;
	else if (gain > 29)
		gain = 29;
	else
		gain &= 0x3f;

	I2CWrite(0x18, 0x00, 0x01); /* Select Page 1 */
	I2CWrite(0x18, 0x10, gain); /* HPL Driver Gain */
	I2CWrite(0x18, 0x11, gain); /* HPR Driver Gain */
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
	I2CWrite(0x18, 0x04, 0x43); /* PLL Clock High, MCLK, PLL */
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
//	I2CWrite(0x18, 0x0a, 0x00); /* Set the Input Common Mode to 0.9V and Output Common Mode for Headphone to Input Common Mode */
	I2CWrite(0x18, 0x0a, 0x33); /* Set the Input Common Mode to 0.9V and Output Common Mode for Headphone to 1.65V */
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

	// output XTAL_OSC to TP_CLK0 for MCLK
	LPC_CGU->BASE_OUT_CLK = CGU_CLKSRC_XTAL_OSC << 24;
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

    I2S_Stop(LPC_I2S0, I2S_TX_MODE);

    I2S_IRQConfig(LPC_I2S0, I2S_TX_MODE, 4);
    I2S_IRQCmd(LPC_I2S0, I2S_TX_MODE, ENABLE);
    I2S_Start(LPC_I2S0);
    NVIC_EnableIRQ(I2S0_IRQn);
}

int main(void) {
    setup_systemclock();
    setup_pll0audio(PLL0_MSEL, PLL0_NSEL, PLL0_PSEL);

    // Setup SysTick Timer to interrupt at 1 msec intervals
	SysTick_Config(CGU_GetPCLKFrequency(CGU_PERIPHERAL_M4CORE)/1000);
    priorityConfig();

    VADC_Stop();

    //printf("Hello SDR!\n");
    LED_INIT();
    ui_init();
	dsp_init();

	scu_pinmux(0x6, 11, SETTINGS_GPIO_OUT, FUNC0); //GPIO3[7], available on J7-14
	LPC_GPIO_PORT->DIR[3] |= (1UL << 7);
	LPC_GPIO_PORT->SET[3] |= (1UL << 7);

	//ConfigureNCO(80400000);
	ConfigureNCO(82500000);
	//ConfigureNCO(85200000);
	ConfigureTLV320(48000);
	VADC_Init();
    VADC_SetupDMA();

	/* wait 5 msec */
	//emc_WaitUS(5000);
	//generate_test_tone(440);

	VADC_Start();

    while(1) {
    	ui_process();

    	//if ((capture_count % 1024) < 512) {
    	// 	GPIO_SetValue(0,1<<8);
    	//} else {
    	//   	GPIO_ClearValue(0,1<<8);
    	//}

    	//printf("%08x %08x\n", LPC_VADC->FIFO_OUTPUT[0], LPC_VADC->FIFO_OUTPUT[1]);
    	//emc_WaitUS(500000);

#if 0
        if ((capture_count % 2048) == 2047) {
        	//int i;
        	//LPC_GPDMA->C0CONFIG |= (1 << 18); //halt further requests
            //printf("write:%d read:%d\n", audio_state.write_total, audio_state.read_total);
            //printf("diff:%d\n", audio_state.write_total - audio_state.read_total);
            //printf("rebuf:%d\n", audio_state.rebuffer_count);
//        	GPIO_SetValue(0,1<<8);

        	//int length = CAPTUREBUFFER_SIZE / 2;
        	//memset(DEST_BUFFER, 0, DEST_BUFFER_SIZE);
            //cic_decimate(&cic1, CAPTUREBUFFER, length);

        	//GPIO_SetValue(0,1<<8);
        	//SET_MEAS_PIN_3();
        	//fir_filter_iq();
        	//fm_demod();
//        	resample_fir_filter2();
        	//CLR_MEAS_PIN_3();
        	//break;
            //printf("carrier:%d\n", fm_demod_state.carrier);
        }
#endif
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


#ifdef DEBUG
void check_failed(uint8_t *file, uint32_t line)
{
/* User can add his own implementation to report the file name and line number,
ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

/* Infinite loop */
while(1);
}
#endif

