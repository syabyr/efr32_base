/**
 * UART common utilities source file
 * @copyright 2019 Silicon Laboratories Inc.
 */


#include <ZAF_uart_utils.h>
#include <em_usart.h>
#include <em_cmu.h>

static void UartEnable(USART_TypeDef * uart, uint32_t baudRate, bool bEnableTx, bool bEnableRx);
static void UartSetTxOutput(USART_TypeDef * usart, GPIO_Port_TypeDef port, unsigned int pin);
static void UartSetRxInput(USART_TypeDef * usart, GPIO_Port_TypeDef port, unsigned int pin);
static uint32_t FindUartTxLocation(GPIO_Port_TypeDef port, unsigned int pin);
static uint32_t FindUartRxLocation(GPIO_Port_TypeDef port, unsigned int pin);

void
ZAF_UART0_init(GPIO_Port_TypeDef txPort,
               unsigned int txPin,
               GPIO_Port_TypeDef rxPort,
               unsigned int rxPin)
{
  static USART_InitAsync_TypeDef uartInit = USART_INITASYNC_DEFAULT;
  uartInit.enable = usartDisable;

  //UARTs clock must be enabled to configure UART
  CMU_ClockEnable(cmuClock_USART0, true);

  USART_InitAsync(USART0, &uartInit);
  UartSetTxOutput(USART0, txPort, txPin);
  UartSetRxInput(USART0, rxPort, rxPin);
  //disable UARTs clock
  CMU_ClockEnable(cmuClock_USART0, false);
}


void
ZAF_UART1_init(GPIO_Port_TypeDef txPort,
               unsigned int txPin,
               GPIO_Port_TypeDef rxPort,
               unsigned int rxPin)
{
  static USART_InitAsync_TypeDef uartInit = USART_INITASYNC_DEFAULT;
  uartInit.enable = usartDisable;

  //UARTs clock must be enabled to configure UART
  CMU_ClockEnable(cmuClock_USART1, true);

  USART_InitAsync(USART1, &uartInit);
  UartSetTxOutput(USART1, txPort, txPin);
  UartSetRxInput(USART1, rxPort, rxPin);
  //disable UARTs clock
  CMU_ClockEnable(cmuClock_USART1, false);
}


void
ZAF_UART0_enable(
  uint32_t baudRate,   /* IN  Baud rate (e.g. 9600=>9600baud/s, 115200=>115200baud/s) */
  bool     bEnableTx,  /* IN  true: Tx enabled, false: Tx disabled */
  bool     bEnableRx)  /* IN  true: Rx enabled, false: Rx disabled */
{
  /* enable cmu clock for USART0 */
  CMU_ClockEnable(cmuClock_USART0, true);
  UartEnable(USART0, baudRate, bEnableTx, bEnableRx);
}


void
ZAF_UART1_enable(
  uint32_t baudRate,  /* IN  Baud rate (e.g. 9600=>9600baud/s, 115200=>115200baud/s) */
  bool     bEnableTx,  /* IN  true: Tx enabled, false: Tx disabled */
  bool     bEnableRx)  /* IN  true: Rx enabled, false: Rx disabled */
{
  /* enable cmu clock for USART1 */
  CMU_ClockEnable(cmuClock_USART1, true);
  UartEnable(USART1, baudRate, bEnableTx, bEnableRx);
}


void
ZAF_UART0_tx_send(
  const uint8_t * pData, uint32_t iLength)
{
  while (iLength > 0)
  {
    USART_Tx(USART0, *pData++);
    iLength--;
  }
}


void
ZAF_UART1_tx_send(
  const uint8_t * pData, uint32_t iLength)
{
  while (iLength > 0)
  {
    USART_Tx(USART1, *pData++);
    iLength--;
  }
}

