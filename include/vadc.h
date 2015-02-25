/*
Copyright (c) 2014-2015, TAKAHASHI Tomohiro (TTRFTECH) edy555@gmail.com
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
1. Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.
3. All advertising materials mentioning features or use of this software
   must display the following acknowledgement:
   This product includes software developed by the TTRFTECH.
4. Neither the name of the TTRFTECH nor the names of its contributors
   may be used to endorse or promote products derived from this software
   without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY TTRFTECH ''AS IS'' AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL TTRFTECH BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
/*
 * @copyright Copyright 2013 Embedded Artists AB
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation is hereby granted, without fee, provided that it
 * is used in conjunction with NXP Semiconductors microcontrollers.  This
 * copyright, permission, and disclaimer notice must appear in all copies of
 * this code.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef __VADC_H__
#define __VADC_H__

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

#define CGU_BASE_VADC CGU_BASE_ENET_CSR
#define VADC_IRQn RESERVED7_IRQn

#define VADC_DMA_WRITE  7
#define VADC_DMA_READ   8
#define VADC_DMA_READ_SRC  (LPC_VADC_BASE + 512)  /* VADC FIFO */

#define RGU_SIG_VADC 60


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

#endif /* __VADC_H__ */
