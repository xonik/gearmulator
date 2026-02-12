#pragma once

#include <cstdint>
#include <stdio.h>

// Lookup and print opcode name from hex value (based on list at line 880)
inline const char* getOpcodeName(uint8_t opc) {
	// All strings are 16 chars, left-padded with spaces
	switch (opc) {
		case 0x00: return "0x00            kMac";
		case 0x04: return "0x04            kMac";
		case 0x08: return "0x08      kStoreIRAM";
		case 0x0C: return "0x0C      kStoreIRAM";
		case 0x10: return "0x10            kMac";
		case 0x14: return "0x14            kMac";
		case 0x18: return "0x18      kStoreIRAM";
		case 0x1C: return "0x1C      kStoreIRAM";
		case 0x20: return "0x20       kReadGRAM";
		case 0x24: return "0x24       kReadGRAM";
		case 0x28: return "0x28         Unknown";
		case 0x2C: return "0x2C         Unknown";
		case 0x30: return "0x30        kMulCoef";
		case 0x34: return "0x34         Special";
		case 0x38: return "0x38      kStoreGRAM";
		case 0x3C: return "0x3C      kStoreGRAM";
		case 0x40: return "0x40 kStoreIRAMUnsat";
		case 0x44: return "0x44 kStoreIRAMUnsat";
		case 0x48: return "0x48  kStoreIRAMRect";
		case 0x4C: return "0x4C  kStoreIRAMRect";
		case 0x50: return "0x50   kSetCondition";
		case 0x54: return "0x54         Unknown";
		case 0x58: return "0x58      kStoreIRAM";
		case 0x5C: return "0x5C      kStoreIRAM";
		case 0x60: return "0x60         kInterp";
		case 0x64: return "0x64         kInterp";
		case 0x68: return "0x68 kInterpStorePos";
		case 0x6C: return "0x6C kInterpStorePos";
		case 0x70: return "0x70         kInterp";
		case 0x74: return "0x74         kInterp";
		case 0x78: return "0x78 kInterpStoreNeg";
		case 0x7C: return "0x7C kInterpStoreNeg";
		default: return "   <Unknown OPC>";
	}
}

inline const char* getOperationDescription(uint8_t opc) {
	switch (opc) {
		case 0x00: return "MAC only";
		case 0x04: return "MAC only";
		case 0x08: return "Store sat(A) to IRAM";
		case 0x0C: return "Store sat(B) to IRAM";
		case 0x10: return "MAC only";
		case 0x14: return "MAC only";
		case 0x18: return "Store sat(A) to IRAM";
		case 0x1C: return "Store sat(B) to IRAM";
		case 0x20: return "Load mulInputA from GRAM";
		case 0x24: return "Load mulInputA from GRAM";
		case 0x28: return "";
		case 0x2C: return "";
		case 0x30: return "";
		case 0x34: return "";
		case 0x38: return "Store sat(A) to GRAM";
		case 0x3C: return "Store sat(B) to IRAM";
		case 0x40: return "Store raw(A) to IRAM";
		case 0x44: return "Store raw(A) to IRAM";
		case 0x48: return "Store clamp(sat(A)) to IRAM";
		case 0x4C: return "Store clamp(sat(A)) to IRAM";
		case 0x50: return "Set skipfield";
		case 0x54: return "";
		case 0x58: return "Store sat(A) to IRAM";
		case 0x5C: return "Store sat(B) to IRAM";
		case 0x60: return "Max - abs(mulInputA)";
		case 0x64: return "Max - abs(mulInputA)";
		case 0x68: return "Pos val: MAX - abs(sat(A))";
		case 0x6C: return "Pos val: MAX - abs(sat(A))";
		case 0x70: return "Max - abs(mulInputA)";
		case 0x74: return "Max - abs(mulInputA)";
		case 0x78: return "abs(sat(A))";
		case 0x7C: return "abs(sat(A))";
		default: return "";
	}
}

inline int getPrefix(char* buf, int pos) {
	return snprintf(buf + pos, sizeof(buf) - pos, "                                                                                            |                      |                                             |  ");
}

// disassemble helper, gets default value for multiplier input A based on mem field
inline const char* getMulInputAFromMem(uint8_t mem) {
    switch (mem)
	{
		case 1: return "0x10";
		case 2: return "0x400";
		case 3: return "0x10000";
		case 4: return "0x400000";
		default: {
			static char buf[32];
			snprintf(buf, sizeof(buf), "iram[0x%02x]", mem);
			return buf;
		}
	}
}

