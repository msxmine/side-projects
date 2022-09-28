#include <string.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/uart.h"
#include "hardware/timer.h"
#include "pico/time.h"

const int PIN_BUS_BUSY = 11;
const int PIN_RS_TEN = 7;
const int PIN_ESP_BOOTED = 22;

const int PIN_UART0_TX = 12;
const int PIN_UART0_RX = 13;
const int PIN_UART1_TX = 8;
const int PIN_UART1_RX = 9;

const uint8_t rsAddr = 0x02;
uint8_t idleResponseTemplate[] = {0xfe, 0x00, 0xc4, 0x00, 0x00, 0x7c, 0x00, 0x00};
uint8_t idleResponse[(sizeof(idleResponseTemplate)*2)+6];
size_t idleSize = 0;

uint8_t recvBuf[1024];
uint8_t proxyBuf[1024];

bool forceIdle = true;

uint64_t lastMsg = -10000ull;

const uint8_t preamble[] = {0xff, 0xff, 0x68};
const uint8_t specialSymbols[] = {0x16, 0x68, 0x80};

uint16_t mbCRC(const uint8_t* msg, size_t len){
  uint16_t crc = 0xffff;

  for (size_t pos = 0; pos < len; pos++){
    crc ^= (uint16_t)msg[pos];
    
    for (int i = 0; i < 8; i++){
      if ((crc & 0x0001) != 0) {
        crc >>= 1;
        crc ^= 0xA001;
      }
      else{
        crc >>= 1;
      }
    }
  }

  return crc;
}

size_t espRecv(){
    size_t pos = 0;
    bool incoming = false;

    while(true){
        incoming = uart_is_readable(uart0);
        if (incoming == false || pos > 250){
            return pos;
        }
        else {
            uart_read_blocking(uart0, proxyBuf+pos, 1);
            pos++;
        }
    }
}

void uart_fifo_clear(uart_inst_t* uart, uint32_t us){
    uint8_t trash[2];
    while(uart_is_readable_within_us(uart, us)){
        uart_read_blocking(uart, trash, 1);
    }
}

int rsRecv(){
  int pos = 0;
  int prematch = 0;

  bool incoming = false;
  uint8_t data[3];
  bool specialByte = false;
  
  if (!uart_is_readable(uart1)){
      sleep_us(600);
      return 0; //No data on FIFO
  }
  
  while (true){
    incoming = uart_is_readable_within_us(uart1, 400);
    if (incoming == false){
      return 0; //Timed out
    }
    else{
      uart_read_blocking(uart1, data, 1);
      if (prematch < sizeof(preamble)){
        if (data[0] == preamble[prematch]){
          prematch++;
        }
        else{
          prematch = 0;
        }
      }
      else{
        if (pos > 250){
          return -1;
        }
        else{
          if (data[0] == 0x16){
            break;
          }
          else{
            if (data[0] == 0x80){
              specialByte = true;
            }
            else{
              if (specialByte){
                recvBuf[pos] = data[0] ^ 0xff;
                specialByte = false;
              }
              else{
                recvBuf[pos] = data[0];
              }
              pos++;
            }
          }
        }
      }
    }
  }

  if (pos < 5){
    return -2; //Corrupted too short
  }

  unsigned int packCRC = mbCRC(recvBuf, pos-2);

  if ( (uint8_t)(packCRC & 0x00ff) != recvBuf[pos-2] || (uint8_t)((packCRC >> 8) & 0x00ff) != recvBuf[pos-1]){
    return -2; //Corrupted CRC mismatch
  }

  if (recvBuf[0] != rsAddr || recvBuf[1] != 0xfe){
    return -3; //Not for this node
  }

  return pos;
}

void rsSendRaw(const uint8_t* data, size_t len){
    uart_set_format(uart1, 8, 2, UART_PARITY_NONE);
    gpio_put(PIN_RS_TEN, true);
    uart_write_blocking(uart1, data, len);
    uart_tx_wait_blocking(uart1);
    gpio_put(PIN_RS_TEN, false);
    uart_set_format(uart1, 8, 1, UART_PARITY_NONE);
}

int rsFrame(uint8_t* buf, size_t bufsize, uint8_t* data, size_t datalen){
  if (datalen*2 + sizeof(preamble) + 2 > bufsize){
    return -1;
  }
  
  memcpy(buf, preamble, sizeof(preamble));
  int tBufIdx = sizeof(preamble);

  for (int i = 0; i < datalen; i++){
    int specialIdx = -1;
    for (int j = 0; j < sizeof(specialSymbols); j++){
      if (data[i] == specialSymbols[j]){
        specialIdx = j;
        break;
      }
    }
    if (specialIdx >= 0){
      buf[tBufIdx++] = 0x80;
      buf[tBufIdx++] = specialSymbols[specialIdx] ^ 0xff;
    }
    else {
      buf[tBufIdx++] = data[i];
    }
  }

  buf[tBufIdx++] = 0x16;

  return tBufIdx;

}

