/*++

Module Name:

    device.c - Device handling events for example driver.

Abstract:

   This file contains the device entry points and callbacks.
    
Environment:

    Kernel-mode Driver Framework

--*/

#include "driver.h"
#include "device.tmh"
#define RESHUB_USE_HELPER_ROUTINES
#include <reshub.h>
#include <gpio.h>
#include <wdf.h>
//#include "device.h"

EVT_WDF_DEVICE_PREPARE_HARDWARE LumiaUSBCDevicePrepareHardware;
EVT_UCM_CONNECTOR_SET_DATA_ROLE     LumiaUSBCSetDataRole;
//EVT_WDF_DEVICE_D0_ENTRY LumiaUSBCDeviceD0Entry;


NTSTATUS ReadRegister(PDEVICE_CONTEXT ctx, int reg, char *value, ULONG length);
NTSTATUS WriteRegister(PDEVICE_CONTEXT ctx, int reg, char *value, ULONG length);
NTSTATUS GetGPIO(PDEVICE_CONTEXT ctx, LARGE_INTEGER gpio, char *value);
NTSTATUS SetGPIO(PDEVICE_CONTEXT ctx, LARGE_INTEGER gpio, char *value);

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, LumiaUSBCKmCreateDevice)
#pragma alloc_text (PAGE, LumiaUSBCDevicePrepareHardware)
#pragma alloc_text (PAGE, LumiaUSBCSetDataRole)
//#pragma alloc_text (PAGE, LumiaUSBCDeviceD0Entry)
#endif

NTSTATUS
LumiaUSBCSetDataRole(
	UCMCONNECTOR  Connector,
	UCM_DATA_ROLE  DataRole
)
{
	PCONNECTOR_CONTEXT connCtx;

	UNREFERENCED_PARAMETER(DataRole);

	connCtx = ConnectorGetContext(Connector);

	return STATUS_SUCCESS;
}

BOOLEAN EvtInterruptIsr(
	WDFINTERRUPT Interrupt,
	ULONG MessageID
)
{
	UNREFERENCED_PARAMETER(MessageID);
	WdfInterruptQueueWorkItemForIsr(Interrupt);

	return TRUE;
}

void Uc120InterruptWorkItem(
	WDFINTERRUPT Interrupt,
	WDFOBJECT AssociatedObject
)
{
	UNREFERENCED_PARAMETER(Interrupt);
	PDEVICE_CONTEXT ctx = DeviceGetContext(AssociatedObject);
	char registers[8];
	char dismiss = 0xFF;
	wchar_t buf[260];
	ULONG data = 0;

	ReadRegister(ctx,  0, registers + 0, 1);
	ReadRegister(ctx,  1, registers + 1, 1);
	ReadRegister(ctx,  2, registers + 2, 1);
	ReadRegister(ctx,  5, registers + 3, 1);
	ReadRegister(ctx,  7, registers + 4, 1);
	ReadRegister(ctx,  9, registers + 5, 1);
	ReadRegister(ctx, 10, registers + 6, 1);
	ReadRegister(ctx, 11, registers + 7, 1);

	WriteRegister(ctx, 2, &dismiss, 1);

	swprintf(buf, L"UC120_%02x%02x%02x%02x%02x%02x%02x%02x", registers[0], registers[1], registers[2], registers[3], registers[4], registers[5], registers[6], registers[7]);

	RtlWriteRegistryValue(RTL_REGISTRY_ABSOLUTE,
		(PCWSTR)L"\\Registry\\Machine\\System\\usbc",
		(PCWSTR)buf,
		REG_DWORD,
		&data,
		sizeof(ULONG));
}

void PlugDetInterruptWorkItem(
	WDFINTERRUPT Interrupt,
	WDFOBJECT AssociatedObject
)
{
	UNREFERENCED_PARAMETER(Interrupt);
	PDEVICE_CONTEXT ctx = DeviceGetContext(AssociatedObject);
	char registers[8];
//	char dismiss = 0x1;
	wchar_t buf[260];
	ULONG data = 0;

	ReadRegister(ctx, 0, registers + 0, 1);
	ReadRegister(ctx, 1, registers + 1, 1);
	ReadRegister(ctx, 2, registers + 2, 1);
	ReadRegister(ctx, 5, registers + 3, 1);
	ReadRegister(ctx, 7, registers + 4, 1);
	ReadRegister(ctx, 9, registers + 5, 1);
	ReadRegister(ctx, 10, registers + 6, 1);
	ReadRegister(ctx, 11, registers + 7, 1);

	//WriteRegister(ctx, 2, &dismiss, 1);

	swprintf(buf, L"PLUGDET_%02x%02x%02x%02x%02x%02x%02x%02x", registers[0], registers[1], registers[2], registers[3], registers[4], registers[5], registers[6], registers[7]);

	RtlWriteRegistryValue(RTL_REGISTRY_ABSOLUTE,
		(PCWSTR)L"\\Registry\\Machine\\System\\usbc",
		(PCWSTR)buf,
		REG_DWORD,
		&data,
		sizeof(ULONG));
}

