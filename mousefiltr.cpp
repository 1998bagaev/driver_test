#include "moufiltr.h"
 
#ifdef ALLOC_PRAGMA
#pragma alloc_text (INIT, DriverEntry)
#pragma alloc_text (PAGE, MouFilter_EvtDeviceAdd)
#pragma alloc_text (PAGE, MouFilter_EvtIoInternalDeviceControl)
#endif
 
NTSTATUS
DriverEntry (
    IN  PDRIVER_OBJECT  DriverObject,
    IN  PUNICODE_STRING RegistryPath
    )
    
    
    {
    WDF_DRIVER_CONFIG               config;
    NTSTATUS                                status;
 
    DebugPrint(("Mouse Filter Driver Sample - Driver Framework Edition.\n"));
    DebugPrint(("Built %s %s\n", __DATE__, __TIME__));
    
    // конфигурации драйвера для управления атрибутами,являются глобальными для драйвера.
     
     
     WDF_DRIVER_CONFIG_INIT(
        &config,
        MouFilter_EvtDeviceAdd
    );
 //Создание объекта базы драйверов для представления нашего драйвера.
 
 status = WdfDriverCreate(DriverObject,
                            RegistryPath,
                            WDF_NO_OBJECT_ATTRIBUTES,
                            &config,
                            WDF_NO_HANDLE); // hDriver optional
    if (!NT_SUCCESS(status)) {
        DebugPrint( ("WdfDriverCreate failed with status 0x%x\n", status));
    }
 
    return status;
}
 
NTSTATUS
MouFilter_EvtDeviceAdd(
    IN WDFDRIVER        Driver,
    IN PWDFDEVICE_INIT  DeviceInit
    )
    
    
    {
    WDF_OBJECT_ATTRIBUTES   deviceAttributes;
    NTSTATUS                            status;
    WDFDEVICE                          hDevice;
    WDF_IO_QUEUE_CONFIG        ioQueueConfig;
     
    UNREFERENCED_PARAMETER(Driver);
 
    PAGED_CODE();
 
    DebugPrint(("Enter FilterEvtDeviceAdd \n"));
    
    WdfFdoInitSetFilter(DeviceInit);
 
    WdfDeviceInitSetDeviceType(DeviceInit, FILE_DEVICE_MOUSE);
 
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&deviceAttributes,
        DEVICE_EXTENSION);
        
        //создание объекта устройства
        
        status = WdfDeviceCreate(&DeviceInit, &deviceAttributes, &hDevice);
    if (!NT_SUCCESS(status)) {
        DebugPrint(("WdfDeviceCreate failed with status code 0x%x\n", status));
        return status;
    }
    
    //найстройка очереди по умолчанию(параллельлизм драйверов(многопоточность))
    
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&ioQueueConfig,
                             WdfIoQueueDispatchParallel);
 ioQueueConfig.EvtIoInternalDeviceControl = MouFilter_EvtIoInternalDeviceControl;
 
    status = WdfIoQueueCreate(hDevice,
                            &ioQueueConfig,
                            WDF_NO_OBJECT_ATTRIBUTES,
                            WDF_NO_HANDLE //указатель на очередь по умолчанию
                            );
    if (!NT_SUCCESS(status)) {
        DebugPrint( ("WdfIoQueueCreate failed 0x%x\n", status));
        return status;
    }
 
    return status;
}

 VOID
 MouFilter_DispatchPassThrough(
    _In_ WDFREQUEST Request,
    _In_ WDFIOTARGET Target
    )
    
    WDF_REQUEST_SEND_OPTIONS options;
    BOOLEAN ret;
    NTSTATUS status = STATUS_SUCCESS;
 
 WDF_REQUEST_SEND_OPTIONS_INIT(&options,
                                  WDF_REQUEST_SEND_OPTION_SEND_AND_FORGET);
 
    ret = WdfRequestSend(Request, Target, &options);
 
    if (ret == FALSE) {
        status = WdfRequestGetStatus (Request);
        DebugPrint( ("WdfRequestSend failed: 0x%x\n", status));
        WdfRequestComplete(Request, status);
    }
 
    return;
}          
 
