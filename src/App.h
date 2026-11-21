#pragma once

#include "core/Result.h"

#include <webgpu/webgpu.h>
#include <GLFW/glfw3.h>

#include <string>

class App
{
public:
	// Initialize everything, or return an error message if something failed.
	// On failure, everything that was already initialized is released.
	HB::TResult<void, std::string> Initialize();

	// Uninitialize everything that was initialized. Safe to call after a
	// failed or partial Initialize(), and safe to call more than once.
	void Terminate();

	// Draw a frame and handle events
	void MainLoop();

	// Return true as long as the main loop should keep on running
	bool IsRunning();

	WGPUInstance GetInstance() const { return m_Instance; }
	WGPUDevice GetDevice() const { return m_Device; }
	WGPUQueue GetQueue() const { return m_Queue; }

private:
	GLFWwindow* m_Window = nullptr;
	WGPUInstance m_Instance = nullptr;
	WGPUDevice m_Device = nullptr;
	WGPUQueue m_Queue = nullptr;
};
