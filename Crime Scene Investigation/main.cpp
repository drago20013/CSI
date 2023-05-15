#include "SerialCon.h"

#include <windows.h>
#include <chrono>                   // for operator""s, chrono_literals
#include <ftxui/screen/screen.hpp>  // for Full, Screen
#include <iostream>                 // for cout, ostream
#include <memory>                   // for allocator, shared_ptr
#include <string>                   // for string, operator<<
#include <thread>                   // for sleep_for

#include "ftxui/dom/elements.hpp"  // for hflow, paragraph, separator, hbox, vbox, filler, operator|, border, Element
#include "ftxui/dom/node.hpp"      // for Render
#include "ftxui/dom/flexbox_config.hpp"  // for FlexboxConfig
#include "ftxui/screen/box.hpp"    // for ftxui
#include "ftxui/component/captured_mouse.hpp"  // for ftxui
#include "ftxui/component/component.hpp"       // for Input, Renderer, Vertical
#include "ftxui/component/component_base.hpp"  // for ComponentBase
#include "ftxui/component/component_options.hpp"  // for InputOption
#include "ftxui/component/screen_interactive.hpp"  // for Component, ScreenInteractive
#include "ftxui/util/ref.hpp"  // for Ref

using namespace ftxui;

int main()
{
	SerialCon connection;
	std::chrono::high_resolution_clock::time_point startTime;
	std::chrono::high_resolution_clock::time_point endTime;
	std::chrono::milliseconds duration;

	std::string toTransmit= {};
	std::string received = {};
	char ping[] = { 0x7 };
	char receivedPing[] = { 0x6 };
	std::atomic<bool> closeApp = false;

	std::vector<std::string> availableDataBits = { "5", "6", "7", "8" };
	int selectedByteSize = 0;
	std::vector<std::string> availableParity = { "None", "Odd", "Even" };
	int selectedParity = 0;
	std::vector<std::string> availableStopBits = { "1", "2" };
	int selectedStopBits = 0;
	std::vector<std::string> terminators = { "None", "CR", "LF", "CR_LF", "Other" };
	int selectedTerminator = 0;
	std::string otherTerminator;
	InputOption otherTermOption;
	otherTermOption.on_change = [&]() {
		if (otherTerminator.size() > 2)
			otherTerminator.resize(2);
	};
	auto screen = ScreenInteractive::Fullscreen();
	auto setOutBox = [&](std::string received) -> Elements {
		Elements linesComponents;

		std::vector<std::string> lines;
		std::string currentLine;
		currentLine.resize(received.size());
		int i{};
		for (char c : received) {
			if (c == '\n') {
				lines.push_back(currentLine);
				currentLine.clear();
				i = 0;
				currentLine.resize(received.size());
			}
			else if (c == '\r') {
				i = 0;
			}
			else if (c == '\t') {
				currentLine.resize(currentLine.size() + 4);
				currentLine[i++] = ' ';
				currentLine[i++] = ' ';
				currentLine[i++] = ' ';
				currentLine[i++] = ' ';
			}
			else {
				currentLine[i] = c;
				i++;
			}
		}

		lines.push_back(currentLine);

		// Create a vbox with text components for each line.
		for (const std::string& line : lines)
			linesComponents.push_back(text(line));

		return linesComponents;
	};

	auto outContainer = vbox(setOutBox(received));
	Component input_transmit = Input(&toTransmit, "");
	auto btns = Container::Horizontal({
		Button("Send", [&] {
			switch (selectedTerminator) {
			case 0:
				break;
			case 1:
				toTransmit += '\r';
				break;
			case 2:
				toTransmit += '\n';
				break;
			case 3:
				toTransmit += "\r\n";
				break;
			case 4:
				toTransmit += otherTerminator;
				break;
			}
			connection.Write(toTransmit.c_str(), toTransmit.size());
			toTransmit.resize(0);
			}), 
		Button("Ping", [&] { 
			//start time mesurment
			connection.Write(ping, 1);
			startTime = std::chrono::high_resolution_clock::now();
			}),
		Button("Clear", [&] {received = "";  outContainer = vbox(setOutBox(received)); }),
		});

	float timeVar = 0.f;

	auto Session = Container::Vertical({
			input_transmit | Renderer([](Element e) {return window(text("Input"), e); }),
			//Renderer([&]() {return window(text("Output"), paragraph(received) | size(HEIGHT, EQUAL, 10)); }), //  might change to be text
			Renderer([&]() {return window(text("Output"), outContainer | size(HEIGHT, EQUAL, 10)); }),
			btns,
			Renderer([&]() {
				return hbox({
					text("Ping time: "),
					text(std::to_string(timeVar)),
					text(" ms"),
					}) | border;
				}),
		});

	std::vector<std::string> tab_values{
		"Session",
		"Options",
	};

	int tab_selected = 0;
	auto tab_toggle = Toggle(&tab_values, &tab_selected);

	std::vector<std::wstring> availablePorts = { L"COM1" };
	int selectedPort = 0;
	std::vector<std::string> bitRates = { "110", "300", "600", "1200", "2400", "4800", "9600", "14400", "19200", "38400", "56000", "57600", "115200", "128000", "256000" };
	std::vector<std::string> flowControl = { "None", "XON / XOFF protocol", "RTS / CTS handshake", "DTR / DSR handshake"};
	int selectedFlowControl = 0;
	int selectedBitRate = 6;				 
	bool DTRchecked = false;				 
	bool RTSchecked = false;
	bool XofXonChecked = false;

	auto options_left = Container::Vertical({
		Container::Horizontal({
			Renderer([&]() {
				return text("COM port: ");
			}),
			Dropdown(&availablePorts, &selectedPort),
			Button("Refresh",[&]() {
					availablePorts = connection.GetAvailablePorts();
				}),
		}),

		Container::Horizontal({
			Renderer([&]() {
				return text("BitRate: ");
			}),
			Dropdown(&bitRates, &selectedBitRate)
		}),
		Container::Horizontal({
			Renderer([&]() {
				return text("Flow control: ");
			}),
			Dropdown(&flowControl, &selectedFlowControl)
		}),
	});

	auto options_right = Container::Vertical({
		Container::Horizontal({
			Renderer([&]() {
				return text("Data field: ");
			}),
			Dropdown(&availableDataBits, &selectedByteSize)
		}),
		Container::Horizontal({
			Renderer([&]() {
				return text("Parity: ");
			}),
			Dropdown(&availableParity, &selectedParity)
		}),
		Container::Horizontal({
			Renderer([&]() {
				return text("Stop bits: ");
			}),
			Dropdown(&availableStopBits, &selectedStopBits)
		}),
		Radiobox(&terminators, &selectedTerminator) | Renderer([](Element e) {return window(text("Terminator"), e); }),//IF other is choosen display input field
		Container::Horizontal({
			Renderer([&]() {
				return text("If other: ");
			}),
			Input(&otherTerminator, "", otherTermOption),
		}),
	});

	auto Options = Container::Horizontal({
		options_left | Renderer([](Element e) {return window(text("Connection"), e); }) | flex,
		options_right | Renderer([](Element e) {return window(text("PDU & Terminator"), e); }) | flex,
		});

	auto OptionsWithSave = Container::Vertical({
		Options,
		Button("Save/Connect/Reconnect", [&](){
			DTRchecked = false;
			RTSchecked = false;
			XofXonChecked = false;
			switch (selectedFlowControl) {
			case 1:
				XofXonChecked = true;
				break;
			case 2:
				RTSchecked = true;
				break;
			case 3:
				DTRchecked = true;
				break;
			}
			connection.Stop();
			connection.UnInit();
			connection.Init(availablePorts[selectedPort], stoi(bitRates[selectedBitRate]), selectedParity, selectedStopBits+1, selectedByteSize+5, DTRchecked, RTSchecked, XofXonChecked);
			connection.Start();
		}),
	});

	auto tab_container = Container::Tab(
		{
			Session,
			OptionsWithSave
		},
		&tab_selected);

	auto container = Container::Vertical({
		tab_toggle,
		tab_container,
		});

	auto renderer = Renderer(container, [&] {
		return window(paragraphAlignCenter("RS-232 Terminal"),
			vbox({
				tab_toggle->Render(),
				separator(),
				tab_container->Render(),
				}));
		});

	std::string temp = {};
	long count;
	std::wstring message = L"Hello from the new terminal!";

	std::thread refresh_ui([&] {
		while (!closeApp) {
			using namespace std::chrono_literals;
			std::this_thread::sleep_for(0.05s);
			
			// The |shift| variable belong to the main thread. `screen.Post(task)`
			// will execute the update on the thread where |screen| lives (e.g. the
			// main thread). Using `screen.Post(task)` is threadsafe.
			
			screen.Post([&] {
				connection.ReadAvailable(temp);

				//Check if "ping" then send back pingReceived;
				if (temp[0] == ping[0])
					connection.Write(receivedPing, 1);
				//if received back stop the timer
				else if(temp[0] == receivedPing[0])
					endTime = std::chrono::high_resolution_clock::now();

				duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
				timeVar = duration.count();

				received += temp;
				outContainer = vbox(setOutBox(received));
			});

			// After updating the state, request a new frame to be drawn. This is done
			// by simulating a new "custom" event to be handled.
			screen.PostEvent(Event::Custom);
		}
	});

	auto component = CatchEvent(renderer, [&](Event event) {
		if (event == Event::Escape) {
			if (!closeApp) {
				closeApp = true;
			}
			refresh_ui.join();
			screen.ExitLoopClosure()();

			return true;
		}
		return false;
		});

	screen.Loop(component);
}
