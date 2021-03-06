#ifndef MOUFILTER_H
#define MOUFILTER_H
 
#include <ntddk.h>
#include <kbdmou.h>
#include <ntddmou.h>
#include <ntdd8042.h>
#include <wdf.h>
 
 
 
#if DBG
 
#define TRAP()                      DbgBreakPoint()
 
#define DebugPrint(_x_) DbgPrint _x_
 
#else   // DBG
 
#define TRAP()
 
#define DebugPrint(_x_)
 
#endif
 
  
typedef struct _DEVICE_EXTENSION //Характеристики устройства
{                             
    PVOID UpperContext;
      
    PI8042_MOUSE_ISR UpperIsrHook;
 
   
    IN PI8042_ISR_WRITE_PORT IsrWritePort;
 
    IN PVOID CallContext;
 
   
    IN PI8042_QUEUE_PACKET QueueMousePacket;
 
    CONNECT_DATA UpperConnectData;
  
}

DEVICE_EXTENSION, *PDEVICE_EXTENSION;
 
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_EXTENSION,
                                        FilterGetData)
  DRIVER_INITIALIZE DriverEntry;
 
EVT_WDF_DRIVER_DEVICE_ADD MouFilter_EvtDeviceAdd;
EVT_WDF_IO_QUEUE_IO_INTERNAL_DEVICE_CONTROL MouFilter_EvtIoInternalDeviceControl;
  
 
 
VOID
MouFilter_DispatchPassThrough(
     _In_ WDFREQUEST Request,
    _In_ WDFIOTARGET Target
    );
 
BOOLEAN
MouFilter_IsrHook (
    PVOID         DeviceExtension,
    PMOUSE_INPUT_DATA       CurrentInput,
    POUTPUT_PACKET          CurrentOutput,
    UCHAR                   StatusByte,
    PUCHAR                  DataByte,
    PBOOLEAN                ContinueProcessing,
    PMOUSE_STATE            MouseState,
    PMOUSE_RESET_SUBSTATE   ResetSubState
);
 
VOID
MouFilter_ServiceCallback(
    IN PDEVICE_OBJECT DeviceObject,
    IN PMOUSE_INPUT_DATA InputDataStart,
    IN PMOUSE_INPUT_DATA InputDataEnd,
    IN OUT PULONG InputDataConsumed
    );
 
#endif  // MOUFILTER_H
