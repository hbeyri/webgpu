#include "App.h"

#include "webgpu-utils.h"

#include <webgpu/webgpu.h>
#include <GLFW/glfw3.h>

#include <iostream>

HB::TResult<void, HB::CString> App::Initialize()
{
	if (!glfwInit())
	{
		return HB::ResultErr("Could not initialize GLFW!");
	}

	// Create the window
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // <-- extra info for glfwCreateWindow
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	m_Window = glfwCreateWindow(640, 480, "Learn WebGPU", nullptr, nullptr);

	if (!m_Window)
	{
		Terminate();
		return HB::ResultErr("Could not open window!");
	}

	// We create a descriptor
	WGPUInstanceDescriptor desc = {};
	desc.nextInChain = nullptr;
	// Needed so wgpuInstanceWaitAny can wait with a timeout (used in requestAdapterSync)
	WGPUInstanceFeatureName requiredFeatures[] = { WGPUInstanceFeatureName_TimedWaitAny };
	desc.requiredFeatureCount = 1;
	desc.requiredFeatures = requiredFeatures;

	// We create the instance using this descriptor
	m_Instance = wgpuCreateInstance(&desc);

	// We can check whether there is actually an instance created
	if (!m_Instance)
	{
		Terminate();
		return HB::ResultErr("Could not initialize WebGPU!");
	}

	// Display the object (WGPUInstance is a simple pointer, it may be
	// copied around without worrying about its size).
	std::cout << "WGPU instance: " << m_Instance << std::endl;

	std::cout << "Requesting adapter..." << std::endl;
	WGPURequestAdapterOptions adapterOpts = {};
	adapterOpts.nextInChain = nullptr;
	WGPUAdapter adapter = requestAdapterSync(m_Instance, &adapterOpts);

	// The request can fail (e.g. no supported GPU), in which case adapter is null
	if (!adapter)
	{
		Terminate();
		return HB::ResultErr("Could not get a WebGPU adapter!");
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
			std::cout << "Device lost: reason 0x" << std::hex << reason << std::dec << " (" << toCStringView(message) << ")" << std::endl;
		};
	// Called for any error that is not caught by an error scope, for example a
	// validation error. It must be set here: the device has no setter for it.
	deviceDesc.uncapturedErrorCallbackInfo.nextInChain = nullptr;
	deviceDesc.uncapturedErrorCallbackInfo.callback = [](WGPUDevice const* /* device */, WGPUErrorType type, WGPUStringView message, void* /* pUserData1 */, void* /* pUserData2 */)
		{
			std::cout << "Uncaptured device error: type 0x" << std::hex << type << std::dec << " (" << toCStringView(message) << ")" << std::endl;
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

	m_Device = requestDeviceSync(m_Instance, adapter, &deviceDesc);

	// The request can fail (e.g. a required limit is too high), in which case device is null
	if (!m_Device)
	{
		wgpuAdapterRelease(adapter);
		Terminate();
		return HB::ResultErr("Could not get a WebGPU device!");
	}
	std::cout << "Got device: " << m_Device << std::endl;

	// we have our device, now we can release the adapter
	wgpuAdapterRelease(adapter);

	// Display information about the device
	inspectDevice(m_Device);

	// The queue is how we send work to the GPU. This call adds a reference, so
	// the queue must be released even though it belongs to the device.
	m_Queue = wgpuDeviceGetQueue(m_Device);

	return HB::ResultOk();
}

void App::Terminate()
{
	// Each handle is reset to null after release, so this works after a
	// partial Initialize() and does nothing when called a second time.
	if (m_Queue)
	{
		wgpuQueueRelease(m_Queue);
		m_Queue = nullptr;
	}

	if (m_Device)
	{
		wgpuDeviceRelease(m_Device);
		m_Device = nullptr;
	}

	// Released after the device, since waiting on asynchronous work needs the instance
	if (m_Instance)
	{
		wgpuInstanceRelease(m_Instance);
		m_Instance = nullptr;
	}

	if (m_Window)
	{
		glfwDestroyWindow(m_Window);
		m_Window = nullptr;
	}

	// Safe to call even if glfwInit() failed
	glfwTerminate();
}

void App::MainLoop()
{
	// Check whether the user clicked on the close button (and any other
	// mouse/key event, which we don't use so far)
#ifdef __EMSCRIPTEN__
	// The browser calls this once per frame, so it must not block
	glfwPollEvents();
#else
	// Nothing is drawn yet, so sleep until an event arrives instead of
	// spinning a CPU core. Switch to glfwPollEvents() once frames are rendered.
	glfwWaitEvents();
#endif
}

bool App::IsRunning()
{
	return m_Window && !glfwWindowShouldClose(m_Window);
}