static void UartEnable(
  USART_TypeDef * uart,
  uint32_t baudRate,  /* IN  Baud rate (e.g. 9600=>9600baud/s, 115200=>115200baud/s) */
  bool     bEnableTx,  /* IN  true: Tx enabled, false: Tx disabled */
  bool     bEnableRx)  /* IN  true: Rx enabled, false: Rx disabled */
{
  USART_BaudrateAsyncSet(uart, 0, baudRate, usartOVS16);

  //disable rx and tx
  uart->ROUTEPEN &= ~_USART_ROUTEPEN_MASK;

  if (bEnableTx)
  {
  //enable tx
    uart->ROUTEPEN |= USART_ROUTEPEN_TXPEN;
    USART_Enable(uart, usartEnableTx);
  }

  if (bEnableRx)
  {
  //enable rx
    uart->ROUTEPEN |= USART_ROUTEPEN_RXPEN;
    USART_Enable(uart, usartEnableRx);
  }

  if(bEnableTx && bEnableRx)
  {
    USART_Enable(uart, usartEnable);
  }
}

static void UartSetTxOutput(USART_TypeDef * usart,
                            GPIO_Port_TypeDef port,
                            unsigned int pin)
{
  GPIO_PinModeSet(port, pin, gpioModePushPull, 1);
  usart->ROUTELOC0 &= ~_USART_ROUTELOC0_TXLOC_MASK;
  usart->ROUTELOC0 |= FindUartTxLocation(port, pin);
}

static void UartSetRxInput(USART_TypeDef * usart,
                           GPIO_Port_TypeDef port,
                           unsigned int pin)
{
  GPIO_PinModeSet(port, pin, gpioModeInput, 0);
  usart->ROUTELOC0 &= ~_USART_ROUTELOC0_RXLOC_MASK;
  usart->ROUTELOC0 |= FindUartRxLocation(port, pin);
}

static uint32_t FindUartTxLocation(GPIO_Port_TypeDef port,
                                   unsigned int pin)
{
  switch(port)
  {
    case gpioPortA:
    {
      if(0 == pin)
      {
        return USART_ROUTELOC0_TXLOC_LOC0;
      }
      else if(1 == pin)
      {
        return USART_ROUTELOC0_TXLOC_LOC1;
      }
      else if(2 == pin)
      {
        return USART_ROUTELOC0_TXLOC_LOC2;
      }
      else if(3 == pin)
      {
        return USART_ROUTELOC0_TXLOC_LOC3;
      }
      else if(4 == pin)
      {
        return USART_ROUTELOC0_TXLOC_LOC4;
      }
      else if(5 == pin)
      {
        return USART_ROUTELOC0_TXLOC_LOC5;
      }
      break;
    }
    case gpioPortB:
    {
      if(11 == pin)
      {
        return USART_ROUTELOC0_TXLOC_LOC6;
      }
      else if(12 == pin)
      {
        return USART_ROUTELOC0_TXLOC_LOC7;
      }
      else if(13 == pin)
      {
        return USART_ROUTELOC0_TXLOC_LOC8;
      }
      else if(14 == pin)
      {
        return USART_ROUTELOC0_TXLOC_LOC9;
      }
      else if(15 == pin)
      {
        return USART_ROUTELOC0_TXLOC_LOC10;
      }
      break;
    }
    case gpioPortC:
    {
      if(6 == pin)
      {
        return USART_ROUTELOC0_TXLOC_LOC11;
      }
      else if(7 == pin)
      {
        return USART_ROUTELOC0_TXLOC_LOC12;
      }
      else if(8 == pin)
      {
        return USART_ROUTELOC0_TXLOC_LOC13;
      }
      else if(9 == pin)
      {
        return USART_ROUTELOC0_TXLOC_LOC14;
      }
      else if(10 == pin)
      {
        return USART_ROUTELOC0_TXLOC_LOC15;
      }
      else if(11 == pin)
      {
        return USART_ROUTELOC0_TXLOC_LOC16;
      }
      break;
    }
    case gpioPortD:
    {
      if(13 == pin)
      {
        return USART_ROUTELOC0_TXLOC_LOC21;
      }
      else if(14 == pin)
      {
        return USART_ROUTELOC0_TXLOC_LOC22;
      }
      else if(15 == pin)
      {
        return USART_ROUTELOC0_TXLOC_LOC23;
      }
      break;
    }
    case gpioPortF:
    {
      if(0 == pin)
      {
        return USART_ROUTELOC0_TXLOC_LOC24;
      }
      else if(1 == pin)
      {
        return USART_ROUTELOC0_TXLOC_LOC25;
      }
      else if(2 == pin)
      {
        return USART_ROUTELOC0_TXLOC_LOC26;
      }
      else if(3 == pin)
      {
        return USART_ROUTELOC0_TXLOC_LOC27;
      }
      else if(4 == pin)
      {
        return USART_ROUTELOC0_TXLOC_LOC28;
      }
      else if(5 == pin)
      {
        return USART_ROUTELOC0_TXLOC_LOC29;
      }
      break;
    }
  }

  return USART_ROUTELOC0_TXLOC_LOC0;
}

