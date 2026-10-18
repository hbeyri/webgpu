#include <webgpu/webgpu.h>

#include <iostream>
#include <cassert>
#include <cstdint>
#include <string_view>

// WebGPU strings are a pointer + length (WGPUStringView). A length of
// WGPU_STRLEN means the string is null-terminated.
std::string_view toStdStringView(WGPUStringView view) {
	if (!view.data) {
		return {};
	}
	if (view.length == WGPU_STRLEN) {
		return std::string_view(view.data);
	}
	return std::string_view(view.data, view.length);
}

/**
 * Utility function to get a WebGPU adapter, so that
 *     WGPUAdapter adapter = requestAdapterSync(options);
 * is roughly equivalent to
 *     const adapter = await navigator.gpu.requestAdapter(options);
 */
WGPUAdapter requestAdapterSync(WGPUInstance instance, WGPURequestAdapterOptions const * options) {
	// A simple structure holding the local information shared with the
	// onAdapterRequestEnded callback.
	struct UserData {
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
	auto onAdapterRequestEnded = [](WGPURequestAdapterStatus status, WGPUAdapter adapter, WGPUStringView message, void * pUserData, void * /* pUserData2 */) {
		UserData& userData = *reinterpret_cast<UserData*>(pUserData);
		if (status == WGPURequestAdapterStatus_Success) {
			userData.adapter = adapter;
		} else {
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
	// timeout requires the instance to be created with timedWaitAnyEnable.
	WGPUFutureWaitInfo waitInfo = {};
	waitInfo.future = future;
	waitInfo.completed = false;
	WGPUWaitStatus waitStatus = wgpuInstanceWaitAny(instance, 1, &waitInfo, UINT64_MAX);
	if (waitStatus != WGPUWaitStatus_Success) {
		std::cout << "Waiting for the adapter request failed" << std::endl;
	}

	assert(userData.requestEnded);

	return userData.adapter;
}

void inspectAdapter(WGPUAdapter adapter) {
#ifndef __EMSCRIPTEN__
	// WGPUSupportedLimits is gone: limits are read directly into WGPULimits
	WGPULimits limits = {};
	limits.nextInChain = nullptr;

	bool success = wgpuAdapterGetLimits(adapter, &limits) == WGPUStatus_Success;

	if (success) {
		std::cout << "Adapter limits:" << std::endl;
		std::cout << " - maxTextureDimension1D: " << limits.maxTextureDimension1D << std::endl;
		std::cout << " - maxTextureDimension2D: " << limits.maxTextureDimension2D << std::endl;
		std::cout << " - maxTextureDimension3D: " << limits.maxTextureDimension3D << std::endl;
		std::cout << " - maxTextureArrayLayers: " << limits.maxTextureArrayLayers << std::endl;
	}
#endif // NOT __EMSCRIPTEN__

	// wgpuAdapterEnumerateFeatures is replaced by wgpuAdapterGetFeatures, which
	// fills a WGPUSupportedFeatures struct in one call. The array it points to
	// is owned by WebGPU and must be freed with wgpuSupportedFeaturesFreeMembers.
	WGPUSupportedFeatures features = {};
	wgpuAdapterGetFeatures(adapter, &features);

	std::cout << "Adapter features:" << std::endl;
	std::cout << std::hex; // Write integers as hexadecimal to ease comparison with webgpu.h literals
	for (size_t i = 0; i < features.featureCount; ++i) {
		std::cout << " - 0x" << features.features[i] << std::endl;
	}
	std::cout << std::dec; // Restore decimal numbers
	wgpuSupportedFeaturesFreeMembers(features);

	// wgpuAdapterGetProperties is replaced by wgpuAdapterGetInfo. Strings are
	// now WGPUStringView, and the struct must be freed with
	// wgpuAdapterInfoFreeMembers.
	WGPUAdapterInfo info = {};
	info.nextInChain = nullptr;
	if (wgpuAdapterGetInfo(adapter, &info) == WGPUStatus_Success) {
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

int main (int, char**) {
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
    if (!instance) {
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
	if (!adapter) {
		std::cerr << "Could not get a WebGPU adapter!" << std::endl;
		wgpuInstanceRelease(instance);
		return 1;
	}
	std::cout << "Got adapter: " << adapter << std::endl;

	// Display some information about the adapter
	inspectAdapter(adapter);

    // We no longer need to use the instance once we have the adapter
    wgpuInstanceRelease(instance);

    wgpuAdapterRelease(adapter);

    return 0;
}