int main() {
    gpio_set_pulls(PIN_UART0_RX, true, false);
    gpio_set_pulls(PIN_UART0_TX, true, false);
    gpio_set_pulls(PIN_UART1_RX, true, false);
    gpio_set_pulls(PIN_UART1_TX, true, false);
    uart_init(uart0, 115200);
    uart_init(uart1, 115200);
    gpio_set_function(PIN_UART0_TX, GPIO_FUNC_UART);
    gpio_set_function(PIN_UART0_RX, GPIO_FUNC_UART);
    gpio_set_function(PIN_UART1_TX, GPIO_FUNC_UART);
    gpio_set_function(PIN_UART1_RX, GPIO_FUNC_UART);

    uart_set_format(uart0, 8, 1, UART_PARITY_NONE);
    uart_set_format(uart1, 8, 1, UART_PARITY_NONE);
    uart_set_hw_flow(uart0, false, false);
    uart_set_hw_flow(uart1, false, false);
    uart_set_fifo_enabled(uart0, true);
    uart_set_fifo_enabled(uart1, true);

    gpio_init(PIN_ESP_BOOTED);
    gpio_init(PIN_BUS_BUSY);
    gpio_init(PIN_RS_TEN);
    gpio_set_oeover(PIN_BUS_BUSY, GPIO_OVERRIDE_LOW);
    gpio_set_dir(PIN_ESP_BOOTED, GPIO_IN);
    gpio_set_dir(PIN_BUS_BUSY, GPIO_OUT);
    gpio_set_dir(PIN_RS_TEN, GPIO_OUT);
    gpio_set_input_enabled(PIN_BUS_BUSY, false);
    gpio_set_pulls(PIN_ESP_BOOTED, true, false);
    gpio_set_pulls(PIN_BUS_BUSY, false, false);
    gpio_put(PIN_RS_TEN, false);
    gpio_put(PIN_BUS_BUSY, false);

    idleResponseTemplate[1] = rsAddr;
    uint16_t idleCRC = mbCRC(idleResponseTemplate, sizeof(idleResponseTemplate)-2);
    idleResponseTemplate[sizeof(idleResponseTemplate)-2] = (uint8_t)(idleCRC & 0x00ff);
    idleResponseTemplate[sizeof(idleResponseTemplate)-1] = (uint8_t)((idleCRC >> 8) & 0x00ff);
    idleSize = rsFrame(idleResponse, sizeof(idleResponse), idleResponseTemplate, sizeof(idleResponseTemplate));

    while(true){
        int recvbytes = rsRecv();
        if (recvbytes > 0){
            bool espBooted = !gpio_get(PIN_ESP_BOOTED);
            int readyEsp = 0;
            if (espBooted){
                readyEsp = espRecv();
            }
            bool packetGood = false;
            if (readyEsp > 6 && readyEsp < 200){
                if (proxyBuf[0] == 0xff && proxyBuf[1] == 0xff && proxyBuf[2] == 0x68 && proxyBuf[readyEsp-1] == 0x16){
                    packetGood = true;
                    for (int termidx = 0; termidx < readyEsp-1; termidx++){
                        if (proxyBuf[termidx] == 0x16){
                            packetGood = false;
                            break;
                        }
                    }
                }
            }
            if (packetGood == true && forceIdle == false){
                rsSendRaw(proxyBuf, readyEsp);
                forceIdle = true;
            }
            else {
                rsSendRaw(idleResponse, idleSize);
                forceIdle = false;
            }

            uart_fifo_clear(uart0, 400);
            uart_fifo_clear(uart1, 400);

            lastMsg = time_us_64();

            if (forceIdle == false){
                gpio_set_oeover(PIN_BUS_BUSY, GPIO_OVERRIDE_NORMAL);
            }
            else {
                gpio_set_oeover(PIN_BUS_BUSY, GPIO_OVERRIDE_LOW);
            }

            if (espBooted){
                int enclen = rsFrame(proxyBuf, sizeof(proxyBuf), recvBuf, recvbytes);
                uart_write_blocking(uart0, proxyBuf, enclen);
                uart_tx_wait_blocking(uart0);
            }
        }

        uint64_t now = time_us_64();
        if ((now - lastMsg) > 5000000ull){
            gpio_set_oeover(PIN_BUS_BUSY, GPIO_OVERRIDE_LOW);
            forceIdle = true;
            lastMsg = now - 6000000ull;
        }
    }


}
