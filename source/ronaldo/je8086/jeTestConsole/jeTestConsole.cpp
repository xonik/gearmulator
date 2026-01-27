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
#include "esp/esp.hpp"

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

	//int g_faderOsc1Ctrl2 = 63;  // kFader_Osc1Ctrl2, range 0-127
	int g_faderOsc1Ctrl2 = -1;  // kFader_Osc1Ctrl2, range 0-127
	//int g_faderOsc1Ctrl1 = 63;  // kFader_Osc1Ctrl1, range 0-127
	int g_faderOsc1Ctrl1 = -1;  // kFader_Osc1Ctrl1, range 0-127
	int g_osc1Waveform = 0;     // Osc1Waveform, range 0-6 (SUPER SAW, TWM, ..., TRI)

	constexpr int kButtonReleaseCycles = 100;  // Number of cycles before button release

	struct PendingButtonRelease
	{
		devices::SwitchType button;
		int cyclesRemaining;
	};

	//static int notes[8] = {60, 62, 64, 65, 67, 69, 71, 72};	
	static int notes[8] = {60,60,60,60,60,60,60,60};	

	// Parameter cycling state
	struct PatchParam {
		Patch param;
		const char* name;
	};

	// All Patch enum values (excluding PatchName1-16)
	constexpr PatchParam g_patchParams[] = {
		{Patch::Lfo1Waveform, "Lfo1Waveform"},
		{Patch::Lfo1Rate, "Lfo1Rate"},
		{Patch::Lfo1Fade, "Lfo1Fade"},
		{Patch::Lfo2Rate, "Lfo2Rate"},
		{Patch::Lfo2DepthSelect, "Lfo2DepthSelect"},
		{Patch::RingModulatorSwitch, "RingModulatorSwitch"},
		{Patch::CrossModulationDepth, "CrossModulationDepth"},
		{Patch::OscillatorBalance, "OscillatorBalance"},
		{Patch::Lfo1AndEnvelopeDestination, "Lfo1AndEnvelopeDestination"},
		{Patch::OscLfo1Depth, "OscLfo1Depth"},
		{Patch::PitchLfo2Depth, "PitchLfo2Depth"},  // 0x1A - start cycling from here (index 10)
		{Patch::PitchEnvelopeDepth, "PitchEnvelopeDepth"},
		{Patch::PitchEnvelopeAttackTime, "PitchEnvelopeAttackTime"},
		{Patch::PitchEnvelopeDecayTime, "PitchEnvelopeDecayTime"},
		{Patch::Osc1Waveform, "Osc1Waveform"},
		{Patch::Osc1Control1, "Osc1Control1"},
		{Patch::Osc1Control2, "Osc1Control2"},
		{Patch::Osc2Waveform, "Osc2Waveform"},
		{Patch::Osc2SyncSwitch, "Osc2SyncSwitch"},
		{Patch::Osc2Range, "Osc2Range"},
		{Patch::Osc2FineWide, "Osc2FineWide"},
		{Patch::Osc2Control1, "Osc2Control1"},
		{Patch::Osc2Control2, "Osc2Control2"},
		{Patch::FilterType, "FilterType"},
		{Patch::CutoffSlope, "CutoffSlope"},
		{Patch::CutoffFrequency, "CutoffFrequency"},
		{Patch::Resonance, "Resonance"},
		{Patch::CutoffFrequencyKeyFollow, "CutoffFrequencyKeyFollow"},
		{Patch::FilterLfo1Depth, "FilterLfo1Depth"},
		{Patch::FilterLfo2Depth, "FilterLfo2Depth"},
		{Patch::FilterEnvelopeDepth, "FilterEnvelopeDepth"},
		{Patch::FilterEnvelopeAttackTime, "FilterEnvelopeAttackTime"},
		{Patch::FilterEnvelopeDecayTime, "FilterEnvelopeDecayTime"},
		{Patch::FilterEnvelopeSustainLevel, "FilterEnvelopeSustainLevel"},
		{Patch::FilterEnvelopeReleaseTime, "FilterEnvelopeReleaseTime"},
		{Patch::AmpLevel, "AmpLevel"},
		{Patch::AmpLfo1Depth, "AmpLfo1Depth"},
		{Patch::AmpLfo2Depth, "AmpLfo2Depth"},
		{Patch::AmpEnvelopeAttackTime, "AmpEnvelopeAttackTime"},
		{Patch::AmpEnvelopeDecayTime, "AmpEnvelopeDecayTime"},
		{Patch::AmpEnvelopeSustainLevel, "AmpEnvelopeSustainLevel"},
		{Patch::AmpEnvelopeReleaseTime, "AmpEnvelopeReleaseTime"},
		{Patch::AutoPanManualPanSwitch, "AutoPanManualPanSwitch"},
		{Patch::ToneControlBass, "ToneControlBass"},
		{Patch::ToneControlTreble, "ToneControlTreble"},
		{Patch::MultiEffectsType, "MultiEffectsType"},
		{Patch::MultiEffectsLevel, "MultiEffectsLevel"},
		{Patch::DelayType, "DelayType"},
		{Patch::DelayTime, "DelayTime"},
		{Patch::DelayFeedback, "DelayFeedback"},
		{Patch::DelayLevel, "DelayLevel"},
		{Patch::BendRangeUp, "BendRangeUp"},
		{Patch::BendRangeDown, "BendRangeDown"},
		{Patch::PortamentoSwitch, "PortamentoSwitch"},
		{Patch::PortamentoTime, "PortamentoTime"},
		{Patch::MonoSwitch, "MonoSwitch"},
		{Patch::LegatoSwitch, "LegatoSwitch"},
		{Patch::OscillatorShift, "OscillatorShift"},
		{Patch::ControlLfo1Rate, "ControlLfo1Rate"},
		{Patch::ControlLfo1Fade, "ControlLfo1Fade"},
		{Patch::ControlLfo2Rate, "ControlLfo2Rate"},
		{Patch::ControlCrossModulationDepth, "ControlCrossModulationDepth"},
		{Patch::ControlOscillatorBalance, "ControlOscillatorBalance"},
		{Patch::ControlPitchLfo1Depth, "ControlPitchLfo1Depth"},
		{Patch::ControlPitchLfo2Depth, "ControlPitchLfo2Depth"},
		{Patch::ControlPitchEnvelopeDepth, "ControlPitchEnvelopeDepth"},
		{Patch::ControlPitchEnvelopeAttackTime, "ControlPitchEnvelopeAttackTime"},
		{Patch::ControlPitchEnvelopeDecayTime, "ControlPitchEnvelopeDecayTime"},
		{Patch::ControlOsc1Control1, "ControlOsc1Control1"},
		{Patch::ControlOsc1Control2, "ControlOsc1Control2"},
		{Patch::ControlOsc2Range, "ControlOsc2Range"},
		{Patch::ControlOsc2FineWide, "ControlOsc2FineWide"},
		{Patch::ControlOsc2Control1, "ControlOsc2Control1"},
		{Patch::ControlOsc2Control2, "ControlOsc2Control2"},
		{Patch::ControlCutoffFrequency, "ControlCutoffFrequency"},
		{Patch::ControlResonance, "ControlResonance"},
		{Patch::ControlCutoffFreqKeyFollow, "ControlCutoffFreqKeyFollow"},
		{Patch::ControlFilterLfo1Depth, "ControlFilterLfo1Depth"},
		{Patch::ControlFilterLfo2Depth, "ControlFilterLfo2Depth"},
		{Patch::ControlFilterEnvDepth, "ControlFilterEnvDepth"},
		{Patch::ControlFilterEnvAttackTime, "ControlFilterEnvAttackTime"},
		{Patch::ControlFilterEnvDecayTime, "ControlFilterEnvDecayTime"},
		{Patch::ControlFilterEnvSustainLevel, "ControlFilterEnvSustainLevel"},
		{Patch::ControlFilterEnvReleaseTime, "ControlFilterEnvReleaseTime"},
		{Patch::ControlAmpLevel, "ControlAmpLevel"},
		{Patch::ControlAmpLfo1Depth, "ControlAmpLfo1Depth"},
		{Patch::ControlAmpLfo2Depth, "ControlAmpLfo2Depth"},
		{Patch::ControlAmpEnvAttackTime, "ControlAmpEnvAttackTime"},
		{Patch::ControlAmpEnvDecayTime, "ControlAmpEnvDecayTime"},
		{Patch::ControlAmpEnvSustainLevel, "ControlAmpEnvSustainLevel"},
		{Patch::ControlAmpEnvReleaseTime, "ControlAmpEnvReleaseTime"},
		{Patch::ControlToneControlBass, "ControlToneControlBass"},
		{Patch::ControlToneControlTreble, "ControlToneControlTreble"},
		{Patch::ControlMultiEffectsLevel, "ControlMultiEffectsLevel"},
		{Patch::ControlDelayTime, "ControlDelayTime"},
		{Patch::ControlDelayFeedback, "ControlDelayFeedback"},
		{Patch::ControlDelayLevel, "ControlDelayLevel"},
		{Patch::MorphBendAssign, "MorphBendAssign"},
		{Patch::ControlPortamentoTime, "ControlPortamentoTime"},
		{Patch::VelocitySwitch, "VelocitySwitch"},
		{Patch::VelocityLfo1Rate, "VelocityLfo1Rate"},
		{Patch::VelocityLfo1Fade, "VelocityLfo1Fade"},
		{Patch::VelocityLfo2Rate, "VelocityLfo2Rate"},
		{Patch::VelocityCrossModulationDepth, "VelocityCrossModulationDepth"},
		{Patch::VelocityOscillatorBalance, "VelocityOscillatorBalance"},
		{Patch::VelocityPitchLfo1Depth, "VelocityPitchLfo1Depth"},
		{Patch::VelocityPitchLfo2Depth, "VelocityPitchLfo2Depth"},
		{Patch::VelocityPitchEnvelopeDepth, "VelocityPitchEnvelopeDepth"},
		{Patch::VelocityPitchEnvelopeAttackTime, "VelocityPitchEnvelopeAttackTime"},
		{Patch::VelocityPitchEnvelopeDecayTime, "VelocityPitchEnvelopeDecayTime"},
		{Patch::VelocityOsc1Control1, "VelocityOsc1Control1"},
		{Patch::VelocityOsc1Control2, "VelocityOsc1Control2"},
		{Patch::VelocityOsc2Range, "VelocityOsc2Range"},
		{Patch::VelocityOsc2FineWide, "VelocityOsc2FineWide"},
		{Patch::VelocityOsc2Control1, "VelocityOsc2Control1"},
		{Patch::VelocityOsc2Control2, "VelocityOsc2Control2"},
		{Patch::VelocityCutoffFrequency, "VelocityCutoffFrequency"},
		{Patch::VelocityResonance, "VelocityResonance"},
		{Patch::VelocityCutoffFreqKeyFollow, "VelocityCutoffFreqKeyFollow"},
		{Patch::VelocityFilterLfo1Depth, "VelocityFilterLfo1Depth"},
		{Patch::VelocityFilterLfo2Depth, "VelocityFilterLfo2Depth"},
		{Patch::VelocityFilterEnvDepth, "VelocityFilterEnvDepth"},
		{Patch::VelocityFilterEnvAttackTime, "VelocityFilterEnvAttackTime"},
		{Patch::VelocityFilterEnvDecayTime, "VelocityFilterEnvDecayTime"},
		{Patch::VelocityFilterEnvSusLevel, "VelocityFilterEnvSusLevel"},
		{Patch::VelocityFilterEnvReleaseTime, "VelocityFilterEnvReleaseTime"},
		{Patch::VelocityAmpLevel, "VelocityAmpLevel"},
		{Patch::VelocityAmpLfo1Depth, "VelocityAmpLfo1Depth"},
		{Patch::VelocityAmpLfo2Depth, "VelocityAmpLfo2Depth"},
		{Patch::VelocityAmpEnvAttackTime, "VelocityAmpEnvAttackTime"},
		{Patch::VelocityAmpEnvDecayTime, "VelocityAmpEnvDecayTime"},
		{Patch::VelocityAmpEnvSustainLevel, "VelocityAmpEnvSustainLevel"},
		{Patch::VelocityAmpEnvReleaseTime, "VelocityAmpEnvReleaseTime"},
		{Patch::VelocityToneControlBass, "VelocityToneControlBass"},
		{Patch::VelocityToneControlTreble, "VelocityToneControlTreble"},
		{Patch::VelocityMultiEffectsLevel, "VelocityMultiEffectsLevel"},
		{Patch::VelocityDelayTime, "VelocityDelayTime"},
		{Patch::VelocityDelayFeedback, "VelocityDelayFeedback"},
		{Patch::VelocityDelayLevel, "VelocityDelayLevel"},
		{Patch::VelocityPortamentoTime, "VelocityPortamentoTime"},
		{Patch::ActiveIndicatorOfBender, "ActiveIndicatorOfBender"},
		{Patch::ActiveIndicatorOfVelocityAssign, "ActiveIndicatorOfVelocityAssign"},
		{Patch::ActiveIndicatorOfControlAssign, "ActiveIndicatorOfControlAssign"},
		{Patch::EnvelopeTypeInSolo, "EnvelopeTypeInSolo"},
		{Patch::Osc2ExternalInputSwitch, "Osc2ExternalInputSwitch"},
		{Patch::VoiceModulatorSendSwitch, "VoiceModulatorSendSwitch"},
		{Patch::UnisonSwitch, "UnisonSwitch"},
		{Patch::UnisonDetune, "UnisonDetune"},
		{Patch::PatchGain, "PatchGain"},
		{Patch::ExternalTriggerSwitch, "ExternalTriggerSwitch"},
		{Patch::ExternalTriggerDestination, "ExternalTriggerDestination"},
	};

	constexpr size_t g_numPatchParams = sizeof(g_patchParams) / sizeof(g_patchParams[0]);
	size_t g_currentParamIndex = 18;
	int g_currentParamValue = 0;  // 0 or 1

	// Waveform cycling state
	int g_osc1WaveformValue = 0;  // 0-6: SUPER SAW, TWM, ..., PULSE, SAW, TRI
	int g_osc2WaveformValue = 0;  // 0-3: PULSE, TRI, SAW, NOISE
	const char* g_osc1WaveformNames[] = {"SUPER SAW", "TWM", "??", "??", "PULSE", "SAW", "TRI"};
	const char* g_osc2WaveformNames[] = {"PULSE", "TRI", "SAW", "NOISE"};

	// Switch cycling state
	int g_ringModulatorValue = 0;  // 0-1: OFF, ON
	int g_osc2SyncValue = 0;  // 0-1: OFF, ON
	const char* g_switchNames[] = {"OFF", "ON"};

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

	void cyclePatchParameter()
	{
		const auto& param = g_patchParams[g_currentParamIndex];
		std::cout << param.name << "(" << g_currentParamIndex << ") = " << g_currentParamValue << std::endl;

			sendParameterChange(PerformanceData::PatchUpper, param.param, g_currentParamValue);

		// Toggle between 0 and 1, and advance parameter when going back to 0
		if (g_currentParamValue == 0)
		{
			g_currentParamValue = 1;
		}
		else
		{
			g_currentParamValue = 0;
			g_currentParamIndex = (g_currentParamIndex + 1) % g_numPatchParams;
		}
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
			case 'd':  // Osc1Control1 increment by 1
				if (g_faderOsc1Ctrl1 < 127) ++g_faderOsc1Ctrl1;
				sendParameterChange(PerformanceData::PatchUpper, Patch::Osc1Control1, g_faderOsc1Ctrl1);
				//std::cout << "Osc1 Control1: " << g_faderOsc1Ctrl1 << "\n";
				std::cout << "\nOsc1 Control1," << g_faderOsc1Ctrl1;
				break;
			case 'D':  // Osc1Control1 increment by 1
				if (g_faderOsc1Ctrl1 > 0) --g_faderOsc1Ctrl1;
				sendParameterChange(PerformanceData::PatchUpper, Patch::Osc1Control1, g_faderOsc1Ctrl1);
				//std::cout << "Osc1 Control1: " << g_faderOsc1Ctrl1 << "\n";
				std::cout << "\nOsc1 Control1," << g_faderOsc1Ctrl1;
				break;
			case 'm':  // Osc1Control2 minimum (0)
				if (g_faderOsc1Ctrl2 < 127) ++g_faderOsc1Ctrl2;
				sendParameterChange(PerformanceData::PatchUpper, Patch::Osc1Control2, g_faderOsc1Ctrl2);
				std::cout << "\nOsc1 Control2," << g_faderOsc1Ctrl2;
				break;
			case 'M':  // Osc1Control2 maximum (127)
				if (g_faderOsc1Ctrl2 > 0) --g_faderOsc1Ctrl2;
				sendParameterChange(PerformanceData::PatchUpper, Patch::Osc1Control2, g_faderOsc1Ctrl2);
				std::cout << "\nOsc1 Control2," << g_faderOsc1Ctrl2;
				break;
			case 'z':  // Cycle through Osc1Waveform values (0-6)
				g_osc1WaveformValue = (g_osc1WaveformValue + 1) % 7;
				sendParameterChange(PerformanceData::PatchUpper, Patch::Osc1Waveform, g_osc1WaveformValue);
				std::cout << "Osc1Waveform: " << g_osc1WaveformNames[g_osc1WaveformValue] << " (" << g_osc1WaveformValue << ")\n";
				break;
			case 'x':  // Cycle through Osc2Waveform values (0-3)
				g_osc2WaveformValue = (g_osc2WaveformValue + 1) % 4;
				sendParameterChange(PerformanceData::PatchUpper, Patch::Osc2Waveform, g_osc2WaveformValue);
				std::cout << "Osc2Waveform: " << g_osc2WaveformNames[g_osc2WaveformValue] << " (" << g_osc2WaveformValue << ")\n";
				break;
			case 't':  // Cycle through RingModulatorSwitch values (0-1)
				g_ringModulatorValue = (g_ringModulatorValue + 1) % 2;
				sendParameterChange(PerformanceData::PatchUpper, Patch::RingModulatorSwitch, g_ringModulatorValue);
				std::cout << "RingModulatorSwitch: " << g_switchNames[g_ringModulatorValue] << " (" << g_ringModulatorValue << ")\n";
				break;
			case 'y':  // Cycle through Osc2SyncSwitch values (0-1)
				g_osc2SyncValue = (g_osc2SyncValue + 1) % 2;
				sendParameterChange(PerformanceData::PatchUpper, Patch::Osc2SyncSwitch, g_osc2SyncValue);
				std::cout << "Osc2SyncSwitch: " << g_switchNames[g_osc2SyncValue] << " (" << g_osc2SyncValue << ")\n";
				break;
			case 'u': // Dump all ASICs
				device.getJe8086().getAsics().dump();
				std::cout << "ASIC dumps written.\n";
				break;
			case 'n': // Increase all 8 notes by 1
				for (int i = 0; i < 8; ++i) {
					if (notes[i] < 127) ++notes[i];
					addMidiEvent(synthLib::M_NOTEON, notes[i], 127);
				}
				std::cout << "All notes increased by 1: ";
				for (int i = 0; i < 8; ++i) std::cout << notes[i] << " ";
				std::cout << std::endl;
				break;
			case 'b': // Decrease all 8 notes by 1
				for (int i = 0; i < 8; ++i) {
					if (notes[i] < 127) --notes[i];
					addMidiEvent(synthLib::M_NOTEON, notes[i], 127);
				}
				std::cout << "All notes decrease by 1: ";
				for (int i = 0; i < 8; ++i) std::cout << notes[i] << " ";
				std::cout << std::endl;
				break;
			case 'N': // Increase all 8 notes by 1
				for (int i = 0; i < 8; ++i) {
					addMidiEvent(synthLib::M_NOTEOFF, notes[i], 127);
				}
				std::cout << "All notes off\n";
				break;
			case 'c':  // Cycle through all Patch parameters (0 then 1 for each)
				cyclePatchParameter();
				break;
/**
400 Cross modulation depth
44 Oscillator balance
OscLfo1Depth -> starts writing to 19.
041D, 041B -> Oscillator shift (two writes for each)
Osc1Waveform -> 0: 0445, 0442, 043f, 043c

Osc2Range endrer 19, Osc2FineWide endrer også 19

osc2waveform -> 045c, 0457 forever (NB: Har da byttet waveform bort fra supersaw! vet ikke om kode er oppdatert)
Osc1Control1 0x43F
Osc1Control2 0x43C
Etter osc2 waveform = 0, mens osc1 control1 and 2 er satt til 1, så endres
0x87, og så endres 82 for

Things that lead to program code changes:
RingModulatorSwitch(5)

Osc1Waveform

Osc2Waveform

Osc2SyncSwith
*/
				break;
			default:
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
					for (int i = 0; i < 8; ++i) {
						addMidiEvent(synthLib::M_NOTEON, notes[i], 127);
					}					

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
				/*std::cout << "Recorded " << (totalProcessedSamples / samplerate) << " sec"
						  << ", last interval speed " << static_cast<int>(intervalRealTimePercent) << "%, "
						  << "total average " << static_cast<int>(realTimePercent) << "%\n";
						  */
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
