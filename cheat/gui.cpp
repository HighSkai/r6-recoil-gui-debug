#include "gui.h"

#include "../imgui/imgui.h"
#include "../imgui/imgui_impl_dx9.h"
#include "../imgui/imgui_impl_win32.h"

#include <Windows.h>
#include <cmath>
#include <fstream>
#include <string>
#include <vector>
#include <iostream>
#include <sstream>

static std::string newConfigName;
static std::vector<std::string> configEntries;

static bool isMacroEnabled;
static bool wasGPressed;

static int verticalValue = 1;
static int angle = 1;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
	HWND window,
	UINT message,
	WPARAM wideParameter,
	LPARAM longParameter
);

long __stdcall WindowProcess(
	HWND window,
	UINT message,
	WPARAM wideParameter,
	LPARAM longParameter)
{
	if (ImGui_ImplWin32_WndProcHandler(window, message, wideParameter, longParameter))
		return true;

	switch (message)
	{
	case WM_SIZE: {
		if (gui::device && wideParameter != SIZE_MINIMIZED)
		{
			gui::presentParameters.BackBufferWidth = LOWORD(longParameter);
			gui::presentParameters.BackBufferHeight = HIWORD(longParameter);
			gui::ResetDevice();
		}
	}return 0;

	case WM_SYSCOMMAND: {
		if ((wideParameter & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
			return 0;
	}break;

	case WM_DESTROY: {
		PostQuitMessage(0);
	}return 0;

	case WM_LBUTTONDOWN: {
		gui::position = MAKEPOINTS(longParameter); // set click points
	}return 0;

	case WM_MOUSEMOVE: {
		if (wideParameter == MK_LBUTTON)
		{
			const auto points = MAKEPOINTS(longParameter);
			auto rect = ::RECT{ };

			GetWindowRect(gui::window, &rect);

			rect.left += points.x - gui::position.x;
			rect.top += points.y - gui::position.y;

			if (gui::position.x >= 0 &&
				gui::position.x <= gui::WIDTH &&
				gui::position.y >= 0 && gui::position.y <= 19)
				SetWindowPos(
					gui::window,
					HWND_TOPMOST,
					rect.left,
					rect.top,
					0, 0,
					SWP_SHOWWINDOW | SWP_NOSIZE | SWP_NOZORDER
				);
		}

	}return 0;

	}

	return DefWindowProc(window, message, wideParameter, longParameter);
}

void gui::CreateHWindow(const char* windowName) noexcept
{
	windowClass.cbSize = sizeof(WNDCLASSEX);
	windowClass.style = CS_CLASSDC;
	windowClass.lpfnWndProc = WindowProcess;
	windowClass.cbClsExtra = 0;
	windowClass.cbWndExtra = 0;
	windowClass.hInstance = GetModuleHandleA(0);
	windowClass.hIcon = 0;
	windowClass.hCursor = 0;
	windowClass.hbrBackground = 0;
	windowClass.lpszMenuName = 0;
	windowClass.lpszClassName = "class001";
	windowClass.hIconSm = 0;

	RegisterClassEx(&windowClass);

	window = CreateWindowEx(
		0,
		"class001",
		windowName,
		WS_POPUP,
		100,
		100,
		WIDTH,
		HEIGHT,
		0,
		0,
		windowClass.hInstance,
		0
	);

	ShowWindow(window, SW_SHOWDEFAULT);
	UpdateWindow(window);
}

void gui::DestroyHWindow() noexcept
{
	DestroyWindow(window);
	UnregisterClass(windowClass.lpszClassName, windowClass.hInstance);
}

bool gui::CreateDevice() noexcept
{
	d3d = Direct3DCreate9(D3D_SDK_VERSION);

	if (!d3d)
		return false;

	ZeroMemory(&presentParameters, sizeof(presentParameters));

	presentParameters.Windowed = TRUE;
	presentParameters.SwapEffect = D3DSWAPEFFECT_DISCARD;
	presentParameters.BackBufferFormat = D3DFMT_UNKNOWN;
	presentParameters.EnableAutoDepthStencil = TRUE;
	presentParameters.AutoDepthStencilFormat = D3DFMT_D16;
	presentParameters.PresentationInterval = D3DPRESENT_INTERVAL_ONE;

	if (d3d->CreateDevice(
		D3DADAPTER_DEFAULT,
		D3DDEVTYPE_HAL,
		window,
		D3DCREATE_HARDWARE_VERTEXPROCESSING,
		&presentParameters,
		&device) < 0)
		return false;

	return true;
}

void gui::ResetDevice() noexcept
{
	ImGui_ImplDX9_InvalidateDeviceObjects();

	const auto result = device->Reset(&presentParameters);

	if (result == D3DERR_INVALIDCALL)
		IM_ASSERT(0);

	ImGui_ImplDX9_CreateDeviceObjects();
}

void gui::DestroyDevice() noexcept
{
	if (device)
	{
		device->Release();
		device = nullptr;
	}

	if (d3d)
	{
		d3d->Release();
		d3d = nullptr;
	}
}

void gui::CreateImGui() noexcept
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ::ImGui::GetIO();

	io.IniFilename = NULL;

	ImGui::StyleColorsDark();

	ImGui_ImplWin32_Init(window);
	ImGui_ImplDX9_Init(device);
}

void gui::DestroyImGui() noexcept
{
	ImGui_ImplDX9_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

void gui::BeginRender() noexcept
{
	MSG message;
	while (PeekMessage(&message, 0, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&message);
		DispatchMessage(&message);

		if (message.message == WM_QUIT)
		{
			isRunning = !isRunning;
			return;
		}
	}

	// Start the Dear ImGui frame
	ImGui_ImplDX9_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}

void gui::EndRender() noexcept
{
	ImGui::EndFrame();

	device->SetRenderState(D3DRS_ZENABLE, FALSE);
	device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
	device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);

	device->Clear(0, 0, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_RGBA(0, 0, 0, 255), 1.0f, 0);

	if (device->BeginScene() >= 0)
	{
		ImGui::Render();
		ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
		device->EndScene();
	}

	const auto result = device->Present(0, 0, 0, 0);

	// Handle loss of D3D9 device
	if (result == D3DERR_DEVICELOST && device->TestCooperativeLevel() == D3DERR_DEVICENOTRESET)
		ResetDevice();
}

void dragMouse(int delay, double angle, double distance) {
	INPUT input;
	input.type = INPUT_MOUSE;

	double angleRad = angle * 3.141592653 / 180.0;
	input.mi.dx = static_cast<int>(distance * cos(angleRad));
	input.mi.dy = static_cast<int>(distance * sin(angleRad));
	input.mi.dwFlags = MOUSEEVENTF_MOVE;

	SendInput(1, &input, sizeof(INPUT));
	Sleep(delay);
}

void saveConfig() {
	std::ofstream configFile("C:/Users/Skai/Documents/GitHub/New folder/r6-recoil/cheat/config.txt");

	if (configFile.is_open()) {
		for (const auto& entry : configEntries) {
			configFile << entry << std::endl;
		}

		configFile.close();
		std::cout << "Configurations saved successfully." << std::endl;
	}
	else {
		std::cerr << "Error opening config.txt for writing." << std::endl;
	}
}

void loadConfig(const std::string& configLine) {
	std::stringstream ss(configLine);
	std::string configName;
	std::getline(ss, configName, ',');

	if (!ss.eof()) {
		std::string verticalValueStr, angleStr;
		std::getline(ss, verticalValueStr, ',');
		std::getline(ss, angleStr);

		verticalValue = std::stoi(verticalValueStr);
		angle = std::stoi(angleStr);

		std::cout << "Loaded config: " << configName << ", verticalValue: " << verticalValue << ", angle: " << angle << std::endl;
	}
}

void gui::Render() noexcept
{
	ImGuiStyle& style = ImGui::GetStyle();

	style.WindowRounding = 2.0f;
	style.FrameRounding = 4.0f;
	style.PopupRounding = 2.0f;
	style.GrabRounding = 3.0f;
	style.TabRounding = 4.0f;

	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

	ImGui::SetNextWindowPos({ 0, 0 });
	ImGui::SetNextWindowSize({ WIDTH, HEIGHT });
	ImGui::Begin(
		"Recoil Gui",
		&isRunning,
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoMove
	);

	ImGui::Text(R"(   
    ------------------------------------------------------------
    __________                    .__.__      ________      .__ 
    \______   \ ____   ____  ____ |__|  |    /  _____/ __ __|__|
     |       _// __ \_/ ___\/  _ \|  |  |   /   \  ___|  |  \  |
     |    |   \  ___/\  \__(  <_> )  |  |__ \    \_\  \  |  /  |
     |____|_  /\___  >\___  >____/|__|____/  \______  /____/|__|
            \/     \/     \/                        \/          
    ------------------------------------------------------------
)");

	ImGui::Checkbox(" [Enable / Disable]", &isMacroEnabled);

	ImGui::SliderInt(" [Speed]", &verticalValue, 1, 9);
	ImGui::SliderInt(" [Angle]", &angle, 1, 360);



	/////////////////////////////////////////////////////
	static std::string currentConfig;
	if (ImGui::BeginCombo("Load Config", currentConfig.c_str())) {
		std::ifstream configFile("C:/Users/Skai/Documents/GitHub/New folder/r6-recoil/cheat/config.txt"); //needs to be changed to personal folder
		std::string line;
		while (std::getline(configFile, line)) {
			if (ImGui::Selectable(line.c_str())) {
				currentConfig = line;
				loadConfig(line);
			}
		}
		configFile.close();
		ImGui::EndCombo();
	}
	/////////////////////////////////////////////////////

	/////////////////////////////////////////////////////
	if (ImGui::Button("Add Config")) {
		ImGui::OpenPopup("Add Config");
	}

	if (ImGui::BeginPopupModal("Add Config", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		static char configNameBuffer[256] = "";
		if (ImGui::InputText("Config Name", configNameBuffer, sizeof(configNameBuffer), ImGuiInputTextFlags_EnterReturnsTrue)) {
			newConfigName = configNameBuffer;
		}

		if (ImGui::Button("Save")) {
			newConfigName = configNameBuffer;

			std::string entry = newConfigName + "," + std::to_string(verticalValue) + "," + std::to_string(angle);
			configEntries.push_back(entry);
			saveConfig();
			ImGui::CloseCurrentPopup();
		}

		ImGui::SameLine();
		if (ImGui::Button("Cancel")) {
			memset(configNameBuffer, 0, sizeof(configNameBuffer));
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
	/////////////////////////////////////////////////////



	if (GetAsyncKeyState(0x47) & 0x8000)
	{
		if (!wasGPressed)
		{
			isMacroEnabled = !isMacroEnabled;
			wasGPressed = true;
		}
	}
	else
	{
		wasGPressed = false;
	}

	if (isMacroEnabled) {
		while (true) {
			if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) {
				int vDelay = 10 - (verticalValue);
				dragMouse(vDelay, angle, 2);
			}
			else {
				break;
			}
		}
	}

	ImVec2 windowSize = ImGui::GetIO().DisplaySize;

	ImVec2 textPos = ImVec2(windowSize.x - 100, windowSize.y - 20);
	ImGui::SetCursorPos(textPos);
	ImGui::Text("     by skai");

	ImGui::End();
}
