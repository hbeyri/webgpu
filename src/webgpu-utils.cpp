#include "webgpu-utils.h"

#include <cassert>
#include <cstdint>
#include <iostream>

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
