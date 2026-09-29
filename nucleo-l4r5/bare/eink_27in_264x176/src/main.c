/*
  main.c - Pervasive Displays 2.7" E-INK (264x176) demo for the nucleo-l4r5
  (STM32L4R5ZIT6 @ 120 MHz).

  Ported from the Keil MDK project D:\stm32l4r5-demo ("Eink Driver Demo for
  L4R5 Nucleo board", l4r5_t1). The panel hardware, its wiring, the vendor
  driver and the demo behaviour are unchanged; what changed is the
  surrounding environment:

    - built with this repo's CMake/Ninja toolchains and shared board layer
      (board/board.c: 120 MHz clock, LEDs, button, console) instead of the
      Keil project's CubeMX clock and peripheral files;
    - the console is the board's LPUART1 (PG7/PG8 -> ST-Link VCP) at
      115200 8-N-1. The original retargeted printf() to ITM/SWO through the
      Keil CMSIS-Compiler "STDOUT: ITM" component; this port has no ITM or
      SWO dependency - printf() goes through the board's _write() to the UART;
    - the two demo images are now the BMPs in ../eink_assets/, converted to
      the panel's frame format by tools/bmp_to_eink.py (see src/resource.c).

  Behaviour: power up the panel, clear to white, then alternate board_0 and
  board_1 every LOGO_DELAY ms with a 2-stage full update, toggling LD2 and
  printing the ADC buffer once per iteration - the same loop as the original.

  Wiring (identical to the original project, Inc/main.h):
    SCK=PA5  MISO=PA6  MOSI=PA7 (SPI1 AF5, mode 0, 30 MHz)
    CS=PD14  RST=PF15  BUSY=PE13  PWR_EN=PF14  IO_EN=PF13
    BRD_CTRL=PE9  DISCHARGE=PE11
*/

#include "stm32l4xx_hal.h"
#include "main.h"
#include "eink_board.h"

#include "pervasive_eink_configuration.h"
#include "pervasive_eink_hardware_driver.h"
#include "resource.h"

#include <stdio.h>

/* Time in mS during which each image is displayed */
#define LOGO_DELAY                      (uint16_t)(25000u)

/* Data type of E-INK update types */
typedef enum
{
    CY_EINK_PARTIAL,
    CY_EINK_FULL_4STAGE,
    CY_EINK_FULL_2STAGE
}   cy_eink_update_t;

/* Variable that stores the pointer to the current frame being displayed.
 *
 * The original main.c also declared two RAM frame buffers and a
 * `currentFrameBuffer` index for them, but never read or wrote them: every
 * update is driven straight from the flash image (`currentFrame` below), which
 * is why both were dead in the Keil build too (and are dropped by
 * --gc-sections here). They are not carried over - the panel's own
 * "previous frame" argument is the flash image / the WHITE sentinel. */
static uint8_t *currentFrame = PV_EINK_WHITE_FRAME_ADDRESS;

/* Images (converted from ../eink_assets/board_*.bmp by tools/bmp_to_eink.py) */
#define IMAGE_COUNT 2

static const uint8_t *const images[IMAGE_COUNT] =
{
    board_0,
    board_1
};

static const char *const imageNames[IMAGE_COUNT] =
{
    "board_0",
    "board_1"
};

/* LED helpers kept tiny so the demo loop reads like the original, which drove
 * LD1/LD2 through HAL_GPIO_WritePin/TogglePin directly; the pin numbers come
 * from the board layer (LD1 = PC7, LD2 = PB7). */
#define LD1_OFF_INDICATE()   LED1_OFF()
#define LD1_ON_INDICATE()    LED1_ON()

/* ------------------------------------------------------------------------ */
/* ADC buffer - printed every loop iteration, exactly as the original did.   */
#define ADC_NUMOFCHANNEL    3

static volatile uint16_t g_adcBuf[ADC_NUMOFCHANNEL];

