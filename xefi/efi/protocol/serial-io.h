#pragma once

#include <efi/types.h>
#include <efi/protocol/file.h>

#define EFI_SERIAL_IO_PROTOCOL_GUID \
    {0xbb25cf6f, 0xf1d4, 0x11d2, {0x9a, 0x0c, 0x00, 0x90, 0x27, 0x3f, 0xc1, 0xfd}}
extern efi_guid SerialIoProtocol;

#define EFI_SERIAL_IO_PROTOCOL_REVISION 0x00010000

typedef struct efi_serial_io_mode {
  uint32_t ControlMask;
  // current Attributes
  uint32_t Timeout;
  uint64_t BaudRate;
  uint32_t ReceiveFifoDepth;
  uint32_t DataBits;
  uint32_t Parity;
  uint32_t StopBits;
} efi_serial_io_mode;

// Control bits
#define EFI_SERIAL_CLEAR_TO_SEND                0x0010
#define EFI_SERIAL_DATA_SET_READY               0x0020
#define EFI_SERIAL_RING_INDICATE                0x0040
#define EFI_SERIAL_CARRIER_DETECT               0x0080
#define EFI_SERIAL_REQUEST_TO_SEND              0x0002
#define EFI_SERIAL_DATA_TERMINAL_READY          0x0001
#define EFI_SERIAL_INPUT_BUFFER_EMPTY           0x0100
#define EFI_SERIAL_OUTPUT_BUFFER_EMPTY          0x0200
#define EFI_SERIAL_HARDWARE_LOOPBACK_ENABLE     0x1000
#define EFI_SERIAL_SOFTWARE_LOOPBACK_ENABLE     0x2000
#define EFI_SERIAL_HARDWARE_FLOW_CONTROL_ENABLE 0x4000

typedef enum {
  DefaultParity,
  NoParity,
  EvenParity,
  OddParity,
  MarkParity,
  SpaceParity
} efi_serial_io_parity_type;

typedef enum {
  DefaultStopBits,
  OneStopBit,      // 1 stop bit
  OneFiveStopBits, // 1.5 stop bits
  TwoStopBits      // 2 stop bits
} efi_serial_io_stop_bits_type; 

typedef struct efi_serial_io_protocol {
    uint64_t Revision;

    efi_status (*Reset) (struct efi_serial_io_protocol* self) EFIAPI;
    efi_status (*SetAttributes) (struct efi_serial_io_protocol* self,
        uint64_t baud_rate, uint32_t receive_fifo_depth, uint32_t timeout,
        efi_serial_io_parity_type parity, uint8_t data_bits, efi_serial_io_stop_bits_type stop_bits)
        EFIAPI;
    efi_status (*SetControl) (struct efi_serial_io_protocol* self,
        uint32_t control) EFIAPI;
    efi_status (*GetControl) (struct efi_serial_io_protocol* self,
        uint32_t *control) EFIAPI;
    efi_status (*Write) (struct efi_serial_io_protocol* self,
        size_t *bufferSize, void *buffer) EFIAPI;
    efi_status (*Read) (struct efi_serial_io_protocol* self,
        size_t *bufferSize, void *buffer) EFIAPI;
    efi_serial_io_mode *Mode;
} efi_serial_io_protocol;