// advanced coef calculation
// kMac + 3 x DMAC = c1 << 16 + c2 << 9 + c3 << 2 + c4 >> 6

inline const char* getMACString(const char* factorA, int8_t coeff, uint8_t shift) {
	static char buf[64];
	if(coeff == 128 && shift == 7 || coeff == 64 && shift == 6 || coeff == 32 && shift == 5 || coeff == 8 && shift == 3) {
		return factorA;
	} else if(coeff == 1){
		snprintf(buf, sizeof(buf), "%s >> %d, mulA=%s", factorA, shift, factorA);
		return buf;
	} else if(coeff == 0){
		snprintf(buf, sizeof(buf), "0, mulA=%s", factorA);
		return buf;
	} else {
		snprintf(buf, sizeof(buf), "%s * %d >> %d", factorA, coeff, shift);
		return buf;
	}
}

inline bool getClr(uint8_t opc, uint8_t mem, uint8_t coef){
	switch (opc)
	{
		case 0x30:
			return !(coef & 1);
		case 0x34:		
			if (mem >= 0xc0) {
				if((mem & 0x10)){
					return true;
				} else {
					return false;
				}
			} else {
				return false;
			}
		case 0x04:
		case 0x08:
		case 0x0c:
		case 0x14:
		case 0x18:
		case 0x1c:
		case 0x24:
		case 0x44:
		case 0x4c:
		case 0x50:
		case 0x64:
		case 0x6c:
		case 0x74:		
		case 0x7c:
			return true;
		default: return false;
	}
}


inline bool getAccumulator(uint8_t opc, uint8_t mem, uint8_t coef, uint8_t shiftbits){
	bool acc = false;
	switch (opc)
	{
		case 0x10:
		case 0x14:
		case 0x18:
		case 0x5c:
		case 0x1c: acc = true; break;
		case 0x20:
			acc = (shiftbits & 2);
			break;
		case 0x24:
			acc = (shiftbits & 2);
			break;
		case 0x30:
		{
			acc = (coef & 2);
		}
			break;
		case 0x34:
			if (mem >= 0xc0)
			{
				acc = (mem & 0x20);
			}
			break;
		
	}
	return acc;
}

inline uint32_t getOpcode(uint32_t address, const uint8_t* intmem) {
	// Look up opcode from intmem
	uint32_t opcode = 0;
	for (int i = 0; i < 4; i++) opcode |= (uint8_t)intmem[address * 4 + i] << (i * 8);
	return opcode & 0xfffffff;
}

inline uint8_t getOp(uint32_t opcode) {
	return ((opcode >> 18) << 2) & 0x7c;
}

inline uint8_t getMem(uint32_t opcode) {
	return (opcode >> 10) & 0xff;
}

inline uint8_t getCoef(uint32_t opcode) {
	return opcode & 0xff;
}

inline uint8_t getShiftbits(uint32_t opcode) {
	return (opcode >> 8) & 3;
}

inline bool getAccumulatorFromAddress(uint32_t address, const uint8_t* intmem) {
    uint32_t opcode = getOpcode(address, intmem);
    uint8_t op = getOp(opcode);
    uint8_t mem = getMem(opcode);
    uint8_t coef = getCoef(opcode);
    uint8_t shiftbits = getShiftbits(opcode);
    return getAccumulator(op, mem, coef, shiftbits);
}

