#pragma once

#include <webgpu/webgpu.h>

#include <cstddef>
#include <string_view>

// WebGPU strings are a pointer + length (WGPUStringView). A length of
// WGPU_STRLEN means the string is null-terminated.
std::string_view toStdStringView(WGPUStringView view);

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
WGPUAdapter requestAdapterSync(WGPUInstance instance, WGPURequestAdapterOptions const* options);

/**
 * Utility function to get a WebGPU device, so that
 *     WGPUDevice device = requestDeviceSync(instance, adapter, options);
 * is roughly equivalent to
 *     const device = await adapter.requestDevice(descriptor);
 * It is very similar to requestAdapter. The instance is needed to wait for the
 * request to complete.
 */
WGPUDevice requestDeviceSync(WGPUInstance instance, WGPUAdapter adapter, WGPUDeviceDescriptor const* descriptor);

// Print the limits, features and properties of an adapter
void inspectAdapter(WGPUAdapter adapter);

// Print the features and limits of a device
void inspectDevice(WGPUDevice device);
