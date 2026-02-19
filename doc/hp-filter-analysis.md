# HP Filter Analysis — ESP DSP (TC170C140)

Analysis of program memory addresses 0x0482–0x049d in core 2 of DSP1.

---

## Q: Is this an HP filter, and what is the algorithm?

### 1. Yes, this is a high-pass filter

The code from 0x0482 to 0x049d implements a **two-stage cascaded Chamberlin State Variable Filter (SVF)**, with the output taken from the **HP (high-pass) node**. The cutoff frequency tracks the pitch via `mulcoeffs[0]`.

### 2. The algorithm

#### State variables (stored in IRAM between runs)

| Variable | Role | IRAM write addr | IRAM read addr (next run) |
|---|---|---|---|
| `BP1` | Band-pass, stage 1 | 0x90 (at 0x0496) | 0x91 |
| `LP1` | Low-pass, stage 1 | 0x92 (at 0x0487) | 0x93 |
| `BP2` | Band-pass, stage 2 | 0x94 (at 0x049c) | 0x95 |
| `LP2` | Low-pass, stage 2 | 0x96 (at 0x048f) | 0x97 |

(IRAM is circular — pointer decrements each program run, so a write to address X is read at X+1 next run.)

#### Parameters

- **`f`** = `mulcoeffs[0]` = pitch coefficient with 14-bit precision (set at 0x0481)
- **`M`** = `flip(mulcoeffs[2])` = input gain (mix-related crossfade coefficient)
- **`input`** = supersaw sum (accB after 0x0480, the 7-oscillator mix)

#### The filter equations

**Stage 1 — Chamberlin SVF:**

```
LP1[n] = LP1[n-1] + (f/2) · BP1[n-1]            ← OP1 (0x0482–0x0484)
HP1[n] = (input · M)/2 − LP1[n] − BP1[n-1]       ← OP5 (0x048d–0x048e)
BP1[n] = BP1[n-1] + f · HP1[n]                    ← OP7 (0x0491–0x0493)
```

**Stage 2 — Chamberlin SVF:**

```
LP2[n] = LP2[n-1] + f · BP2[n-1]                  ← OP4 (0x048a–0x048c)
HP2[n] = (5/16) · HP1[n] − LP2[n] − BP2[n-1]      ← OP8 (0x0494–0x0495)
BP2[n] = BP2[n-1] + f · HP2[n]                     ← OP9 (0x0497–0x0499)
```

**Output (to GRAM[0x70]):**

```
output = (29/16) · HP2[n]                          ← OP10 (0x049a, 0x049d)
```

#### Instruction-by-instruction trace

| Addr | Operation | What it computes |
|---|---|---|
| 0x0481 | Store pitch to mulcoeffs[0] | `f` = pitch (14-bit precision) |
| 0x0482–0x0484 | **OP1**: DMAC(BP1\_prev, f) shift=7, + LP1\_prev | `LP1_new = LP1_prev + (f/2)·BP1_prev` |
| 0x0485–0x0486 | **OP2**: input × flip(mulcoeffs[2]) 14-bit | `scaled_input = input · M` |
| 0x0487–0x0488 | **OP3**: Store LP1\_new to IRAM, + BP1\_prev | `temp = LP1_new + BP1_prev` |
| 0x0489 | Store OP2 | `iram[0x17] = scaled_input` |
| 0x048a–0x048c | **OP4**: DMAC(BP2\_prev, f) shift=6, + LP2\_prev | `LP2_new = LP2_prev + f·BP2_prev` |
| 0x048d–0x048e | **OP5**: −temp + scaled\_input/2 | `HP1 = input'/2 − LP1_new − BP1_prev` |
| 0x048f–0x0490 | **OP6**: Store LP2\_new, + BP2\_prev | `temp2 = LP2_new + BP2_prev` |
| 0x0491–0x0493 | **OP7**: DMAC(HP1, f) shift=6, + BP1\_prev | `BP1_new = BP1_prev + f·HP1` |
| 0x0494–0x0495 | **OP8**: −temp2 + HP1·(20/64) | `HP2 = (5/16)·HP1 − LP2_new − BP2_prev` |
| 0x0496 | Store BP1\_new to IRAM[0x90] | persists `BP1` for next run |
| 0x0497–0x0499 | **OP9**: BP2\_prev + DMAC(HP2, f) shift=6 | `BP2_new = BP2_prev + f·HP2` |
| 0x049a | **OP10**: HP2 × 116/64 | output gain ≈ 1.8125 |
| 0x049c | Store BP2\_new to IRAM[0x94] | persists `BP2` for next run |
| 0x049d | Store to GRAM[0x70] | output to inter-DSP bus |