// Get high-precision 24-bit coefficient from 0x04/0x14 instruction followed by up to three 0x34 instructions
inline uint32_t getHiPrecisionCoef(uint32_t address, const uint8_t* intmem) {
	uint32_t opcode = getOpcode(address, intmem);
	uint8_t op = getOp(opcode);
	
	// Must start with 0x04 or 0x14
	if (op != 0x04 && op != 0x14) return 0;
	
	uint8_t coef0 = getCoef(opcode);  // 8 bits
	uint8_t coef1 = 0, coef2 = 0, coef3 = 0;
	
	// Look forward at next addresses for 0x34 opcodes
	for (int i = 1; i <= 3; i++) {
		uint32_t nextOpcode = getOpcode(address + i, intmem);
		uint8_t nextOp = getOp(nextOpcode);
		if (nextOp != 0x34) break;
		
		uint8_t nextCoef = getCoef(nextOpcode);
		if (i == 1) coef1 = nextCoef;
		else if (i == 2) coef2 = nextCoef;
		else if (i == 3) coef3 = nextCoef;
	}
	
	// Combine into 24-bit number:
	// - 8 bits from first coef (bits 23-16)
	// - 7 LSB from second coef (bits 15-9)
	// - 7 LSB from third coef (bits 8-2)
	// - bits 6 and 5 from last coef (bits 1-0)
	uint32_t result = ((uint32_t)coef0 << 16) |
	                  ((uint32_t)(coef1 & 0x7f) << 9) |
	                  ((uint32_t)(coef2 & 0x7f) << 2) |
	                  ((coef3 >> 5) & 0x3);
	
	return result;
}

inline const char* getCoefComment(uint32_t address, const uint8_t* intmem) {
    uint32_t opcode = getOpcode(address, intmem);
	uint8_t op = getOp(opcode);
    if(op == 0x04 || op == 0x14) {
        uint32_t highPrecCoef = getHiPrecisionCoef(address, intmem);
        static char coefComment[64];
        snprintf(coefComment, sizeof(coefComment), "# Coefficient = %d (0x%06x)\n", highPrecCoef, highPrecCoef);        
        return coefComment;
    } else {
        return "";
    }
}


/*
Etter osc2 waveform = 0, mens osc1 control1 and 2 er satt til 1, så endres
0x87, og så endres 82 for*/