/*******************************************************************************
* Function Name: bool ST_EINK_Power(bool powerCtrl)
********************************************************************************
*
* Summary: This function is used to turn on/off the E-INK display power.
*
*  Note: This function can not be used to clear the E-INK display. The display
*  will retain the previously written frame even when it's turned off.
*
* Parameters:
*  bool powerCtrl : "False" turns off and "True" turns on the display.
*
* Return:
*  bool           : "True" if operation was successful; "False" otherwise
*******************************************************************************/
static bool ST_EINK_Power(bool powerCtrl)
{
    pv_eink_status_t pwrStatus;

    if (powerCtrl == true)
    {
        pwrStatus = Pv_EINK_HardwarePowerOn();
    }
    else
    {
        pwrStatus = Pv_EINK_HardwarePowerOff();
    }

    /* The original collapsed this to a single bool. With a real UART console
     * available it is worth naming the failure: PV_EINK_ERROR_ID means the
     * panel did not answer on MISO (wiring), while BUSY/BREAKAGE/CHARGE_PUMP
     * are panel-side conditions. */
    if ((powerCtrl == true) && (pwrStatus != PV_EINK_RES_OK))
    {
        static const char *const errName[] =
        {
            "OK", "ERROR_BUSY", "ERROR_ID", "ERROR_BREAKAGE",
            "ERROR_DC", "ERROR_CHARGEPUMP"
        };
        unsigned e = ((unsigned)pwrStatus < 6U) ? (unsigned)pwrStatus : 6U;
        printf("[EINK] Pv_EINK_HardwarePowerOn: %s\r\n",
               (e < 6U) ? errName[e] : "ERROR_UNKNOWN");
    }

    return (pwrStatus == PV_EINK_RES_OK);
}

/*******************************************************************************
* Function Name: void ST_EINK_Clear(bool background, bool powerCycle)
********************************************************************************
*
* Summary: This function is used to clear the display to all white or all black
*  pixels.
*
*  Note1: The E-INK display should be powered on (using ST_EINK_Power) before
*  calling this function if "powerCycle" is false.
*
*  Note2: This function is intended to be called only after a reset/power up.
*
* Parameters:
*  bool background   : False for black background and True for white background.
*  bool powerCycle   : True for automatic power cycle. False otherwise
*******************************************************************************/
static void ST_EINK_Clear(bool background, bool powerCycle)
{
    if (powerCycle)
    {
        ST_EINK_Power(true);
    }

    if (background == true)
    {
        /* Two consecutive display updates to reduce ghosting */
        Pv_EINK_FullStageHandler(PV_EINK_WHITE_FRAME_ADDRESS, PV_EINK_STAGE4);
        Pv_EINK_FullStageHandler(PV_EINK_WHITE_FRAME_ADDRESS, PV_EINK_STAGE4);
    }
    else
    {
        /* Two consecutive display updates to reduce ghosting */
        Pv_EINK_FullStageHandler(PV_EINK_BLACK_FRAME_ADDRESS, PV_EINK_STAGE4);
        Pv_EINK_FullStageHandler(PV_EINK_BLACK_FRAME_ADDRESS, PV_EINK_STAGE4);
    }

    if (powerCycle)
    {
        ST_EINK_Power(false);
    }
}

/*******************************************************************************
* Function Name: void ST_EINK_ShowFrame(uint8_t* prevFrame,
*      uint8_t* newFrame, cy_eink_update_t updateType, bool powerCycle)
********************************************************************************
*
* Summary: Updates the E-INK display with a frame/image stored in the flash or
*  RAM.
*
*  Notes: This function requires the previous frame data as well as the new
*  frame data. If the previous frame data changes from the actual frame
*  previously written to the display, considerable ghosting may occur.
*
*  The E-INK display should be powered on (using ST_EINK_Power) before calling
*  this function, if "powerCycle" parameter is false.
*******************************************************************************/
static void ST_EINK_ShowFrame(uint8_t *prevFrame, uint8_t *newFrame,
                              cy_eink_update_t updateType, bool powerCycle)
{
    if (powerCycle)
    {
        ST_EINK_Power(true);
    }

    if (updateType == CY_EINK_PARTIAL)
    {
        Pv_EINK_PartialStageHandler(prevFrame, newFrame);
    }
    else if ((updateType == CY_EINK_FULL_4STAGE) ||
             (updateType == CY_EINK_FULL_2STAGE))
    {
        /* Stage 1: update the display with the inverted previous frame */
        Pv_EINK_FullStageHandler(prevFrame, PV_EINK_STAGE1);

        if (updateType == CY_EINK_FULL_4STAGE)
        {
            /* Stage 2: all white frame */
            Pv_EINK_FullStageHandler(prevFrame, PV_EINK_STAGE2);
            /* Stage 3: inverted new frame */
            Pv_EINK_FullStageHandler(newFrame, PV_EINK_STAGE3);
        }

        /* Stage 4: the new frame */
        Pv_EINK_FullStageHandler(newFrame, PV_EINK_STAGE4);
    }

    if (powerCycle)
    {
        ST_EINK_Power(false);
    }
}

