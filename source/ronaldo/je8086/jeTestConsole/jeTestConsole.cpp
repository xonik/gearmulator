#include <iostream>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <vector>

#include "dsp56kEmu/audio.h"
#include "jeLib/device.h"
#include "jeLib/je8086.h"
#include "jeLib/je8086devices.h"
#include "jeLib/romloader.h"
#include "jeLib/state.h"
#include "synthLib/midiTypes.h"
#include "synthLib/wavWriter.h"

using namespace jeLib;

namespace
{
	termios g_originalTermios;
	bool g_termiosModified = false;

	void enableRawMode()
	{
		tcgetattr(STDIN_FILENO, &g_originalTermios);
		g_termiosModified = true;

		termios raw = g_originalTermios;
		raw.c_lflag &= ~(ICANON | ECHO);
		raw.c_cc[VMIN] = 0;
		raw.c_cc[VTIME] = 0;
		tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
	}

	void disableRawMode()
	{
		if (g_termiosModified)
			tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_originalTermios);
	}

	int getKeyPress()
	{
		char c;
		if (read(STDIN_FILENO, &c, 1) == 1)
			return c;
		return -1;
	}

	int g_faderOsc1Ctrl2 = 63;  // kFader_Osc1Ctrl2, range 0-127
	int g_faderOsc1Ctrl1 = 63;  // kFader_Osc1Ctrl1, range 0-127
	int g_osc1Waveform = 0;     // Osc1Waveform, range 0-6 (SUPER SAW, TWM, ..., TRI)

	constexpr int kButtonReleaseCycles = 100;  // Number of cycles before button release

	struct PendingButtonRelease
	{
		devices::SwitchType button;
		int cyclesRemaining;
	};

	std::vector<PendingButtonRelease> g_pendingReleases;
	std::vector<synthLib::SMidiEvent> g_pendingMidiIn;  // Queue for MIDI events to send via device.process()

	void scheduleButtonRelease(devices::SwitchType button)
	{
		g_pendingReleases.push_back({button, kButtonReleaseCycles});
	}

	void processPendingReleases(Je8086& je8086)
	{
		for (auto it = g_pendingReleases.begin(); it != g_pendingReleases.end(); )
		{
			if (--it->cyclesRemaining <= 0)
			{
				std::cout << "Releasing button " << static_cast<int>(it->button) << "\n";
				je8086.setButton(it->button, false);
				it = g_pendingReleases.erase(it);
			}
			else
			{
				++it;
			}
		}
	}

	// Helper to add MIDI event to pending queue (goes through device.process like the plugin)
	void addMidiEvent(uint8_t status, uint8_t data1, uint8_t data2)
	{
		synthLib::SMidiEvent ev(synthLib::MidiEventSource::Host);
		ev.a = status;
		ev.b = data1;
		ev.c = data2;
		g_pendingMidiIn.push_back(ev);
	}

	// Helper to send SysEx parameter change (goes through device.process like the plugin)
	void sendParameterChange(PerformanceData perfData, Patch param, int value)
	{
		auto sysex = State::createParameterChange(perfData, param, value);
		synthLib::SMidiEvent ev(synthLib::MidiEventSource::Host);
		ev.sysex = std::move(sysex);
		g_pendingMidiIn.push_back(ev);
	}

	// Helper to send SysEx parameter change for PerformanceCommon parameters
	void sendParameterChange(PerformanceCommon param, int value)
	{
		auto sysex = State::createParameterChange(param, value);
		synthLib::SMidiEvent ev(synthLib::MidiEventSource::Host);
		ev.sysex = std::move(sysex);
		g_pendingMidiIn.push_back(ev);
	}

	void handleKeyPress(int key, Device& device)
	{
		auto& je8086 = device.getJe8086();
		switch (key)
		{
			case 's':  // increase Osc1Ctrl2
				if (g_faderOsc1Ctrl2 < 127)
				{
					++g_faderOsc1Ctrl2;
					je8086.setFader(devices::kFader_Osc1Ctrl2, g_faderOsc1Ctrl2 * 8);  // scale 0-127 to 0-1016
					std::cout << "Osc1Ctrl2: " << g_faderOsc1Ctrl2 << "\n";
				}
				break;
			case 'S':  // decrease Osc1Ctrl2
				if (g_faderOsc1Ctrl2 > 0)
				{
					--g_faderOsc1Ctrl2;
					je8086.setFader(devices::kFader_Osc1Ctrl2, g_faderOsc1Ctrl2 * 8);
					std::cout << "Osc1Ctrl2: " << g_faderOsc1Ctrl2 << "\n";
				}
				break;
			case 'a':  // increase Osc1Ctrl1
				if (g_faderOsc1Ctrl1 < 127)
				{
					g_faderOsc1Ctrl1+=8;
					je8086.setFader(devices::kFader_Osc1Ctrl1, g_faderOsc1Ctrl1 * 8);
					std::cout << "Osc1Ctrl1: " << g_faderOsc1Ctrl1 << "\n";
				}
				break;
			case 'A':  // decrease Osc1Ctrl1
				if (g_faderOsc1Ctrl1 > 0)
				{
					g_faderOsc1Ctrl1-=8;
					je8086.setFader(devices::kFader_Osc1Ctrl1, g_faderOsc1Ctrl1 * 8);
					std::cout << "Osc1Ctrl1: " << g_faderOsc1Ctrl1 << "\n";
				}
				break;
			case 'p':  // quit
				disableRawMode();
				std::cout << "Quitting...\n";
				exit(0);
				break;			
			case 'w':  // Set waveform to SUPER SAW (0) via SysEx
				sendParameterChange(PerformanceData::PatchUpper, Patch::Osc1Waveform, 0);
				std::cout << "Osc1 Waveform: SUPER SAW (0)\n";
				break;
			case 'W':  // Set waveform to TRI (6) via SysEx
				sendParameterChange(PerformanceData::PatchUpper, Patch::Osc1Waveform, 6);
				std::cout << "Osc1 Waveform: TRI (6)\n";
				break;
			case 'd':  // Osc1Control1 minimum (0)
				sendParameterChange(PerformanceData::PatchUpper, Patch::Osc1Control1, 0);
				std::cout << "Osc1 Control1: 0 (min)\n";
				break;
			case 'D':  // Osc1Control1 maximum (127)
				sendParameterChange(PerformanceData::PatchUpper, Patch::Osc1Control1, 127);
				std::cout << "Osc1 Control1: 127 (max)\n";
				break;
			case 'm':  // Osc1Control2 minimum (0)
				sendParameterChange(PerformanceData::PatchUpper, Patch::Osc1Control2, 0);
				std::cout << "Osc1 Control2: 0 (min)\n";
				break;
			case 'M':  // Osc1Control2 maximum (127)
				sendParameterChange(PerformanceData::PatchUpper, Patch::Osc1Control2, 127);
				std::cout << "Osc1 Control2: 127 (max)\n";
				break;
			case 'z': 
				sendParameterChange(PerformanceData::PatchUpper, Patch::OscillatorBalance, 127);
				std::cout << "Setting OscBalance to 127\n";;
				break;
			case 'Z':  
				sendParameterChange(PerformanceData::PatchUpper, Patch::OscillatorBalance, 0);
				std::cout << "Setting OscBalance to 0\n";;
				break;
			case 'r':  // KeyMode SINGLE
				sendParameterChange(PerformanceCommon::KeyMode, 0);
				std::cout << "KeyMode: SINGLE (0)\n";
				break;
			case 't':  // KeyMode DUAL
				sendParameterChange(PerformanceCommon::KeyMode, 1);
				std::cout << "KeyMode: DUAL (1)\n";
				break;
			case 'y':  // KeyMode SPLIT
				sendParameterChange(PerformanceCommon::KeyMode, 2);
				std::cout << "KeyMode: SPLIT (2)\n";
				break;

		}
	}
}