// Lookup and print opcode name from hex value (based on list at line 880)
inline const char* getAddressComment(uint32_t address, const uint8_t* intmem, bool clr) {
    const bool prevAccIsDifferent = false;
    bool acc = getAccumulatorFromAddress(address, intmem);

    // There may be empty addresses between two instructions with different accumulators, so we have to search backwards
    uint32_t prevAddr = address - 1;
    while(getOpcode(prevAddr, intmem) == 0 && prevAddr > 0) {
        prevAddr--;
    }
    bool prevAcc = address > 0 ? getAccumulatorFromAddress(prevAddr, intmem) : false;

	// All strings are 16 chars, left-padded with spaces
	switch (address) {
		case 0x0000: return "\n# Dump after setting note to 61, detune to 4 and mix to 424 \n";
		case 0x0005: return "\n# Ring modulator start \n";
		case 0x0008: return "# Ring modulator end\n\n";
		case 0x0019: return "\n# Sets Osc2 range and fine, and affected by OscLFO1Depth \n";
		case 0x0022: return "\n# Updated when pitch changes, first to 14336 then immediately back to 64\n";
		case 0x0044: return "\n# Set oscillator balance\n";
		case 0x0068: return "\n# Osc 2 sync start\n";
		case 0x006b: return "\n# Osc 2 start\n";
		case 0x00b2: return "# Osc 2 sync end\n\n";
		case 0x00b9: return "# Osc 2 end\n\n";
		case 0x0400: return "\n# Set X-mod depth\n";
		case 0x0404: return "\n# Input X1\n";		
		case 0x040d: return "\n# Stores X-mod depth\n";
    case 0x040e: return "\n# Input X2\n";
    case 0x0411: return "\n# Stores X1\n";
    case 0x0418: return "\n# Stores X2\n";
    case 0x0420: return "\n# Input X3\n";
    case 0x0424: return "\n# Stores pitch\n";
    case 0x0425: return "\n# Input X4\n";
    case 0x042a: return "\n# Stores X3\n";
    case 0x042b: return "\n# Stores X4\n";
    case 0x0430: return "\n# Input X5\n";
		case 0x0408: return "\n# Updated when pitch changes, first to 14336 then immediately back to 32\n";
		case 0x041b: return "\n# Pitch. Includes LFO from mcu, affected by Oscillator shift\n";
		case 0x043c: return "\n# Osc 1 start\n# Mix input (soft set)\n";
		case 0x043f: return "\n# Detune input (soft set)\n";
		case 0x0442: return "\n# Mix input (soft set pt. 2).\n";
		case 0x0445: return "\n# Detune input (soft set pt. 2).\n";
		case 0x0448: return "\n# Store mix to iram[0x15] and mulcoeff[1].\n";
		case 0x044a: return "\n# mix * 70 / 32\n";
		case 0x044c: return "\n# Stores detune to iram[0x13] and mulcoeff[0] TODO: WRONG MEM, ERROR IN SAT-CALC WHEN MULCOEFFS \n";
		case 0x044e: return "\n# Stores iram[0x06] (prev value of iram[0x05] to gram[0xf6] \n";
		case 0x044f: return "\n# Load pitch into A\n";
		case 0x0450: return "\n# Osc 1 - Load pitch and add previous value may be center oscillator\n";
    case 0x0452: return "\n# Write pitch to gram[0xf0]?\n";
    case 0x0453: return "\n# Store mix * 70 / 32 to mulcoeffs[2]\n";
    case 0x0454: return "\n# Osc 1 - Store updated value\n";
    case 0x0455: return "\n# Osc 2 - Multiply pitch and detune, 14bit precision, and add pitch + previous value for osc.\n";
    case 0x0459: return "\n# Osc 3 - Multiply pitch and detune, 14bit precision\n";
    case 0x045b: return "\n# Osc 2 - Store updated value\n";
		case 0x045c: return "\n# Osc 3 - Load pitch, add -1 * pitch * detune (from 0x0459++) + previous value\n";
    case 0x045f: return "\n# Osc 4 - Multiply pitch and detune, 14bit precision\n";
    case 0x0461: return "\n# Osc 3 - Store updated value\n";
    case 0x0462: return "\n# Osc 4 - Load pitch, add (pitch * detune (from 0x045f++) * 120) >> 5 + previous value\n";
    case 0x0465: return "\n# Osc 5 - Multiply pitch and detune, 14bit precision\n";
    case 0x0467: return "\n# Osc 4 - Store updated value\n";
    case 0x0468: return "\n# Osc 5 - Load pitch, add (pitch * detune (from 0x0465++) * -102) >> 5 + previous value\n";
    case 0x046b: return "\n# Osc 6 - Multiply pitch and detune, 14bit precision\n";
    case 0x046d: return "\n# Osc 5 - Store updated value\n";
    case 0x046e: return "\n# Osc 6 - Load pitch, add (pitch * detune (from 0x046b++) * 44) >> 3 + previous value\n";
    case 0x0471: return "\n# Osc 7 - Multiply pitch and detune, 14bit precision\n";
    case 0x0473: return "\n# Osc 6 - Store updated value\n";
    case 0x0474: return "\n# Osc 7 - Load pitch, add (pitch * detune (from 0x0471++) * -45) >> 3 + previous value\n";
    case 0x0477: return "\n# Load pitch with 14bit precision A\n";
    case 0x0479: return "\n# Osc 7 - Store updated value\n";
		case 0x047a: return "\n# Set B = (Osc 7 * (mix >> 16)) >> 7\n";
    case 0x047b: return "\n# B += (Osc 5 * (mix >> 16)) >> 7\n";
    case 0x047c: return "\n# B += (Osc 3 * (mix >> 16)) >> 7\n";
    case 0x047d: return "\n# B += (Osc 1 * 25) >> 7\n";
    case 0x047e: return "\n# B += (Osc 2 * (mix >> 16)) >> 7\n";
    case 0x047f: return "\n# B += (Osc 4 * (mix >> 16)) >> 7\n";
    case 0x0480: return "\n# B += (Osc 6 * (mix >> 16)) >> 7\n";
    case 0x0481: return "\n# Store pitch with 14bit precision, from 0x0477 (read from 0x0479), to mulcoeffs[0] \n";
    case 0x0482: return "\n# AS FAR AS I GOT, FILTERING PROB STARTS HERE\n";
		case 0x0485: return "# Mulcoeffs[2] read here is set right after center oscillator pitch (0x453), could this be HPF cutoff-related?\n";
		case 0x049e: return "# Osc 1 end\n\n\n";

		default: {
			return clr || acc != prevAcc ? "\n" : "";
		}
	}
	/*
	no changes from PitchLfo2Depth, 
	OscLfo1Depth changes pitch but no internal parameter.
	Lfo1Rate and Lfo1Fade do not seem to have any effect at all.

	*/
}