/* ------------------------------------------------------------------------ */
static void InitDisplay(void)
{
    printf("[EINK] Pv_EINK_Init\r\n");
    Pv_EINK_Init();

    /* Temperature compensation of the E-INK parameters. The original had this
     * hard-coded rather than reading a sensor, and it is kept as-is. */
    Pv_EINK_SetTempFactor(41);

    if (ST_EINK_Power(true) == true)
    {
        /* Clear to BLACK first, then white.
         *
         * The original cleared straight to white, which makes a dead SPI bus
         * indistinguishable from a working one: a fresh panel is already
         * white, so "cleared to white" and "nothing happened" look identical.
         * A black pass first gives an unmistakable signal that the panel is
         * actually being driven, and it also exercises the
         * PV_EINK_BLACK_FRAME_ADDRESS sentinel path that the demo otherwise
         * never uses (the sentinel is the last byte of the 2 MB flash, which
         * this part has - see the note in the driver). */
        printf("[EINK] power on OK, clearing black then white\r\n");
        ST_EINK_Clear(false, true);
        ST_EINK_Clear(true, true);
    }
    else
    {
        printf("[EINK] power on FAILED (see the status above)\r\n");
        __SEV();
    }
}

static void DisplayImage(const uint8_t *imagePointer, const char *name)
{
    printf("[EINK] update -> %s\r\n", name);

    LD1_OFF_INDICATE();

    /* 2-stage full update, as the original: fast refresh with reduced
     * ghosting for images that are similar to each other. */
    ST_EINK_ShowFrame(currentFrame, (uint8_t *)imagePointer,
                      CY_EINK_FULL_2STAGE, true);

    /* Store the pointer to the current image, needed for the next update */
    currentFrame = (uint8_t *)imagePointer;

    LD1_ON_INDICATE();
}

int main(void)
{
    unsigned index = 0U;

    HAL_Init();

    /* Clocks (120 MHz), LEDs, button, console - then the panel peripherals
     * (GPIO, DMA, ADC1, SPI1) in the original main()'s order. */
    Board_Eink_Init();

    printf("\r\n==== nucleo-l4r5 (STM32L4R5ZIT6) eink_27in_264x176 @ %lu MHz ====\r\n",
           (unsigned long)(SystemCoreClock / 1000000UL));
    printf("2.7\" E-INK 264x176 (Pervasive Displays), SPI1 mode 0 @ 30 MHz\r\n");
    printf("SCK=PA5 MISO=PA6 MOSI=PA7 CS=PD14 RST=PF15 BUSY=PE13\r\n");
    printf("PWR_EN=PF14 IO_EN=PF13 BRD=PE9 DISCHARGE=PE11\r\n");
    printf("console: LPUART1 PG7/PG8 @ 115200 (was ITM/SWO in the Keil build)\r\n");
    printf("images: %s, %s (from ../eink_assets/*.bmp)\r\n",
           imageNames[0], imageNames[1]);

    InitDisplay();

    while (1)
    {
        /* The original prints the ADC buffer every iteration. Uncomment the
         * next line to actually run the conversion (see MX_ADC1_Init() in
         * eink_board.c) - with it commented, as in the original, the values
         * stay 0. */
        /* HAL_ADC_Start_DMA(&hadc1, (uint32_t *)&g_adcBuf, ADC_NUMOFCHANNEL); */

        printf("%u %u %u\r\n",
               (unsigned)g_adcBuf[0],
               (unsigned)g_adcBuf[1],
               (unsigned)g_adcBuf[2]);

        LD2_TOGGLE();

        DisplayImage(images[index], imageNames[index]);
        HAL_Delay(LOGO_DELAY);

        LD2_TOGGLE();

        index = (index + 1U) % IMAGE_COUNT;
    }
}