static uint32_t FindUartRxLocation(GPIO_Port_TypeDef port,
                                   unsigned int pin)
{
  switch(port)
  {
    case gpioPortA:
    {
      if(0 == pin)
      {
        return USART_ROUTELOC0_RXLOC_LOC31;
      }
      else if(1 == pin)
      {
        return USART_ROUTELOC0_RXLOC_LOC0;
      }
      else if(2 == pin)
      {
        return USART_ROUTELOC0_RXLOC_LOC1;
      }
      else if(3 == pin)
      {
        return USART_ROUTELOC0_RXLOC_LOC2;
      }
      else if(4 == pin)
      {
        return USART_ROUTELOC0_RXLOC_LOC3;
      }
      else if(5 == pin)
      {
        return USART_ROUTELOC0_RXLOC_LOC4;
      }
      break;
    }
    case gpioPortB:
    {
      if(11 == pin)
      {
        return USART_ROUTELOC0_RXLOC_LOC5;
      }
      else if(12 == pin)
      {
        return USART_ROUTELOC0_RXLOC_LOC6;
      }
      else if(13 == pin)
      {
        return USART_ROUTELOC0_RXLOC_LOC7;
      }
      else if(14 == pin)
      {
        return USART_ROUTELOC0_RXLOC_LOC8;
      }
      else if(15 == pin)
      {
        return USART_ROUTELOC0_RXLOC_LOC9;
      }
      break;
    }
    case gpioPortC:
    {
      if(6 == pin)
      {
        return USART_ROUTELOC0_RXLOC_LOC10;
      }
      else if(7 == pin)
      {
        return USART_ROUTELOC0_RXLOC_LOC11;
      }
      else if(8 == pin)
      {
        return USART_ROUTELOC0_RXLOC_LOC12;
      }
      else if(9 == pin)
      {
        return USART_ROUTELOC0_RXLOC_LOC13;
      }
      else if(10 == pin)
      {
        return USART_ROUTELOC0_RXLOC_LOC14;
      }
      else if(11 == pin)
      {
        return USART_ROUTELOC0_RXLOC_LOC15;
      }
      break;
    }
    case gpioPortD:
    {
      if(13 == pin)
      {
        return USART_ROUTELOC0_RXLOC_LOC20;
      }
      else if(14 == pin)
      {
        return USART_ROUTELOC0_RXLOC_LOC21;
      }
      else if(15 == pin)
      {
        return USART_ROUTELOC0_RXLOC_LOC22;
      }
      break;
    }
    case gpioPortF:
    {
      if(0 == pin)
      {
        return USART_ROUTELOC0_RXLOC_LOC23;
      }
      else if(1 == pin)
      {
        return USART_ROUTELOC0_RXLOC_LOC24;
      }
      else if(2 == pin)
      {
        return USART_ROUTELOC0_RXLOC_LOC25;
      }
      else if(3 == pin)
      {
        return USART_ROUTELOC0_RXLOC_LOC26;
      }
      else if(4 == pin)
      {
        return USART_ROUTELOC0_RXLOC_LOC27;
      }
      else if(5 == pin)
      {
        return USART_ROUTELOC0_RXLOC_LOC28;
      }
      break;
    }
  }

  return USART_ROUTELOC0_RXLOC_LOC0;
}