int main(int _argc, char* _argv[])
{
	std::cout << "JE-8086 Test Console\n";
	std::cout << "Searching for ROM...\n";

	auto rom = RomLoader::findROM();

	if (!rom.isValid())
	{
		std::cerr << "No valid ROM found.\n";
		return -1;
	}

	std::cout << "ROM found, creating device...\n";

	synthLib::DeviceCreateParams params;

	params.romData = rom.getData();
	params.romName = rom.getName();

	constexpr uint32_t samplerate = 88200;
	params.hostSamplerate = samplerate;
	params.preferredSamplerate = samplerate;

	try
	{
		Device device(params);

		device.setMasterVolume(7.0f);

		enableRawMode();
		atexit(disableRawMode);

		std::cout << "Boot done, starting factory demo playback...\n";

		// prepare audio buffers
		std::array<std::vector<float>, 2> outBuffers;

		synthLib::TAudioInputs inputs;
		synthLib::TAudioOutputs outputs;

		constexpr size_t blocksize = 128;

		for (size_t i=0; i<outBuffers.size(); ++i)
		{
			outBuffers[i].resize(blocksize);
			outputs[i] = outBuffers[i].data();
		}

		std::vector<synthLib::SMidiEvent> midiOut;

		uint64_t sampleCounter = 0;

		bool bootFinished = false;
		bool demoRunning = false;

		SysexRemoteControl sysexRemote;

		sysexRemote.evLcdDdDataChanged.addListener([&](const std::array<char, 40>& _lcdContent)
		{
			std::cout << " Anything going on on the screen?\n";
			char lcdString[41]{0};

			for (size_t i=0; i<_lcdContent.size(); ++i)
				lcdString[i] = _lcdContent[i] >= ' ' ? static_cast<char>(_lcdContent[i]) : ' ';

			std::string s(lcdString);

			std::cout << "LCD: [" << s.substr(0 , 16) << "]\n";
			std::cout << "LCD: [" << s.substr(20, 16) << "]\n";

			if (!bootFinished)
			{
				if (s.find("PERFORM") != std::string::npos)
				{
					bootFinished = true;
					std::cout << "Boot finished, starting demo playback...\n";

					// With this pressed, the output of asic1 cycles through waveforms in the order it is listed
					// on osc1. It does not affect asic0.  The waveforms clip heavily
					//device.getJe8086().setButton(devices::kSwitch_Osc1Waveform, true);

					// This changes neither asic0 nor asic1 output, but it logs a lot of traffic to writeuC to asic 0, 1 and 2. 
					//device.getJe8086().setButton(devices::kSwitch_Osc2Waveform, true);
					//device.getJe8086().setButton(devices::kSwitch_Hold, true);
				}
			}
			else if(!demoRunning)
			{
				//if (s.find("=== ROM PLAY ===") != std::string::npos)
				if (s.find("Chariots       ") != std::string::npos)
				{
					demoRunning = true;

/*
Teori: asic 0 and 1 are upper and lower voices. se om 0 ednrer seg når man endrer waveform på osc1
Og hva med polyfoni, hvordan gjøres det - 
Antakelig derfor det er flere adresser som kopieres mellom asicene?



*/

					//device.getJe8086().addMidiEvent({synthLib::MidiEventSource::Host, synthLib::M_NOTEON, synthLib::Note_C4, 0x7f});
/*
					device.getJe8086().setButton(devices::kSwitch_Rec, false);
					device.getJe8086().setButton(devices::kSwitch_Hold, false);


					device.getJe8086().setButton(devices::kSwitch_Rec, true);
					device.getJe8086().setButton(devices::kSwitch_Hold, true);
*/					
					// Try to switch to manual mode					
					// Something definitely happens, we get a high pitch note.
					device.getJe8086().setButton(devices::kSwitch_Exit, true);
					device.getJe8086().setButton(devices::kSwitch_Write, true);	
					
					// Switch to key mode single - to make all 8 voices play the same patch
					// This makes detune++ stop working. Even switching to single after startup makes
					// it go silent and reverting to other mode won't work either.
					//sendParameterChange(PerformanceCommon::KeyMode, 0);
					std::cout << "Setting KeyMode to SINGLE (0)\n";

					// Set default waveform to triangle (6)
					//sendParameterChange(PerformanceData::PatchUpper, Patch::Osc1Waveform, 6);
					//std::cout << "Setting default Osc1 waveform to TRI (6)\n";

// no r, t ok, y ok
					// On startup, the two oscillators have completely different pitch, but they change to
					// the same once pitch is set using midi. I first thought these didn't work but they may
					// do once a pitch is set.
					//device.getJe8086().setFader(devices::kFader_Osc2Range, 0);
					//device.getJe8086().setFader(devices::kFader_FineTune, 0);

					// Route MIDI through device.process() like the plugin does
					addMidiEvent(synthLib::M_NOTEON, 56, 127);
					addMidiEvent(synthLib::M_NOTEON, 57, 127);
					addMidiEvent(synthLib::M_NOTEON, 58, 127);
					addMidiEvent(synthLib::M_NOTEON, 59, 127);
					addMidiEvent(synthLib::M_NOTEON, 60, 127);
					addMidiEvent(synthLib::M_NOTEON, 61, 127);
					addMidiEvent(synthLib::M_NOTEON, 62, 127);
					addMidiEvent(synthLib::M_NOTEON, 63, 127);
					// Knapper virker. Osc balace virker
					// detune/mix/range/fine/pulse width virker IKKE.

					std::cout << "Demo playback started, starting recording to .wav file.\n";

				}
			}
		});


		while (!demoRunning)
		{
			// Pass pending MIDI events through device.process() like the plugin does
			device.process(inputs, outputs, blocksize, g_pendingMidiIn, midiOut);
			g_pendingMidiIn.clear();

			for (const auto& e : midiOut)
				sysexRemote.receive(e);
			midiOut.clear();
		}

		synthLib::AsyncWriter writer("je8086_out.wav", samplerate);

		auto t0 = std::chrono::high_resolution_clock::now();
		auto tLast = t0;
		uint64_t totalProcessedSamples = 0;
		uint32_t intervalProcessedSamples = 0;

		while (true)
		{
			int key = getKeyPress();
			// After a couple of fader key presses, the display changes to 
			//"PERFORM * ", the star seems to indicate that the patch has changed. Good!
			if (key != -1)
				handleKeyPress(key, device);

			processPendingReleases(device.getJe8086());

			// Pass pending MIDI events through device.process() like the plugin does
			device.process(inputs, outputs, blocksize, g_pendingMidiIn, midiOut);
			g_pendingMidiIn.clear();

			for (const auto& e : midiOut)
				sysexRemote.receive(e);
			midiOut.clear();

			sampleCounter += blocksize;

			writer.append([&outBuffers, blocksize](std::vector<dsp56k::TWord>& _dst)
			{
				_dst.reserve(_dst.size() + blocksize * 2);

				for (size_t i=0; i<blocksize; ++i)
				{
					_dst.push_back(dsp56k::sample2dsp(outBuffers[0][i]));
					_dst.push_back(dsp56k::sample2dsp(outBuffers[1][i]));
				}
			});

			totalProcessedSamples += blocksize;
			intervalProcessedSamples += blocksize;

			if(intervalProcessedSamples >= samplerate)
			{
				auto tNow = std::chrono::high_resolution_clock::now();
				std::chrono::duration<double> elapsed = tNow - tLast;
				tLast = tNow;
				double intervalRealTimePercent = 100.0f * static_cast<double>(intervalProcessedSamples) / (elapsed.count() * params.hostSamplerate);
				double realTimePercent = 100.0f * static_cast<double>(totalProcessedSamples) / (std::chrono::duration<double>(tNow - t0).count() * params.hostSamplerate);
				std::cout << "Recorded " << (totalProcessedSamples / samplerate) << " sec"
						  << ", last interval speed " << static_cast<int>(intervalRealTimePercent) << "%, "
						  << "total average " << static_cast<int>(realTimePercent) << "%\n";
				intervalProcessedSamples -= samplerate;
			}
		}
	}
	catch (std::exception& e)
	{
		std::cerr << "Error: " << e.what() << "\n";
		return -1;
	}

	return 0;
}
