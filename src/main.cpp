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

	// The queue is how we send work to the GPU. This call adds a reference, so
	// the queue must be released even though it belongs to the device.
	WGPUQueue queue = wgpuDeviceGetQueue(device);
	std::cout << "Got queue: " << queue << std::endl;

	// The encoder records commands on the CPU side
	WGPUCommandEncoderDescriptor encoderDesc = {};
	encoderDesc.nextInChain = nullptr;
	encoderDesc.label = toWGPUStringView("My command encoder");
	WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, &encoderDesc);

	// Debug markers label points in the command stream for GPU debugging tools
	wgpuCommandEncoderInsertDebugMarker(encoder, toWGPUStringView("Do one thing"));
	wgpuCommandEncoderInsertDebugMarker(encoder, toWGPUStringView("Do another thing"));

	// Finishing the encoder turns what it recorded into a command buffer. The
	// encoder cannot be used after this, so it can be released right away.
	WGPUCommandBufferDescriptor commandBufferDesc = {};
	commandBufferDesc.nextInChain = nullptr;
	commandBufferDesc.label = toWGPUStringView("My command buffer");
	WGPUCommandBuffer commandBuffer = wgpuCommandEncoderFinish(encoder, &commandBufferDesc);
	wgpuCommandEncoderRelease(encoder); // release encoder after it's finished

	// Send the command buffer to the GPU. It can only be submitted once, so it
	// can be released right after.
	std::cout << "Submitting command buffer..." << std::endl;
	wgpuQueueSubmit(queue, 1, &commandBuffer);
	wgpuCommandBufferRelease(commandBuffer);

	// Called once all the work submitted to the queue so far has finished
	WGPUQueueWorkDoneCallbackInfo workDoneCallbackInfo = {};
	workDoneCallbackInfo.nextInChain = nullptr;
	workDoneCallbackInfo.mode = WGPUCallbackMode_WaitAnyOnly;
	workDoneCallbackInfo.callback = [](WGPUQueueWorkDoneStatus status, WGPUStringView /* message */, void* /* pUserData1 */, void* /* pUserData2 */)
		{
			std::cout << "Queued work finished with status: " << status << std::endl;
		};
	WGPUFuture workDoneFuture = wgpuQueueOnSubmittedWorkDone(queue, workDoneCallbackInfo);

	// Without this wait the callback would never run, since WaitAnyOnly only
	// runs it from inside wgpuInstanceWaitAny.
	WGPUFutureWaitInfo workDoneWaitInfo = {};
	workDoneWaitInfo.future = workDoneFuture;
	workDoneWaitInfo.completed = false;
	if (wgpuInstanceWaitAny(instance, 1, &workDoneWaitInfo, UINT64_MAX) != WGPUWaitStatus_Success)
	{
		std::cout << "Waiting for the queued work failed" << std::endl;
	}

	wgpuQueueRelease(queue);
	wgpuDeviceRelease(device);

	// Released last, since waiting on asynchronous work needs the instance
	wgpuInstanceRelease(instance);

	return 0;
}