NTSTATUS
LumiaUSBCProbeResources(
	PDEVICE_CONTEXT ctx,
	WDFCMRESLIST res
)
{
	NTSTATUS status = STATUS_SUCCESS;
	PCM_PARTIAL_RESOURCE_DESCRIPTOR  desc;
	WDF_INTERRUPT_CONFIG Config;
	int k = 0, l = 0;
	int spi_found = 0;
	for (unsigned int i = 0; i < WdfCmResourceListGetCount(res); i++) {
		desc = WdfCmResourceListGetDescriptor(res, i);
		switch (desc->Type) {

		case CmResourceTypeConnection:
			//
			// Handle memory resources here.
			//
			if (desc->u.Connection.Class == CM_RESOURCE_CONNECTION_CLASS_GPIO && desc->u.Connection.Type == CM_RESOURCE_CONNECTION_TYPE_GPIO_IO) {
				switch (k) {
				case 0:
					ctx->VbusGpioId.LowPart = desc->u.Connection.IdLowPart;
					ctx->VbusGpioId.HighPart = desc->u.Connection.IdHighPart;
					break;
				case 1:
					ctx->PolGpioId.LowPart = desc->u.Connection.IdLowPart;
					ctx->PolGpioId.HighPart = desc->u.Connection.IdHighPart;
					break;
				case 2:
					ctx->AmselGpioId.LowPart = desc->u.Connection.IdLowPart;
					ctx->AmselGpioId.HighPart = desc->u.Connection.IdHighPart;
					break;
				case 3:
					ctx->EnGpioId.LowPart = desc->u.Connection.IdLowPart;
					ctx->EnGpioId.HighPart = desc->u.Connection.IdHighPart;
					break;
				default:
					break;
				}

				k++;
			}

			if (desc->u.Connection.Class == CM_RESOURCE_CONNECTION_CLASS_SERIAL && desc->u.Connection.Type == CM_RESOURCE_CONNECTION_TYPE_SERIAL_SPI) {
				ctx->SpiId.LowPart = desc->u.Connection.IdLowPart;
				ctx->SpiId.HighPart = desc->u.Connection.IdHighPart;
				spi_found = 1;
			}
			break;

		case CmResourceTypeInterrupt:
			switch (l)
			{
			case 0:
				WDF_INTERRUPT_CONFIG_INIT(&Config, EvtInterruptIsr, NULL);
				Config.EvtInterruptWorkItem = PlugDetInterruptWorkItem;
				Config.InterruptTranslated = desc;
				WdfInterruptCreate(ctx->Device, &Config, WDF_NO_OBJECT_ATTRIBUTES, &ctx->PlugDetectInterrupt);
				break;
			case 1:
				WDF_INTERRUPT_CONFIG_INIT(&Config, EvtInterruptIsr, NULL);
				Config.EvtInterruptWorkItem = PlugDetInterruptWorkItem;
				Config.InterruptTranslated = desc;
				WdfInterruptCreate(ctx->Device, &Config, WDF_NO_OBJECT_ATTRIBUTES, &ctx->Uc120Interrupt);
				break;
			default:
				break;
			}
			l++;
			break;
		default:
			//
			// Ignore all other descriptors.
			//
			break;
		}
	}

	if (!spi_found || k < 4) {
		status = 0xC0000000 + 4 * spi_found + k;
	}

	return status;
}

