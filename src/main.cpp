#include "webgpu-utils.h"

#include <webgpu/webgpu.h>
#include <GLFW/glfw3.h>

#include "App.h"

#include <cstdlib>
#include <iostream>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

int main(int, char**)
{
	App app;
	auto init_result = app.Initialize();
	if(init_result.IsErr())
	{
		std::cerr<< init_result.GetError() << std::endl;
		return EXIT_FAILURE;
	}

	std::cout << "Got queue: " << app.GetQueue() << std::endl;

	// The encoder records commands on the CPU side
	WGPUCommandEncoderDescriptor encoderDesc = {};
	encoderDesc.nextInChain = nullptr;
	encoderDesc.label = toWGPUStringView("My command encoder");
	WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(app.GetDevice(), &encoderDesc);

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
	wgpuQueueSubmit(app.GetQueue(), 1, &commandBuffer);
	wgpuCommandBufferRelease(commandBuffer);

	// Called once all the work submitted to the queue so far has finished
	WGPUQueueWorkDoneCallbackInfo workDoneCallbackInfo = {};
	workDoneCallbackInfo.nextInChain = nullptr;
	workDoneCallbackInfo.mode = WGPUCallbackMode_WaitAnyOnly;
	workDoneCallbackInfo.callback = [](WGPUQueueWorkDoneStatus status, WGPUStringView /* message */, void* /* pUserData1 */, void* /* pUserData2 */)
		{
			std::cout << "Queued work finished with status: " << status << std::endl;
		};
	WGPUFuture workDoneFuture = wgpuQueueOnSubmittedWorkDone(app.GetQueue(), workDoneCallbackInfo);

	// Without this wait the callback would never run, since WaitAnyOnly only
	// runs it from inside wgpuInstanceWaitAny.
	WGPUFutureWaitInfo workDoneWaitInfo = {};
	workDoneWaitInfo.future = workDoneFuture;
	workDoneWaitInfo.completed = false;
	if (wgpuInstanceWaitAny(app.GetInstance(), 1, &workDoneWaitInfo, UINT64_MAX) != WGPUWaitStatus_Success)
	{
		std::cout << "Waiting for the queued work failed" << std::endl;
	}

#ifdef __EMSCRIPTEN__
	// A blocking loop would freeze the browser tab, so let the browser call
	// MainLoop once per frame instead. With simulate_infinite_loop = true this
	// call never returns, which keeps `app` alive. The page has no close
	// button, so Terminate() is not called on the web.
	emscripten_set_main_loop_arg(
		[](void* userData) { static_cast<App*>(userData)->MainLoop(); },
		&app, 0, true);
#else
	while (app.IsRunning())
	{
		app.MainLoop();
	}

	std::cout << "Window is closed. Exiting..." << std::endl;
	app.Terminate();
#endif

	return 0;
}