VOID
MouFilter_EvtIoInternalDeviceControl(
    IN WDFQUEUE      Queue,
    IN WDFREQUEST    Request,
    IN size_t        OutputBufferLength,
    IN size_t        InputBufferLength,
    IN ULONG         IoControlCode
    )
    
    
    {
     
    PDEVICE_EXTENSION           devExt;
    PCONNECT_DATA               connectData;
    PINTERNAL_I8042_HOOK_MOUSE  hookMouse;
    NTSTATUS                   status = STATUS_SUCCESS;
    WDFDEVICE                 hDevice;
    size_t                           length;
 
    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);
 
    PAGED_CODE();
 
    hDevice = WdfIoQueueGetDevice(Queue);
    devExt = FilterGetData(hDevice);
 
    switch (IoControlCode) {
   
    // Подключаем мышь
      
    case IOCTL_INTERNAL_MOUSE_CONNECT:
    
        if (devExt->UpperConnectData.ClassService != NULL) {
            status = STATUS_SHARING_VIOLATION;
            break;
        }
         
        //
        // Копируем параметры соединения
        //
         status = WdfRequestRetrieveInputBuffer(Request,
                            sizeof(CONNECT_DATA),
                            &connectData,
                            &length);
        if(!NT_SUCCESS(status)){
            DebugPrint(("WdfRequestRetrieveInputBuffer failed %x\n", status));
            break;
        }
 
         
        devExt->UpperConnectData = *connectData;
        
        connectData->ClassDeviceObject = WdfDeviceWdmGetDeviceObject(hDevice);
        connectData->ClassService = MouFilter_ServiceCallback;
 
        break;
        
         case IOCTL_INTERNAL_MOUSE_DISCONNECT:
         
          status = STATUS_NOT_IMPLEMENTED;
        break;
 case IOCTL_INTERNAL_I8042_HOOK_MOUSE:  
 
          DebugPrint(("hook mouse received!\n"));
         
      
        status = WdfRequestRetrieveInputBuffer(Request,
                            sizeof(INTERNAL_I8042_HOOK_MOUSE),
                            &hookMouse,
                            &length);
        if(!NT_SUCCESS(status)){
            DebugPrint(("WdfRequestRetrieveInputBuffer failed %x\n", status));
            break;
        }
       
       
        devExt->UpperContext = hookMouse->Context;
        hookMouse->Context = (PVOID) devExt;
 
        if (hookMouse->IsrRoutine) {
            devExt->UpperIsrHook = hookMouse->IsrRoutine;
        }
        hookMouse->IsrRoutine = (PI8042_MOUSE_ISR) MouFilter_IsrHook;
 
       
        devExt->IsrWritePort = hookMouse->IsrWritePort;
        devExt->CallContext = hookMouse->CallContext;
        devExt->QueueMousePacket = hookMouse->QueueMousePacket;
 
        status = STATUS_SUCCESS;
        break;
        
        case IOCTL_MOUSE_QUERY_ATTRIBUTES:
    default:
        break;
    }
 
    if (!NT_SUCCESS(status)) {
        WdfRequestComplete(Request, status);
        return ;
    }
 
    MouFilter_DispatchPassThrough(Request,WdfDeviceGetIoTarget(hDevice));
}
 
 
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
)

{
    PDEVICE_EXTENSION   devExt;
    BOOLEAN             retVal = TRUE;
 
    devExt = DeviceExtension;
     
    if (devExt->UpperIsrHook) {
        retVal = (*devExt->UpperIsrHook) (devExt->UpperContext,
                            CurrentInput,
                            CurrentOutput,
                            StatusByte,
                            DataByte,
                            ContinueProcessing,
                            MouseState,
                            ResetSubState
            );
 
        if (!retVal || !(*ContinueProcessing)) {
            return retVal;
        }
    }
 
    *ContinueProcessing = TRUE;
    return retVal;
}
 
     
 
VOID
MouFilter_ServiceCallback(
    IN PDEVICE_OBJECT DeviceObject,
    IN PMOUSE_INPUT_DATA InputDataStart,
    IN PMOUSE_INPUT_DATA InputDataEnd,
    IN OUT PULONG InputDataConsumed
    )
    
    {
    PDEVICE_EXTENSION   devExt;
    WDFDEVICE   hDevice;
 
    hDevice = WdfWdmDeviceGetWdfDeviceHandle(DeviceObject);
 
    devExt = FilterGetData(hDevice);
    
    (*(PSERVICE_CALLBACK_ROUTINE) devExt->UpperConnectData.ClassService)(
        devExt->UpperConnectData.ClassDeviceObject,
        InputDataStart,
        InputDataEnd,
        InputDataConsumed
        );
}
 
#pragma warning(pop)
 