NTSTATUS OpenIOTarget(PDEVICE_CONTEXT ctx, LARGE_INTEGER res, ACCESS_MASK use, WDFIOTARGET *target)
{
	NTSTATUS status = STATUS_SUCCESS;
	WDF_OBJECT_ATTRIBUTES ObjectAttributes;
	WDF_IO_TARGET_OPEN_PARAMS OpenParams;
	UNICODE_STRING ReadString;
	WCHAR ReadStringBuffer[260];

	RtlInitEmptyUnicodeString(&ReadString,
		ReadStringBuffer,
		sizeof(ReadStringBuffer));

	status = RESOURCE_HUB_CREATE_PATH_FROM_ID(&ReadString,
		res.LowPart,
		res.HighPart);
	if (!NT_SUCCESS(status))
		return status;

	WDF_OBJECT_ATTRIBUTES_INIT(&ObjectAttributes);
	ObjectAttributes.ParentObject = ctx->Device;

	status = WdfIoTargetCreate(ctx->Device, &ObjectAttributes, target);
	if (!NT_SUCCESS(status)) {
		return status;
	}

	WDF_IO_TARGET_OPEN_PARAMS_INIT_OPEN_BY_NAME(&OpenParams, &ReadString, use);
	status = WdfIoTargetOpen(*target, &OpenParams);
	
	return status;
}

NTSTATUS ReadRegister(PDEVICE_CONTEXT ctx, int reg, char *value, ULONG length)
{
	NTSTATUS status = STATUS_SUCCESS;
	WDFIOTARGET IoTarget;
	WDF_MEMORY_DESCRIPTOR regDescriptor, outputDescriptor;
	char command = (char)(reg << 3);
	
	status = OpenIOTarget(ctx, ctx->SpiId, GENERIC_READ, &IoTarget);
	if (!NT_SUCCESS(status))
		return status;

	WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&regDescriptor, &command, 1);
	WdfIoTargetSendWriteSynchronously(IoTarget, NULL, &regDescriptor, NULL, NULL, NULL);


	WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&outputDescriptor, value, length);
	WdfIoTargetSendReadSynchronously(IoTarget, NULL, &outputDescriptor, NULL, NULL, NULL);

	return status;
}

NTSTATUS WriteRegister(PDEVICE_CONTEXT ctx, int reg, char *value, ULONG length)
{
	NTSTATUS status = STATUS_SUCCESS;
	WDFIOTARGET IoTarget;
	WDF_MEMORY_DESCRIPTOR regDescriptor, inputDescriptor;
	char command = (char)((reg << 3) | 1);

	status = OpenIOTarget(ctx, ctx->SpiId, GENERIC_WRITE, &IoTarget);
	if (!NT_SUCCESS(status))
		return status;

	WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&regDescriptor, &command, 1);
	WdfIoTargetSendWriteSynchronously(IoTarget, NULL, &regDescriptor, NULL, NULL, NULL);

	WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&inputDescriptor, value, length);
	WdfIoTargetSendWriteSynchronously(IoTarget, NULL, &inputDescriptor, NULL, NULL, NULL);

	return status;
}

NTSTATUS GetGPIO(PDEVICE_CONTEXT ctx, LARGE_INTEGER gpio, char *value)
{
	NTSTATUS status = STATUS_SUCCESS;
	WDFIOTARGET IoTarget;
	WDF_MEMORY_DESCRIPTOR outputDescriptor;
	
	status = OpenIOTarget(ctx, gpio, GENERIC_READ, &IoTarget);
	if (!NT_SUCCESS(status))
		return status;

	WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&outputDescriptor, value, 1);

	status = WdfIoTargetSendIoctlSynchronously(IoTarget, NULL, IOCTL_GPIO_READ_PINS, NULL, &outputDescriptor, NULL, NULL);

	return status;
}

NTSTATUS SetGPIO(PDEVICE_CONTEXT ctx, LARGE_INTEGER gpio, char *value)
{
	NTSTATUS status = STATUS_SUCCESS;
	WDFIOTARGET IoTarget;
	WDF_MEMORY_DESCRIPTOR inputDescriptor, outputDescriptor;

	status = OpenIOTarget(ctx, gpio, GENERIC_WRITE, &IoTarget);
	if (!NT_SUCCESS(status))
		return status;

	WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&inputDescriptor, value, 1);
	WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&outputDescriptor, value, 1);

	status = WdfIoTargetSendIoctlSynchronously(IoTarget, NULL, IOCTL_GPIO_WRITE_PINS, &inputDescriptor, &outputDescriptor, NULL, NULL);

	return status;
}

