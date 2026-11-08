#include <webgpu/webgpu.h>

#include <iostream>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string_view>

// WebGPU strings are a pointer + length (WGPUStringView). A length of
// WGPU_STRLEN means the string is null-terminated.
std::string_view toStdStringView(WGPUStringView view)
{
	if (!view.data)
	{
		return {};
	}
	if (view.length == WGPU_STRLEN)
	{
		return std::string_view(view.data);
	}
	return std::string_view(view.data, view.length);
}

// Makes a WGPUStringView from a string literal, with its length known at
// compile time. Only use it with literals: for a char buffer, N is the size of
// the buffer rather than the length of the text it holds.
template <size_t N>
constexpr WGPUStringView toWGPUStringView(char const (&str)[N])
{
	return { str, N - 1 }; // N includes the null terminator, the length must not
}

/**
 * Utility function to get a WebGPU adapter, so that
 *     WGPUAdapter adapter = requestAdapterSync(instance, options);
 * is roughly equivalent to
 *     const adapter = await navigator.gpu.requestAdapter(options);
 */
WGPUAdapter requestAdapterSync(WGPUInstance instance, WGPURequestAdapterOptions const* options)
{
	// A simple structure holding the local information shared with the
	// onAdapterRequestEnded callback.
	struct UserData
	{
		WGPUAdapter adapter = nullptr;
		bool requestEnded = false;
	};
	UserData userData;

	// Callback called by wgpuInstanceRequestAdapter when the request returns
	// This is a C++ lambda function, but could be any function defined in the
	// global scope. It must be non-capturing (the brackets [] are empty) so
	// that it behaves like a regular C function pointer, which is what
	// wgpuInstanceRequestAdapter expects (WebGPU being a C API). The workaround
	// is to convey what we want to capture through the userdata1 pointer,
	// set in the callback info and received by the callback as its
	// second-to-last argument (userdata2 is a second, unused slot).
	auto onAdapterRequestEnded = [](WGPURequestAdapterStatus status, WGPUAdapter adapter, WGPUStringView message, void* pUserData, void* /* pUserData2 */)
		{
			UserData& userData = *reinterpret_cast<UserData*>(pUserData);
			if (status == WGPURequestAdapterStatus_Success)
			{
				userData.adapter = adapter;
			}
			else
			{
				std::cout << "Could not get WebGPU adapter: " << toStdStringView(message) << std::endl;
			}
			userData.requestEnded = true;
		};

	// The callback and its user data are now passed together in a struct.
	// WaitAnyOnly means the callback only runs from inside wgpuInstanceWaitAny.
	WGPURequestAdapterCallbackInfo callbackInfo = {};
	callbackInfo.nextInChain = nullptr;
	callbackInfo.mode = WGPUCallbackMode_WaitAnyOnly;
	callbackInfo.callback = onAdapterRequestEnded;
	callbackInfo.userdata1 = &userData;
	callbackInfo.userdata2 = nullptr;

	// Call to the WebGPU request adapter procedure. It returns a future that
	// completes once the callback has run.
	WGPUFuture future = wgpuInstanceRequestAdapter(
		instance /* equivalent of navigator.gpu */,
		options,
		callbackInfo
	);

	// Block until the request ends (this is the "await" part). An infinite
	// timeout requires the instance to be created with the TimedWaitAny feature.
	WGPUFutureWaitInfo waitInfo = {};
	waitInfo.future = future;
	waitInfo.completed = false;
	WGPUWaitStatus waitStatus = wgpuInstanceWaitAny(instance, 1, &waitInfo, UINT64_MAX);
	if (waitStatus != WGPUWaitStatus_Success)
	{
		std::cout << "Waiting for the adapter request failed" << std::endl;
	}

	assert(userData.requestEnded);

	return userData.adapter;
}

