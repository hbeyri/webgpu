#include "webgpu-utils.h"

#include <webgpu/webgpu.h>

#include <iostream>

int main(int, char**)
{
	// We create a descriptor
	WGPUInstanceDescriptor desc = {};
	desc.nextInChain = nullptr;
	// Needed so wgpuInstanceWaitAny can wait with a timeout (used in requestAdapterSync)
	WGPUInstanceFeatureName requiredFeatures[] = { WGPUInstanceFeatureName_TimedWaitAny };
	desc.requiredFeatureCount = 1;
	desc.requiredFeatures = requiredFeatures;

	// We create the instance using this descriptor
	WGPUInstance instance = wgpuCreateInstance(&desc);

	// We can check whether there is actually an instance created
	if (!instance)
	{
		std::cerr << "Could not initialize WebGPU!" << std::endl;
		return 1;
	}

	// Display the object (WGPUInstance is a simple pointer, it may be
	// copied around without worrying about its size).
	std::cout << "WGPU instance: " << instance << std::endl;

	std::cout << "Requesting adapter..." << std::endl;
	WGPURequestAdapterOptions adapterOpts = {};
	adapterOpts.nextInChain = nullptr;
	WGPUAdapter adapter = requestAdapterSync(instance, &adapterOpts);

	// The request can fail (e.g. no supported GPU), in which case adapter is null
	if (!adapter)
	{
		std::cerr << "Could not get a WebGPU adapter!" << std::endl;
		wgpuInstanceRelease(instance);
		return 1;
	}
	std::cout << "Got adapter: " << adapter << std::endl;

	// Display some information about the adapter
	inspectAdapter(adapter);

	std::cout << "Requesting device..." << std::endl;

	WGPUDeviceDescriptor deviceDesc = {};
	deviceDesc.nextInChain = nullptr;
	deviceDesc.label = toWGPUStringView("My Device"); // anything works here, that's your call
	deviceDesc.requiredFeatureCount = 0; // we do not require any specific feature
	deviceDesc.requiredLimits = nullptr; // we do not require any specific limit (Default limits are minimal limits)
	deviceDesc.defaultQueue.nextInChain = nullptr;
	deviceDesc.defaultQueue.label = toWGPUStringView("The default queue");
	// Called when the device stops being usable, which includes releasing it at
	// the end of the program. AllowSpontaneous lets it run whenever that happens,
	// since nothing is waiting on a device loss with wgpuInstanceWaitAny.
	deviceDesc.deviceLostCallbackInfo.nextInChain = nullptr;
	deviceDesc.deviceLostCallbackInfo.mode = WGPUCallbackMode_AllowSpontaneous;
	deviceDesc.deviceLostCallbackInfo.callback = [](WGPUDevice const* /* device */, WGPUDeviceLostReason reason, WGPUStringView message, void* /* pUserData1 */, void* /* pUserData2 */)
		{
			std::cout << "Device lost: reason 0x" << std::hex << reason << std::dec << " (" << toStdStringView(message) << ")" << std::endl;
		};
	// Called for any error that is not caught by an error scope, for example a
	// validation error. It must be set here: the device has no setter for it.
	deviceDesc.uncapturedErrorCallbackInfo.nextInChain = nullptr;
	deviceDesc.uncapturedErrorCallbackInfo.callback = [](WGPUDevice const* /* device */, WGPUErrorType type, WGPUStringView message, void* /* pUserData1 */, void* /* pUserData2 */)
		{
			std::cout << "Uncaptured device error: type 0x" << std::hex << type << std::dec << " (" << toStdStringView(message) << ")" << std::endl;
		};
#ifdef WEBGPU_BACKEND_DAWN
	// By default, errors from command encoders are held back until
	// wgpuCommandEncoderFinish. This reports them from the call that caused them,
	// so the failing call is on the stack in the uncaptured error callback.
	char const* enabledToggles[] = { "enable_immediate_error_handling" };
	WGPUDawnTogglesDescriptor toggles = {};
	toggles.chain.next = nullptr;
	toggles.chain.sType = WGPUSType_DawnTogglesDescriptor;
	toggles.enabledToggleCount = 1;
	toggles.enabledToggles = enabledToggles;
	deviceDesc.nextInChain = &toggles.chain;
#endif // WEBGPU_BACKEND_DAWN

	WGPUDevice device = requestDeviceSync(instance, adapter, &deviceDesc);

	// The request can fail (e.g. a required limit is too high), in which case device is null
	if (!device)
	{
		std::cerr << "Could not get a WebGPU device!" << std::endl;
		wgpuAdapterRelease(adapter);
		wgpuInstanceRelease(instance);
		return 1;
	}
	std::cout << "Got device: " << device << std::endl;

	// we have our device, now we can release the adapter
	wgpuAdapterRelease(adapter);

	// Display information about the device
	inspectDevice(device);

	wgpuDeviceRelease(device);

	// Released last, since waiting on asynchronous work needs the instance
	wgpuInstanceRelease(instance);

	return 0;
}