NTSTATUS
LumiaUSBCDevicePrepareHardware(
	WDFDEVICE Device,
	WDFCMRESLIST ResourcesRaw,
	WDFCMRESLIST ResourcesTranslated
)
{
	NTSTATUS status = STATUS_SUCCESS;
	PDEVICE_CONTEXT devCtx;
	UCM_MANAGER_CONFIG ucmCfg;
	UCM_CONNECTOR_CONFIG connCfg;
	UCM_CONNECTOR_TYPEC_CONFIG typeCConfig;
	UCM_CONNECTOR_PD_CONFIG pdConfig;
	WDF_OBJECT_ATTRIBUTES attr;
	PCONNECTOR_CONTEXT connCtx;

	UNREFERENCED_PARAMETER(ResourcesRaw);
	UNREFERENCED_PARAMETER(ResourcesTranslated);


	devCtx = DeviceGetContext(Device);

	status = LumiaUSBCProbeResources(devCtx, ResourcesTranslated);
	if (!NT_SUCCESS(status))
		return status;

	if (devCtx->Connector)
	{
		goto Exit;
	}

	//
	// Initialize UCM Manager
	//
	UCM_MANAGER_CONFIG_INIT(&ucmCfg);

	status = UcmInitializeDevice(Device, &ucmCfg);
	if (!NT_SUCCESS(status))
	{
		goto Exit;
	}

	//
	// Create a USB Type-C connector #0 with PD
	//
	UCM_CONNECTOR_CONFIG_INIT(&connCfg, 0);

	UCM_CONNECTOR_TYPEC_CONFIG_INIT(
		&typeCConfig,
		UcmTypeCOperatingModeDrp,
		UcmTypeCCurrentDefaultUsb | UcmTypeCCurrent1500mA | UcmTypeCCurrent3000mA);

	typeCConfig.EvtSetDataRole = LumiaUSBCSetDataRole;

	UCM_CONNECTOR_PD_CONFIG_INIT(&pdConfig, UcmPowerRoleSink | UcmPowerRoleSource);

	connCfg.TypeCConfig = &typeCConfig;
	connCfg.PdConfig = &pdConfig;

	WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attr, CONNECTOR_CONTEXT);

	status = UcmConnectorCreate(Device, &connCfg, &attr, &devCtx->Connector);
	if (!NT_SUCCESS(status))
	{
		goto Exit;
	}

	connCtx = ConnectorGetContext(devCtx->Connector);

	UCM_CONNECTOR_TYPEC_ATTACH_PARAMS Params;
	UCM_CONNECTOR_TYPEC_ATTACH_PARAMS_INIT(&Params, UcmTypeCPartnerUfp);
	Params.CurrentAdvertisement = UcmTypeCCurrentDefaultUsb;
	UcmConnectorTypeCAttach(devCtx->Connector, &Params);

	UCM_PD_POWER_DATA_OBJECT Pdos[1];
	UCM_PD_POWER_DATA_OBJECT_INIT_FIXED(&Pdos[0]);

	Pdos[0].FixedSupplyPdo.VoltageIn50mV = 100;         // 5V
	Pdos[0].FixedSupplyPdo.MaximumCurrentIn10mA = 50;  // 0.5 A
	UcmConnectorPdSourceCaps(devCtx->Connector, Pdos, 1);
	UCM_CONNECTOR_PD_CONN_STATE_CHANGED_PARAMS PdParams;
	UCM_CONNECTOR_PD_CONN_STATE_CHANGED_PARAMS_INIT(&PdParams, UcmPdConnStateNotSupported);
	UcmConnectorPdConnectionStateChanged(devCtx->Connector, &PdParams);
	UcmConnectorPowerDirectionChanged(devCtx->Connector, TRUE, UcmPowerRoleSource);

	char value = (char)0;
	SetGPIO(devCtx, devCtx->VbusGpioId, &value);
	SetGPIO(devCtx, devCtx->PolGpioId, &value);
	SetGPIO(devCtx, devCtx->AmselGpioId, &value); // high = HDMI only, medium (unsupported) = USB only, low = both
	value = (char)1;
	SetGPIO(devCtx, devCtx->EnGpioId, &value);

	// Initialize the UC120
	char initvals[] = { 0x0C, 0x7C, 0x31, 0x5E, 0x9D, 0x0A, 0x7A, 0x2F, 0x5C, 0x9B };

	WriteRegister(devCtx, 18, initvals + 0, 1);
	WriteRegister(devCtx, 19, initvals + 1, 1);
	WriteRegister(devCtx, 20, initvals + 2, 1);
	WriteRegister(devCtx, 21, initvals + 3, 1);
	WriteRegister(devCtx, 26, initvals + 4, 1);
	WriteRegister(devCtx, 22, initvals + 5, 1);
	WriteRegister(devCtx, 23, initvals + 6, 1);
	WriteRegister(devCtx, 24, initvals + 7, 1);
	WriteRegister(devCtx, 25, initvals + 8, 1);
	WriteRegister(devCtx, 27, initvals + 9, 1);

	value = 0xFF;
	WriteRegister(devCtx, 2, &value, 1);
	WriteRegister(devCtx, 3, &value, 1);

	ReadRegister(devCtx, 4, &value, 1);
	value |= 1;
	WriteRegister(devCtx, 4, &value, 1);

	char registers[8];
	wchar_t buf[260];
	ULONG data = 0;

	ReadRegister(devCtx, 0, registers + 0, 1);
	ReadRegister(devCtx, 1, registers + 1, 1);
	ReadRegister(devCtx, 2, registers + 2, 1);
	ReadRegister(devCtx, 5, registers + 3, 1);
	ReadRegister(devCtx, 7, registers + 4, 1);
	ReadRegister(devCtx, 9, registers + 5, 1);
	ReadRegister(devCtx, 10, registers + 6, 1);
	ReadRegister(devCtx, 11, registers + 7, 1);
	swprintf(buf, L"INIT_%02x%02x%02x%02x%02x%02x%02x%02x", registers[0], registers[1], registers[2], registers[3], registers[4], registers[5], registers[6], registers[7]);

	/*status =*/ RtlWriteRegistryValue(RTL_REGISTRY_ABSOLUTE,
		(PCWSTR)L"\\Registry\\Machine\\System\\usbc",
		(PCWSTR)buf,
		REG_DWORD,
		&data,
		sizeof(ULONG));

	//UcmEventInitialize(&connCtx->EventSetDataRole);