void inspectAdapter(WGPUAdapter adapter)
{
	WGPULimits limits = {};
	limits.nextInChain = nullptr;

	bool success = wgpuAdapterGetLimits(adapter, &limits) == WGPUStatus_Success;

	if (success)
	{
		std::cout << "Adapter limits:" << std::endl;
		std::cout << " - maxTextureDimension1D: " << limits.maxTextureDimension1D << std::endl;
		std::cout << " - maxTextureDimension2D: " << limits.maxTextureDimension2D << std::endl;
		std::cout << " - maxTextureDimension3D: " << limits.maxTextureDimension3D << std::endl;
		std::cout << " - maxTextureArrayLayers: " << limits.maxTextureArrayLayers << std::endl;
	}

	// wgpuAdapterGetFeatures fills a WGPUSupportedFeatures struct in one call. The array it points to
	// is owned by WebGPU and must be freed with wgpuSupportedFeaturesFreeMembers.
	WGPUSupportedFeatures features = {};
	wgpuAdapterGetFeatures(adapter, &features);

	std::cout << "Adapter features:" << std::endl;
	std::cout << std::hex; // Write integers as hexadecimal to ease comparison with webgpu.h literals
	for (size_t i = 0; i < features.featureCount; ++i)
	{
		std::cout << " - 0x" << features.features[i] << std::endl;
	}
	std::cout << std::dec; // Restore decimal numbers
	wgpuSupportedFeaturesFreeMembers(features);

	// wgpuAdapterGetProperties is replaced by wgpuAdapterGetInfo. Strings are
	// now WGPUStringView, and the struct must be freed with
	// wgpuAdapterInfoFreeMembers.
	WGPUAdapterInfo info = {};
	info.nextInChain = nullptr;
	if (wgpuAdapterGetInfo(adapter, &info) == WGPUStatus_Success)
	{
		std::cout << "Adapter info:" << std::endl;
		std::cout << " - vendorID: " << info.vendorID << std::endl;
		std::cout << " - vendor: " << toStdStringView(info.vendor) << std::endl;
		std::cout << " - architecture: " << toStdStringView(info.architecture) << std::endl;
		std::cout << " - deviceID: " << info.deviceID << std::endl;
		std::cout << " - device: " << toStdStringView(info.device) << std::endl;
		std::cout << " - description: " << toStdStringView(info.description) << std::endl;
		std::cout << std::hex;
		std::cout << " - adapterType: 0x" << info.adapterType << std::endl;
		std::cout << " - backendType: 0x" << info.backendType << std::endl;
		std::cout << std::dec; // Restore decimal numbers
		wgpuAdapterInfoFreeMembers(info);
	}
}

/**
 * Utility function to get a WebGPU device, so that
 *     WGPUDevice device = requestDeviceSync(instance, adapter, options);
 * is roughly equivalent to
 *     const device = await adapter.requestDevice(descriptor);
 * It is very similar to requestAdapter. The instance is needed to wait for the
 * request to complete.
 */
WGPUDevice requestDeviceSync(WGPUInstance instance, WGPUAdapter adapter, WGPUDeviceDescriptor const* descriptor)
{
	struct UserData
	{
		WGPUDevice device = nullptr;
		bool requestEnded = false;
	};
	UserData userData;

	auto onDeviceRequestEnded = [](WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView message, void* pUserData, void* /* pUserData2 */)
		{
			UserData& userData = *reinterpret_cast<UserData*>(pUserData);
			if (status == WGPURequestDeviceStatus_Success)
			{
				userData.device = device;
			}
			else
			{
				std::cout << "Could not get WebGPU device: " << toStdStringView(message) << std::endl;
			}
			userData.requestEnded = true;
		};

	WGPURequestDeviceCallbackInfo callbackInfo = {};
	callbackInfo.nextInChain = nullptr;
	callbackInfo.mode = WGPUCallbackMode_WaitAnyOnly;
	callbackInfo.callback = onDeviceRequestEnded;
	callbackInfo.userdata1 = &userData;
	callbackInfo.userdata2 = nullptr;

	WGPUFuture future = wgpuAdapterRequestDevice(
		adapter,
		descriptor,
		callbackInfo
	);

	WGPUFutureWaitInfo waitInfo = {};
	waitInfo.future = future;
	waitInfo.completed = false;
	WGPUWaitStatus waitStatus = wgpuInstanceWaitAny(instance, 1, &waitInfo, UINT64_MAX);
	if (waitStatus != WGPUWaitStatus_Success)
	{
		std::cout << "Waiting for the device request failed" << std::endl;
	}

	assert(userData.requestEnded);

	return userData.device;
}

void inspectDevice(WGPUDevice device)
{
	// As for the adapter, the array is owned by WebGPU and must be freed with
	// wgpuSupportedFeaturesFreeMembers.
	WGPUSupportedFeatures features = {};
	wgpuDeviceGetFeatures(device, &features);

	std::cout << "Device features:" << std::endl;
	std::cout << std::hex;
	for (size_t i = 0; i < features.featureCount; ++i)
	{
		std::cout << " - 0x" << features.features[i] << std::endl;
	}
	std::cout << std::dec;
	wgpuSupportedFeaturesFreeMembers(features);

	WGPULimits limits = {};
	limits.nextInChain = nullptr;

	bool success = wgpuDeviceGetLimits(device, &limits) == WGPUStatus_Success;

	if (success)
	{
		std::cout << "Device limits:" << std::endl;
		std::cout << " - maxTextureDimension1D: " << limits.maxTextureDimension1D << std::endl;
		std::cout << " - maxTextureDimension2D: " << limits.maxTextureDimension2D << std::endl;
		std::cout << " - maxTextureDimension3D: " << limits.maxTextureDimension3D << std::endl;
		std::cout << " - maxTextureArrayLayers: " << limits.maxTextureArrayLayers << std::endl;
	}
}

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