#### Key observations

1. **It's a 4th-order (24 dB/oct) high-pass filter** — two cascaded 2nd-order SVF stages, both outputting from the HP node.

2. **Both stages use Q=1** (no resonance). The HP equation is `HP = input − LP − BP` with no Q multiplier on BP.

3. **Stage 1's LP integrator uses half the frequency coefficient** (`f/2`, shift=7) compared to all other integrators (`f`, shift=6). This is likely a crude frequency pre-warping correction for the Chamberlin SVF's known inaccuracy at higher frequencies — the forward-Euler integration introduces delay between integrators, and halving the LP coefficient partially compensates.

4. **The gain factors** (`M/2` at input, `5/16` between stages, `29/16` at output) provide level matching through the filter chain. Saw waves inherently have a DC offset, and an HP filter tracking pitch is the standard way to remove it while preserving harmonics.

5. **Note on the dump comments**: The sign-detection annotations (`>= 0 ? 0x7fffff : 0xFF800000`) at lines 0x0485, 0x0491, 0x0498 are **incorrect** — `weird` is false for all `kMulCoef` instructions in this section (coefficients 0x00, 0x02, 0x04, 0x07, 0x56 all fail the `(coef & 0x1c) == 0x1c` test), so `mulInputA` is the actual saturated accumulator value, not a sign detector. This is essential — a filter requires linear operations, not comparator-like sign detection.

---

## Q: If the cutoff is higher than the oscillator frequency, wouldn't it silence the oscillator?

Correct — my initial math was wrong. Here is the corrected calculation:

### Corrected calculation

The effective multiply with DMAC at shift=6 computes:

$$\text{result} = BP \times \frac{\text{pitch}}{2^{22}}$$

(The main multiply gives `BP * (pitch >> 16) >> 6`, and the DMAC adds the lower bits for 14-bit precision — together they approximate `BP * pitch / 2^22`.)

So the filter coefficient is:

$$f_{\text{coeff}} = \frac{\text{pitch}}{2^{22}}$$

The Chamberlin SVF relationship (small angle approximation):

$$f_{\text{coeff}} \approx \frac{2\pi f_c}{f_s}$$

$$f_c \approx \frac{\text{pitch} \times f_s}{2\pi \times 2^{22}}$$

The oscillator frequency:

$$f_{\text{osc}} = \frac{\text{pitch} \times f_s}{2^{24}}$$

So the ratio is:

$$\frac{f_c}{f_{\text{osc}}} = \frac{2^{24}}{2\pi \times 2^{22}} = \frac{4}{2\pi} = \frac{2}{\pi} \approx 0.637$$

### What this actually means

The HP cutoff is at **~64% of the oscillator fundamental frequency** — slightly *below* the fundamental, not above it. This makes complete physical sense:

- **The fundamental passes through** with minimal attenuation (it's above the −3dB point)
- **DC is completely eliminated** — with 24 dB/oct rolloff below cutoff, DC at 0 Hz is essentially gone
- **The cutoff tracks pitch** so it works correctly at every note

This is textbook DC-removal for a saw wave — saws have an inherent DC offset (their average is not zero), and a pitch-tracking HP filter removes it cleanly without affecting audible harmonics. The 4th-order (cascaded two SVFs) gives a steep enough rolloff that the transition from "block DC" to "pass fundamental" is sharp.