Exit:

	return status;
}

NTSTATUS
LumiaUSBCKmCreateDevice(
    _Inout_ PWDFDEVICE_INIT DeviceInit
    )
/*++

Routine Description:

    Worker routine called to create a device and its software resources.

Arguments:

    DeviceInit - Pointer to an opaque init structure. Memory for this
                    structure will be freed by the framework when the WdfDeviceCreate
                    succeeds. So don't access the structure after that point.

Return Value:

    NTSTATUS

--*/
{
    WDF_OBJECT_ATTRIBUTES deviceAttributes;
	WDF_PNPPOWER_EVENT_CALLBACKS pnpPowerCallbacks;
    PDEVICE_CONTEXT deviceContext;
    WDFDEVICE device;
	UCM_MANAGER_CONFIG ucmConfig;
    NTSTATUS status;

    PAGED_CODE();

	WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpPowerCallbacks);
	pnpPowerCallbacks.EvtDevicePrepareHardware = LumiaUSBCDevicePrepareHardware;
	//pnpPowerCallbacks.EvtDeviceReleaseHardware = Fdo_EvtDeviceReleaseHardware;
	//pnpPowerCallbacks.EvtDeviceD0Entry = LumiaUSBCDeviceD0Entry;
	//pnpPowerCallbacks.EvtDeviceD0Exit = Fdo_EvtDeviceD0Exit;
	//pnpPowerCallbacks.EvtDeviceSelfManagedIoInit = Fdo_EvtDeviceSelfManagedIoInit;
	//pnpPowerCallbacks.EvtDeviceSelfManagedIoRestart = Fdo_EvtDeviceSelfManagedIoRestart;
	WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &pnpPowerCallbacks);

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&deviceAttributes, DEVICE_CONTEXT);

    status = WdfDeviceCreate(&DeviceInit, &deviceAttributes, &device);

    if (NT_SUCCESS(status)) {
        //
        // Get a pointer to the device context structure that we just associated
        // with the device object. We define this structure in the device.h
        // header file. DeviceGetContext is an inline function generated by
        // using the WDF_DECLARE_CONTEXT_TYPE_WITH_NAME macro in device.h.
        // This function will do the type checking and return the device context.
        // If you pass a wrong object handle it will return NULL and assert if
        // run under framework verifier mode.
        //
        deviceContext = DeviceGetContext(device);

        //
        // Initialize the context.
        //
		deviceContext->Device = device;
        deviceContext->Connector = NULL;

		UCM_MANAGER_CONFIG_INIT(&ucmConfig);
		status = UcmInitializeDevice(device, &ucmConfig);
		if (!NT_SUCCESS(status))
			return status;

        //
        // Create a device interface so that applications can find and talk
        // to us.
        //
        status = WdfDeviceCreateDeviceInterface(
            device,
            &GUID_DEVINTERFACE_LumiaUSBCKm,
            NULL // ReferenceString
            );

        if (NT_SUCCESS(status)) {
            //
            // Initialize the I/O Package and any Queues
            //
            status = LumiaUSBCKmQueueInitialize(device);
        }
    }

    return status;
}