// Recursively search backwards from address to find previous instruction operating on same accumulator.
// If no work is done on an accumulator, the current value will propagate to the next place, so after three
// repetitions all values in the accumulator are the same, meaning we can just look for the last time the accumulator was written to.
inline uint16_t findPrevAccInstruction(uint16_t address, bool acc, const uint8_t* intmem) {
	// Never pass address 0x0000 or 0x4000 (0x1000 in word addressing)
	if (address == 0 || address == 0x400) return 0;
	
	// Move to previous address
	address--;
	
	// Look up opcode from intmem
	uint32_t opcode = getOpcode(address, intmem);
	
	// If opcode is 0, continue searching backwards
	if (!opcode) return findPrevAccInstruction(address, acc, intmem);
	
	// Check if this instruction operates on the same accumulator
	if (getAccumulator(getOp(opcode), getMem(opcode), getCoef(opcode), getShiftbits(opcode)) == acc) {
		return address;
	}
	
	// Continue searching backwards
	return findPrevAccInstruction(address, acc, intmem);
}


// Get opcode description with detailed operation explanation
inline const char* getOpcodeDesc(uint16_t address, bool lastWasOp30, const uint8_t* intmem) {

    uint32_t opcode = getOpcode(address, intmem);
    uint8_t opc = getOp(opcode);
    uint8_t mem = getMem(opcode);
    uint8_t coeff = getCoef(opcode);
    uint8_t shiftbits = getShiftbits(opcode);

	const int shifts[4] = {7, 6, 5, 3};
	const int shift = shifts[shiftbits & 3];
	const char* acc = (shiftbits & 2) ? "B" : "A";
	const int gramShift = (shiftbits & 1) ? 6 : 7;

    // accumulators are ring buffers. When reading sat(A) or raw(A), we need to find the last instruction that wrote to that accumulator 3 or more steps
    // back - it gets a bit confusing since there may be two instructions in-between that work on the same accumulator, but those values are not yet
    // available through sat(A) etc.
    const uint32_t prevAccAAddress = address < 2 ? 0 : findPrevAccInstruction(address-2, false, intmem);
    const uint32_t prevAccBAddress = address < 2 ? 0 : findPrevAccInstruction(address-2, true, intmem);

    static char satAStr[64];
    snprintf(satAStr, sizeof(satAStr), "sat(A) @ [0x%04x]", prevAccAAddress);

    static char satBStr[64];
    snprintf(satBStr, sizeof(satBStr), "sat(B) @ [0x%04x]", prevAccBAddress);

    static char rawAStr[64];
    snprintf(rawAStr, sizeof(rawAStr), "raw(A) @ [0x%04x]", prevAccAAddress);

    static char rawBStr[64];
    snprintf(rawBStr, sizeof(rawBStr), "raw(B) @ [0x%04x]", prevAccBAddress);

    const char* accChar = acc ? "B" : "A";
    const char* satAccStr = acc ? satBStr : satAStr;

    static char buf[8192];
    switch (opc) {
        case 0x00: snprintf(buf, sizeof(buf), "A += %s", getMACString(getMulInputAFromMem(mem), coeff, shift)); return buf;
        case 0x04: snprintf(buf, sizeof(buf), "A  = %s", getMACString(getMulInputAFromMem(mem), coeff, shift)); return buf;
        case 0x08: snprintf(buf, sizeof(buf), "A  = %s", getMACString(satAStr, coeff, shift)); return buf;
        case 0x0C: snprintf(buf, sizeof(buf), "A  = %s", getMACString(satBStr, coeff, shift)); return buf;
        case 0x10: snprintf(buf, sizeof(buf), "B += %s", getMACString(getMulInputAFromMem(mem), coeff, shift)); return buf;
        case 0x14: snprintf(buf, sizeof(buf), "B  = %s", getMACString(getMulInputAFromMem(mem), coeff, shift)); return buf;
        case 0x18: snprintf(buf, sizeof(buf), "B  = %s", getMACString(satAStr, coeff, shift)); return buf;
        case 0x1C: snprintf(buf, sizeof(buf), "B  = %s", getMACString(satBStr, coeff, shift)); return buf;
        //case 0x20: snprintf(buf, sizeof(buf), "%s += gram[0x%02x] * %d >> %d", acc, mem, coeff, gramShift); return buf;
        case 0x20: snprintf(buf, sizeof(buf), "%s += gram[0x%02x]", acc, mem); return buf;
        //case 0x24: snprintf(buf, sizeof(buf), "%s  = gram[0x%02x] * %d >> %d", acc, mem, coeff, gramShift); return buf;
        case 0x24: snprintf(buf, sizeof(buf), "%s  = gram[0x%02x]", acc, mem); return buf;
        case 0x28: return "N/A";
        case 0x2C: return "N/A";
        case 0x30: {
			bool clr = !(coeff & 1);
			bool weird = (coeff & 0x1c) == 0x1c;
			const char* accChar = (coeff & 2) ? "B" : "A";
			satAccStr = (coeff & 2) ? satBStr : satAStr;
			const char* clrChar = clr ? " " : "+";
			int pos = 0;

			if (coeff & 4) {
				if(weird){
					pos += snprintf(buf + pos, sizeof(buf) - pos, "%s\n", satAccStr);
					pos += getPrefix(buf, pos);
					pos += snprintf(buf + pos, sizeof(buf) - pos, "iram[0x%02x] = mulA\n", mem);					
				} else {
					pos += snprintf(buf + pos, sizeof(buf) - pos, "mulA = (%s >= 0 ? 0x7fffff : 0xFF800000)\n", satAccStr);
					pos += getPrefix(buf, pos);
					pos += snprintf(buf + pos, sizeof(buf) - pos, "iram[0x%02x] = mulA\n", mem);
				}
			} else {
				pos += snprintf(buf + pos, sizeof(buf) - pos, "mulA = %s\n", getMulInputAFromMem(mem));				
			}

			if ((coeff >> 5) == 6) {
				pos += getPrefix(buf, pos);
				pos += snprintf(buf + pos, sizeof(buf) - pos, "mulB = (eram.eramVarOffset << 11) & 0x7fffff\n");
			} else if ((coeff >> 5) == 7) {
				pos += getPrefix(buf, pos);
				pos += snprintf(buf + pos, sizeof(buf) - pos, "mulB = mulcoeffs[5]\n");
			} else {
				pos += getPrefix(buf, pos);
				pos += snprintf(buf + pos, sizeof(buf) - pos, "mulB = mulcoeffs[%d]\n", (coeff >> 5));
			}

			if ((coeff & 8) && !weird) {
				pos += getPrefix(buf, pos);
				pos += snprintf(buf + pos, sizeof(buf) - pos, "mulB *= -1\n");
			}
			if ((coeff & 16) && !weird) {
				pos += getPrefix(buf, pos);
				pos += snprintf(buf + pos, sizeof(buf) - pos, "if(mulB >= 0) mulB = (~mulB & 0x7fffff)\n");
				pos += getPrefix(buf, pos);
				pos += snprintf(buf + pos, sizeof(buf) - pos, "if(mulB < 0) mulB = ~(mulB & 0x7fffff)\n");
			}

			pos += getPrefix(buf, pos);			
			pos += snprintf(buf + pos, sizeof(buf) - pos, "save mulB to lastMulB\n");

			pos += getPrefix(buf, pos);
			pos += snprintf(buf + pos, sizeof(buf) - pos, "%s %s= (mulA * (mulB >> 16)) >> %d", accChar, clrChar, shift);

			return buf;
		}
        case 0x34: {
                if (mem >= 0xa0 && mem < 0xb0) {
                satAccStr = (mem & 1) ? satBStr : satAStr;                
                snprintf(buf, sizeof(buf), "mulcoeffs[%d] = %s;\n", (mem >> 1) & 7, satAccStr); return buf;
			} else if (mem >= 0xc0) {
				accChar = (mem & 0x20) ? "B" : "A";
				const char* clrChar = (mem & 0x10) ? " " : "+";
				switch (mem & 0xf)
				{
					case 0x0: snprintf(buf, sizeof(buf), "If %s=0 jump to 0x%04x", accChar, coeff); return buf;
					case 0x1: snprintf(buf, sizeof(buf), "If %s < 0 jump to 0x%04x", accChar, coeff); return buf;
					case 0x2: snprintf(buf, sizeof(buf), "If %s > 0 jump to 0x%04x", accChar, coeff); return buf;
					case 0x3: snprintf(buf, sizeof(buf), "jump to 0x%04x", coeff); return buf;
					case 0x4: return "Set INT pins";
					case 0x6: {
						if(lastWasOp30){
							snprintf(buf, sizeof(buf), "%s %s= ((lastMulA >> 7) * ((lastMulB >> 9) & 0x7f)) >> %d", accChar, clrChar, shift); return buf;													
							//snprintf(buf, sizeof(buf), "%s: Increase multiplication precision", accChar); return buf;
						} else {
							snprintf(buf, sizeof(buf), "%s %s= (lastMulA >> 7) * %d >> %d", accChar, clrChar, coeff, shift); return buf;
						}
					}
					case 0x7: snprintf(buf, sizeof(buf), "eram.eramVarOffset = %s", accChar); return buf;
					case 0xa: snprintf(buf, sizeof(buf), "readback_regs = %s", (mem & 0x20) ? satBStr : satAStr); return buf;
					case 0xb: snprintf(buf, sizeof(buf), "eram.eramWriteLatch = %s", (mem & 0x20) ? satBStr : satAStr); return buf;
					case 0xc:
					case 0xd:
					case 0xe: // TODO: These have a multiplication as well, need to figure out how multInputA_24 works in this case
					case 0xf: snprintf(buf, sizeof(buf), "iram[0x%02x] = eram.eramReadLatch (also sets mulInputA_24 so more happens)", mem | 0xf0); return buf;
					default:
						printf("Unknown value for mem (%02x) with opcode 0x34\n", mem);
						break;
				}
			} else {
				return "<---";
			}

		}
        case 0x38: snprintf(buf, sizeof(buf), "A += %s", getMACString(satAStr, coeff, shift)); return buf;
        //case 0x38: return "";
        case 0x3C: snprintf(buf, sizeof(buf), "A += %s", getMACString(satBStr, coeff, shift)); return buf;
        //case 0x3C: return "";
        case 0x40: snprintf(buf, sizeof(buf), "A += %s", getMACString(rawAStr, coeff, shift)); return buf;
        case 0x44: snprintf(buf, sizeof(buf), "A  = %s", getMACString(rawAStr, coeff, shift)); return buf;
        case 0x48: { static char rectSatA[80]; snprintf(rectSatA, sizeof(rectSatA), "rect(%s)", satAStr); snprintf(buf, sizeof(buf), "A += %s", getMACString(rectSatA, coeff, shift)); return buf; }
        case 0x4C: { static char rectSatA[80]; snprintf(rectSatA, sizeof(rectSatA), "rect(%s)", satAStr); snprintf(buf, sizeof(buf), "A  = %s", getMACString(rectSatA, coeff, shift)); return buf; }
        case 0x50: return "setcondition true, clear A. Some skipfield magic";
        case 0x54: return "N/A";
        case 0x58: snprintf(buf, sizeof(buf), "A += %s", getMACString(satAStr, coeff, shift)); return buf;
        //case 0x58: return "";
        case 0x5C: snprintf(buf, sizeof(buf), "B += %s", getMACString(satBStr, coeff, shift)); return buf;
        //case 0x5C: return "";
        case 0x60: snprintf(buf, sizeof(buf), "A += (1-abs(%s)) * %d", getMulInputAFromMem(mem), coeff); return buf;
        case 0x64: snprintf(buf, sizeof(buf), "A  = (1-abs(%s)) * %d", getMulInputAFromMem(mem), coeff); return buf;
        case 0x68: snprintf(buf, sizeof(buf), "A += (1-A)*%d if A is positive, %d * (A with sign removed) if negative", coeff, coeff); return buf;
        case 0x6C: snprintf(buf, sizeof(buf), "A  = (1-A)*%d if A is positive, %d * (A with sign removed) if negative", coeff, coeff); return buf;
        case 0x70: snprintf(buf, sizeof(buf), "A += (1-abs(%s)) * %d", getMulInputAFromMem(mem), coeff); return buf;
        case 0x74: snprintf(buf, sizeof(buf), "A  = (1-abs(%s)) * %d", getMulInputAFromMem(mem), coeff); return buf;
        case 0x78: snprintf(buf, sizeof(buf), "A += (1-A)*%d >> %d if A is negative, %s * %d >> %d if positive", coeff, shift, satAStr, coeff, shift); return buf;
        case 0x7C: snprintf(buf, sizeof(buf), "A  = (1-%s)*%d >> %d if A is negative, %s * %d >> %d if positive", satAStr, coeff, shift, satAStr, coeff, shift); return buf;
        default: return "<Unknown OPC>";
    }
}
