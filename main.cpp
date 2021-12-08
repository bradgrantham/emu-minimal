#include <algorithm>
#include <map>
#include <string>
#include <queue>
#include <array>
#include <vector>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cassert>
#include <cstring>
#include <chrono>

#include <MiniFB.h>

constexpr bool debug = false;
constexpr int QuiescentEvaluateMaxCycles = 100;
constexpr uint64_t SystemClockRate = 3686400;
constexpr uint64_t CPUClockRate = 3686400;

constexpr size_t FlashSize = 512 * 1024;
constexpr size_t RAMSize = 32 * 1024;

constexpr uint32_t AI   = 0x0001; // latch bus into A
constexpr uint32_t AO   = 0x0002; // enable output from A
constexpr uint32_t BI   = 0x0004; // latch bus into B
constexpr uint32_t BO   = 0x0008; // enable output from B
constexpr uint32_t CI   = 0x0010; // latch bus into program counter low or high byte
constexpr uint32_t CO   = 0x0020; // enable output from program counter low or high byte
constexpr uint32_t EC   = 0x0040; // enable carry, also latch into BANK register if HI
constexpr uint32_t ES   = 0x0080; // negate B input to ALU
constexpr uint32_t CEME = 0x0100; // represents both CE/chipenable and ME/memoryenable
constexpr uint32_t EOFI = 0x0200; // represents both EO/accumulator-buffer-output-enable and FI/latch-flags-from-buffer
constexpr uint32_t HI   = 0x0400; // whether I/O is for low or high byte
constexpr uint32_t IC   = 0x0800; // reset microcode step register
constexpr uint32_t MI   = 0x1000; // latch bus into memory address low or high byte register (MAH,MAL)
constexpr uint32_t RI   = 0x2000; // latch bus into memory data  (RAM or ROM/Flash) using MAH,MAL,BANK registers
constexpr uint32_t RO   = 0x4000; // enable output from memory data using MAH,MAL,BANK registers
constexpr uint32_t TR   = 0x8000; // I/O transfer in and out depending on HI

/*
------------------------------------------------------------------------------
MIT License
Copyright (c) 2021 Carsten Herting
------------------------------------------------------------------------------
Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
------------------------------------------------------------------------------
*/

// XXX grantham - changed Out to OUT

// MINIMAL CPU SYSTEM - MICROCODE VERSION 1.5 written by Carsten Herting 17.04.2021
// Use for board revisions 1.3 and higher.

#define NOP     CO|MI, CO|MI|HI, RO|HI|CEME, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
#define BNK     CO|MI, CO|MI|HI, RO|HI|CEME, AO|EC|HI, IC, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
#define OUT     CO|MI, CO|MI|HI, RO|HI|CEME, AO|TR|HI, IC, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
#define CLC     CO|MI, CO|MI|HI, RO|HI|CEME, AO|BI, EOFI|ES,        IC, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
#define SEC     CO|MI, CO|MI|HI, RO|HI|CEME, AO|BI, EOFI|ES|EC,     IC, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
#define LSL     CO|MI, CO|MI|HI, RO|HI|CEME, AO|BI,  EOFI|AI,       IC, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
#define ROL0    CO|MI, CO|MI|HI, RO|HI|CEME, AO|BI,  EOFI|AI,       IC, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
#define ROL1    CO|MI, CO|MI|HI, RO|HI|CEME, AO|BI,  EOFI|AI|EC,    IC, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
#define LSR0    CO|MI, CO|MI|HI, RO|HI|CEME, AO|BI,  EOFI|ES,       EOFI|AI|BI,    EOFI|AI|BI,    EOFI|AI|BI,    EOFI|AI|BI,    EOFI|AI|BI,    EOFI|AI|BI,    EOFI|AI|BI,    EOFI|AI|BI,    IC, 0, 0
#define LSR1    CO|MI, CO|MI|HI, RO|HI|CEME, AO|BI,  EOFI|ES,       EOFI|EC|AI|BI, EOFI|EC|AI|BI, EOFI|EC|AI|BI, EOFI|EC|AI|BI, EOFI|EC|AI|BI, EOFI|EC|AI|BI, EOFI|EC|AI|BI, EOFI|EC|AI|BI, IC, 0, 0
#define ROR0    CO|MI, CO|MI|HI, RO|HI|CEME, AO|BI,  EOFI|AI|BI,    EOFI|AI|BI,    EOFI|AI|BI,    EOFI|AI|BI,    EOFI|AI|BI,    EOFI|AI|BI,    EOFI|AI|BI,    EOFI|AI|BI,    IC,            0,  0, 0
#define ROR1    CO|MI, CO|MI|HI, RO|HI|CEME, AO|BI,  EOFI|AI|BI|EC, EOFI|EC|AI|BI, EOFI|EC|AI|BI, EOFI|EC|AI|BI, EOFI|EC|AI|BI, EOFI|EC|AI|BI, EOFI|EC|AI|BI, EOFI|EC|AI|BI, IC,            0,  0, 0
#define ASR00x  CO|MI, CO|MI|HI, RO|HI|CEME, BI, EOFI|EC, AO|BI,    EOFI|ES,       EOFI|AI|BI,    EOFI|AI|BI,    EOFI|AI|BI,    EOFI|AI|BI,    EOFI|AI|BI,    EOFI|AI|BI,    EOFI|AI|BI,    EOFI|AI|BI,    IC
#define ASR01x  CO|MI, CO|MI|HI, RO|HI|CEME, BI, EOFI|EC, AO|BI,    EOFI|ES,       EOFI|EC|AI|BI, EOFI|EC|AI|BI, EOFI|EC|AI|BI, EOFI|EC|AI|BI, EOFI|EC|AI|BI, EOFI|EC|AI|BI, EOFI|EC|AI|BI, EOFI|EC|AI|BI, IC
#define ASR10x  CO|MI, CO|MI|HI, RO|HI|CEME, BI, EOFI|EC, AO|BI,    EOFI|ES|EC,    EOFI|AI|BI,    EOFI|AI|BI,    EOFI|AI|BI,    EOFI|AI|BI,    EOFI|AI|BI,    EOFI|AI|BI,    EOFI|AI|BI,    EOFI|AI|BI,    IC
#define ASR11x  CO|MI, CO|MI|HI, RO|HI|CEME, BI, EOFI|EC, AO|BI,    EOFI|ES|EC,    EOFI|EC|AI|BI, EOFI|EC|AI|BI, EOFI|EC|AI|BI, EOFI|EC|AI|BI, EOFI|EC|AI|BI, EOFI|EC|AI|BI, EOFI|EC|AI|BI, EOFI|EC|AI|BI, IC
#define INP     CO|MI, CO|MI|HI, RO|HI|CEME, TR|AI|BI, EOFI|ES|BI, EOFI|ES|EC, IC, 0, 0, 0, 0, 0, 0, 0, 0, 0
#define NEG     CO|MI, CO|MI|HI, RO|HI|CEME, AO|BI, EOFI|ES|EC|AI, EOFI|ES|EC|AI, IC, 0, 0, 0, 0, 0, 0, 0, 0, 0
#define Inc     CO|MI, CO|MI|HI, RO|HI|CEME, BI, EOFI|ES|EC|AI, IC, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
#define Dec     CO|MI, CO|MI|HI, RO|HI|CEME, BI, EOFI|AI, IC, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0

#define LDI     CO|MI, CO|MI|HI, RO|HI|CEME, RO|AI|CEME, IC, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
#define ADI     CO|MI, CO|MI|HI, RO|HI|CEME, RO|BI, EOFI|AI|CEME, IC, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
#define SBI     CO|MI, CO|MI|HI, RO|HI|CEME, RO|BI, EOFI|ES|EC|AI|CEME, IC, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
#define CPI     CO|MI, CO|MI|HI, RO|HI|CEME, RO|BI, EOFI|ES|EC|CEME, IC, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
#define ACI0    CO|MI, CO|MI|HI, RO|HI|CEME, RO|BI, EOFI|AI|CEME, IC, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
#define ACI1    CO|MI, CO|MI|HI, RO|HI|CEME, RO|BI, EOFI|EC|AI|CEME, IC, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
#define SCI0    CO|MI, CO|MI|HI, RO|HI|CEME, RO|BI, EOFI|ES|AI|CEME, IC, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
#define SCI1    CO|MI, CO|MI|HI, RO|HI|CEME, RO|BI, EOFI|ES|EC|AI|CEME, IC, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0

#define JPA     CO|MI, CO|MI|HI, RO|HI|CEME, RO|BI|CEME, RO|CI|HI, BO|CI, IC, 0, 0, 0, 0, 0, 0, 0, 0, 0
#define LDA     CO|MI, CO|MI|HI, RO|HI|CEME, RO|BI|CEME, RO|MI|HI, BO|MI, RO|AI|CEME, IC, 0, 0, 0, 0, 0, 0, 0, 0
#define STA     CO|MI, CO|MI|HI, RO|HI|CEME, RO|BI|CEME, RO|MI|HI, BO|MI, AO|RI, CEME, IC, 0, 0, 0, 0, 0, 0, 0
#define ADA     CO|MI, CO|MI|HI, RO|HI|CEME, RO|BI|CEME, RO|MI|HI, BO|MI, RO|BI, EOFI|AI|CEME, IC, 0, 0, 0, 0, 0, 0, 0
#define SBA     CO|MI, CO|MI|HI, RO|HI|CEME, RO|BI|CEME, RO|MI|HI, BO|MI, RO|BI, EOFI|ES|EC|AI|CEME, IC, 0, 0, 0, 0, 0, 0, 0
#define CPA     CO|MI, CO|MI|HI, RO|HI|CEME, RO|BI|CEME, RO|MI|HI, BO|MI, RO|BI, EOFI|ES|EC|CEME, IC, 0, 0, 0, 0, 0, 0, 0
#define ACA0    CO|MI, CO|MI|HI, RO|HI|CEME, RO|BI|CEME, RO|MI|HI, BO|MI, RO|BI, EOFI|AI|CEME, IC, 0, 0, 0, 0, 0, 0, 0
#define ACA1    CO|MI, CO|MI|HI, RO|HI|CEME, RO|BI|CEME, RO|MI|HI, BO|MI, RO|BI, EOFI|EC|AI|CEME, IC, 0, 0, 0, 0, 0, 0, 0
#define SCA0    CO|MI, CO|MI|HI, RO|HI|CEME, RO|BI|CEME, RO|MI|HI, BO|MI, RO|BI, EOFI|ES|AI|CEME, IC, 0, 0, 0, 0, 0, 0, 0
#define SCA1    CO|MI, CO|MI|HI, RO|HI|CEME, RO|BI|CEME, RO|MI|HI, BO|MI, RO|BI, EOFI|ES|EC|AI|CEME, IC, 0, 0, 0, 0, 0, 0, 0

#define JPR     CO|MI, CO|MI|HI, RO|HI|CEME, RO|BI|CEME, RO|MI|HI, BO|MI, RO|BI|CEME, RO|CI|HI, BO|CI, IC, 0, 0, 0, 0, 0, 0
#define LDR     CO|MI, CO|MI|HI, RO|HI|CEME, RO|BI|CEME, RO|MI|HI, BO|MI, RO|BI|CEME, RO|MI|HI, BO|MI, RO|AI, IC, 0, 0, 0, 0, 0
#define STR     CO|MI, CO|MI|HI, RO|HI|CEME, RO|BI|CEME, RO|MI|HI, BO|MI, RO|BI|CEME, RO|MI|HI, BO|MI, AO|RI, IC, 0, 0, 0, 0, 0
#define ADR     CO|MI, CO|MI|HI, RO|HI|CEME, RO|BI|CEME, RO|MI|HI, BO|MI, RO|BI|CEME, RO|MI|HI, BO|MI, RO|BI, EOFI|AI, IC, 0, 0, 0, 0
#define SBR     CO|MI, CO|MI|HI, RO|HI|CEME, RO|BI|CEME, RO|MI|HI, BO|MI, RO|BI|CEME, RO|MI|HI, BO|MI, RO|BI, EOFI|ES|EC|AI, IC, 0, 0, 0, 0
#define CPR     CO|MI, CO|MI|HI, RO|HI|CEME, RO|BI|CEME, RO|MI|HI, BO|MI, RO|BI|CEME, RO|MI|HI, BO|MI, RO|BI, EOFI|ES|EC, IC, 0, 0, 0, 0
#define ACR0    CO|MI, CO|MI|HI, RO|HI|CEME, RO|BI|CEME, RO|MI|HI, BO|MI, RO|BI|CEME, RO|MI|HI, BO|MI, RO|BI, EOFI|AI, IC, 0, 0, 0, 0
#define ACR1    CO|MI, CO|MI|HI, RO|HI|CEME, RO|BI|CEME, RO|MI|HI, BO|MI, RO|BI|CEME, RO|MI|HI, BO|MI, RO|BI, EOFI|EC|AI, IC, 0, 0, 0, 0
#define SCR0    CO|MI, CO|MI|HI, RO|HI|CEME, RO|BI|CEME, RO|MI|HI, BO|MI, RO|BI|CEME, RO|MI|HI, BO|MI, RO|BI, EOFI|ES|AI, IC, 0, 0, 0, 0
#define SCR1    CO|MI, CO|MI|HI, RO|HI|CEME, RO|BI|CEME, RO|MI|HI, BO|MI, RO|BI|CEME, RO|MI|HI, BO|MI, RO|BI, EOFI|ES|EC|AI, IC, 0, 0, 0, 0

#define CLB     CO|MI, CO|MI|HI, RO|HI|CEME, RO|BI|CEME, RO|MI|HI, BO|MI|AI, EOFI|ES|EC|RI, EOFI|ES|EC|AI|CEME, IC, 0, 0, 0, 0, 0, 0, 0
#define NEB     CO|MI, CO|MI|HI, RO|HI|CEME, RO|BI|CEME, RO|MI|HI, BO|MI, RO|AI|BI, EOFI|ES|EC|AI, EOFI|ES|EC|RI, EOFI|ES|EC|AI|CEME, IC, 0, 0, 0, 0, 0
#define INB     CO|MI, CO|MI|HI, RO|HI|CEME, RO|BI|CEME, RO|MI|HI, BO|MI, RO|AI|BI, EOFI|ES|EC|BI, EOFI|EC|RI, EOFI|EC|AI|CEME, IC, 0, 0, 0, 0, 0
#define DEB     CO|MI, CO|MI|HI, RO|HI|CEME, RO|BI|CEME, RO|MI|HI, BO|MI, RO|AI|BI, EOFI|ES|EC|BI, EOFI|ES|RI, EOFI|ES|AI|CEME, IC, 0, 0, 0, 0, 0
#define ADB     CO|MI, CO|MI|HI, RO|HI|CEME, RO|BI|CEME, RO|MI|HI, BO|MI, RO|BI, EOFI|RI, CEME, IC, 0, 0, 0, 0, 0, 0
#define SBB     CO|MI, CO|MI|HI, RO|HI|CEME, RO|BI|CEME, RO|MI|HI, BO|MI, AO|BI, RO|AI, EOFI|ES|EC|RI, BO|AI|CEME, IC, 0, 0, 0, 0, 0
#define ACB0    CO|MI, CO|MI|HI, RO|HI|CEME, RO|BI|CEME, RO|MI|HI, BO|MI, RO|BI, EOFI|AI,    AO|RI, CEME, IC, 0, 0, 0, 0, 0
#define ACB1    CO|MI, CO|MI|HI, RO|HI|CEME, RO|BI|CEME, RO|MI|HI, BO|MI, RO|BI, EOFI|EC|AI, AO|RI, CEME, IC, 0, 0, 0, 0, 0
#define SCB0    CO|MI, CO|MI|HI, RO|HI|CEME, RO|BI|CEME, RO|MI|HI, BO|MI, AO|BI, RO|AI, EOFI|ES|AI,    AO|RI, BO|AI|CEME, IC, 0, 0, 0, 0
#define SCB1    CO|MI, CO|MI|HI, RO|HI|CEME, RO|BI|CEME, RO|MI|HI, BO|MI, AO|BI, RO|AI, EOFI|ES|EC|AI, AO|RI, BO|AI|CEME, IC, 0, 0, 0, 0

#define CLW     CO|MI, CO|MI|HI, RO|HI|CEME, RO|BI|CEME, RO|MI|HI, BO|MI, AO|BI,    EOFI|ES|EC|RI, CEME, EOFI|ES|EC|RI, IC, 0, 0, 0, 0, 0
#define NEW0    CO|MI, CO|MI|HI, RO|HI|CEME, RO|BI|CEME, RO|MI|HI, BO|MI, RO|AI|BI, EOFI|ES|EC|AI, EOFI|ES|EC|RI, CEME, RO|BI, EOFI|ES|AI,     AO|RI, IC, 0, 0
#define NEW1    CO|MI, CO|MI|HI, RO|HI|CEME, RO|BI|CEME, RO|MI|HI, BO|MI, RO|AI|BI, EOFI|ES|EC|AI, EOFI|ES|EC|RI, CEME, RO|BI, EOFI|ES|EC|AI, AO|RI, IC, 0, 0
#define INW0    CO|MI, CO|MI|HI, RO|HI|CEME, RO|BI|CEME, RO|MI|HI, BO|MI, RO|AI|BI, EOFI|ES|EC|BI, EOFI|EC|RI, CEME|BI, RO|AI, EOFI|ES|AI,     AO|RI, IC, 0, 0
#define INW1    CO|MI, CO|MI|HI, RO|HI|CEME, RO|BI|CEME, RO|MI|HI, BO|MI, RO|AI|BI, EOFI|ES|EC|BI, EOFI|EC|RI, CEME|BI, RO|AI, EOFI|ES|EC|AI, AO|RI, IC, 0, 0
#define DEW0    CO|MI, CO|MI|HI, RO|HI|CEME, RO|BI|CEME, RO|MI|HI, BO|MI, RO|AI|BI, EOFI|ES|EC|BI, EOFI|ES|RI, CEME|BI, RO|AI, EOFI|AI,       AO|RI, IC, 0, 0
#define DEW1    CO|MI, CO|MI|HI, RO|HI|CEME, RO|BI|CEME, RO|MI|HI, BO|MI, RO|AI|BI, EOFI|ES|EC|BI, EOFI|ES|RI, CEME|BI, RO|AI, EOFI|EC|AI,     AO|RI, IC, 0, 0
#define ADW0    CO|MI, CO|MI|HI, RO|HI|CEME, RO|BI|CEME, RO|MI|HI, BO|MI, RO|BI,    EOFI|RI, CEME|BI, RO|AI, EOFI|ES|AI,    AO|RI, IC, 0, 0, 0
#define ADW1    CO|MI, CO|MI|HI, RO|HI|CEME, RO|BI|CEME, RO|MI|HI, BO|MI, RO|BI,    EOFI|RI, CEME|BI, RO|AI, EOFI|ES|EC|AI, AO|RI, IC, 0, 0, 0
#define SBW0    CO|MI, CO|MI|HI, RO|HI|CEME, RO|BI|CEME, RO|MI|HI, BO|MI, AO|BI,    RO|AI, EOFI|ES|EC|RI, CEME|BI, RO|AI, EOFI|AI,    AO|RI, IC, 0, 0
#define SBW1    CO|MI, CO|MI|HI, RO|HI|CEME, RO|BI|CEME, RO|MI|HI, BO|MI, AO|BI,    RO|AI, EOFI|ES|EC|RI, CEME|BI, RO|AI, EOFI|EC|AI, AO|RI, IC, 0, 0
#define ACW0    CO|MI, CO|MI|HI, RO|HI|CEME, RO|BI|CEME, RO|MI|HI, BO|MI, RO|BI,    EOFI|BI,    BO|RI, CEME|BI, RO|AI, EOFI|ES|AI,    AO|RI, IC, 0, 0
#define ACW1    CO|MI, CO|MI|HI, RO|HI|CEME, RO|BI|CEME, RO|MI|HI, BO|MI, RO|BI,    EOFI|EC|BI, BO|RI, CEME|BI, RO|AI, EOFI|ES|EC|AI, AO|RI, IC, 0, 0
#define SCW0    CO|MI, CO|MI|HI, RO|HI|CEME, RO|BI|CEME, RO|MI|HI, BO|MI, AO|BI,    RO|AI, EOFI|ES|BI,    BO|RI, CEME|BI, RO|AI, EOFI|AI,     AO|RI, IC, 0
#define SCW1    CO|MI, CO|MI|HI, RO|HI|CEME, RO|BI|CEME, RO|MI|HI, BO|MI, AO|BI,    RO|AI, EOFI|ES|EC|BI, BO|RI, CEME|BI, RO|AI, EOFI|EC|AI, AO|RI, IC, 0

#define LDS     CO|MI, CO|MI|HI, RO|HI|CEME, RO|BI|CEME, MI|HI, MI, RO|AI, EOFI|MI, RO|AI, IC, 0, 0, 0, 0, 0, 0
#define STS     CO|MI, CO|MI|HI, RO|HI|CEME, RO|BI|CEME, MI|HI, MI, RO|MI, AO|RI, MI, RO|AI, EOFI|BI, MI, RO|MI, RO|AI, BO|MI, AO|RI
#define PHS     CO|MI, CO|MI|HI, RO|HI|CEME, MI|HI, MI, RO|MI|BI, AO|RI, BO|AI,  EOFI|ES|BI|MI, EOFI|RI, AO|MI, RO|AI, IC, 0, 0, 0
#define PLS     CO|MI, CO|MI|HI, RO|HI|CEME, MI|HI, MI|BI, RO|AI, EOFI|ES|EC|AI, AO|RI, AO|MI, RO|AI, IC, 0, 0, 0, 0, 0
#define JPS     CO|MI, CO|MI|HI, RO|HI|CEME, MI|HI, MI|BI, RO|AI|MI, CO|RI, EOFI|AI|MI, CO|RI|HI, BO|MI, EOFI|RI, CO|MI, CO|MI|HI, RO|BI|CEME, RO|CI|HI, BO|CI
#define RTS     CO|MI, CO|MI|HI, RO|HI|CEME, MI|HI, MI|BI, RO|AI|MI, EOFI|ES|EC|AI|MI, RO|CI|HI, EOFI|ES|EC|AI|MI, RO|CI, BO|MI, AO|RI, CEME, CEME, IC, 0

#define BRA     CO|MI, CO|MI|HI, RO|HI|CEME, RO|BI|CEME, RO|CI|HI, BO|CI, IC, 0, 0, 0, 0, 0, 0, 0, 0, 0    // branching
#define ___     CO|MI, CO|MI|HI, RO|HI|CEME, CEME, CEME, IC, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0                  // non-branching

uint16_t mEEPROM[8192]    // microcode depending on flags, opcode and stepcounter
{
  /*NCZ    target: A, operand: none                                                       target: A, operand: immediate      target: A, operand:  byte at abs address    target: A, operand:  byte at rel address    target: byte at abs address, operand: A      target: word at abs address, operand: A          stack operations                BNE  BEQ  BCC  BCS  BPL  BMI */
  /*---*/  NOP, BNK, OUT, CLC, SEC, LSL, ROL0, LSR0, ROR0, ASR00x, INP,  NEG, Inc, Dec,   LDI, ADI, SBI, CPI, ACI0, SCI0,    JPA, LDA, STA, ADA, SBA, CPA, ACA0, SCA0,   JPR, LDR, STR, ADR, SBR, CPR, ACR0, SCR0,   CLB, NEB, INB, DEB, ADB, SBB, ACB0, SCB0,    CLW, NEW0, INW0, DEW0, ADW0, SBW0, ACW0, SCW0,   LDS, STS, PHS, PLS, JPS, RTS,   BRA, ___, BRA, ___, BRA, ___,
  /*--1*/  NOP, BNK, OUT, CLC, SEC, LSL, ROL0, LSR0, ROR0, ASR00x, INP,  NEG, Inc, Dec,   LDI, ADI, SBI, CPI, ACI0, SCI0,    JPA, LDA, STA, ADA, SBA, CPA, ACA0, SCA0,   JPR, LDR, STR, ADR, SBR, CPR, ACR0, SCR0,   CLB, NEB, INB, DEB, ADB, SBB, ACB0, SCB0,    CLW, NEW0, INW0, DEW0, ADW0, SBW0, ACW0, SCW0,   LDS, STS, PHS, PLS, JPS, RTS,   ___, BRA, BRA, ___, BRA, ___,
  /*-1-*/  NOP, BNK, OUT, CLC, SEC, LSL, ROL1, LSR1, ROR1, ASR01x, INP,  NEG, Inc, Dec,   LDI, ADI, SBI, CPI, ACI1, SCI1,    JPA, LDA, STA, ADA, SBA, CPA, ACA1, SCA1,   JPR, LDR, STR, ADR, SBR, CPR, ACR1, SCR1,   CLB, NEB, INB, DEB, ADB, SBB, ACB1, SCB1,    CLW, NEW1, INW1, DEW1, ADW1, SBW1, ACW1, SCW1,   LDS, STS, PHS, PLS, JPS, RTS,   BRA, ___, ___, BRA, BRA, ___,
  /*-11*/  NOP, BNK, OUT, CLC, SEC, LSL, ROL1, LSR1, ROR1, ASR01x, INP,  NEG, Inc, Dec,   LDI, ADI, SBI, CPI, ACI1, SCI1,    JPA, LDA, STA, ADA, SBA, CPA, ACA1, SCA1,   JPR, LDR, STR, ADR, SBR, CPR, ACR1, SCR1,   CLB, NEB, INB, DEB, ADB, SBB, ACB1, SCB1,    CLW, NEW1, INW1, DEW1, ADW1, SBW1, ACW1, SCW1,   LDS, STS, PHS, PLS, JPS, RTS,   ___, BRA, ___, BRA, BRA, ___,
  /*1--*/  NOP, BNK, OUT, CLC, SEC, LSL, ROL0, LSR0, ROR0, ASR10x, INP,  NEG, Inc, Dec,   LDI, ADI, SBI, CPI, ACI0, SCI0,    JPA, LDA, STA, ADA, SBA, CPA, ACA0, SCA0,   JPR, LDR, STR, ADR, SBR, CPR, ACR0, SCR0,   CLB, NEB, INB, DEB, ADB, SBB, ACB0, SCB0,    CLW, NEW0, INW0, DEW0, ADW0, SBW0, ACW0, SCW0,   LDS, STS, PHS, PLS, JPS, RTS,   BRA, ___, BRA, ___, ___, BRA,
  /*1-1*/  NOP, BNK, OUT, CLC, SEC, LSL, ROL0, LSR0, ROR0, ASR10x, INP,  NEG, Inc, Dec,   LDI, ADI, SBI, CPI, ACI0, SCI0,    JPA, LDA, STA, ADA, SBA, CPA, ACA0, SCA0,   JPR, LDR, STR, ADR, SBR, CPR, ACR0, SCR0,   CLB, NEB, INB, DEB, ADB, SBB, ACB0, SCB0,    CLW, NEW0, INW0, DEW0, ADW0, SBW0, ACW0, SCW0,   LDS, STS, PHS, PLS, JPS, RTS,   ___, BRA, BRA, ___, ___, BRA,
  /*11-*/  NOP, BNK, OUT, CLC, SEC, LSL, ROL1, LSR1, ROR1, ASR11x, INP,  NEG, Inc, Dec,   LDI, ADI, SBI, CPI, ACI1, SCI1,    JPA, LDA, STA, ADA, SBA, CPA, ACA1, SCA1,   JPR, LDR, STR, ADR, SBR, CPR, ACR1, SCR1,   CLB, NEB, INB, DEB, ADB, SBB, ACB1, SCB1,    CLW, NEW1, INW1, DEW1, ADW1, SBW1, ACW1, SCW1,   LDS, STS, PHS, PLS, JPS, RTS,   BRA, ___, ___, BRA, ___, BRA,
  /*111*/  NOP, BNK, OUT, CLC, SEC, LSL, ROL1, LSR1, ROR1, ASR11x, INP,  NEG, Inc, Dec,   LDI, ADI, SBI, CPI, ACI1, SCI1,    JPA, LDA, STA, ADA, SBA, CPA, ACA1, SCA1,   JPR, LDR, STR, ADR, SBR, CPR, ACR1, SCR1,   CLB, NEB, INB, DEB, ADB, SBB, ACB1, SCB1,    CLW, NEW1, INW1, DEW1, ADW1, SBW1, ACW1, SCW1,   LDS, STS, PHS, PLS, JPS, RTS,   ___, BRA, ___, BRA, ___, BRA,
};

/* End of snippet by Carsten Herting */

/* instruction must be 6 bits */
/* step must be 4 bits */
uint16_t GetMicrocodeWord(uint8_t instruction, uint8_t N, uint8_t C, uint8_t Z, uint8_t step)
{
    return mEEPROM[(N << 12) | (C << 11) | (Z << 10) | (instruction << 4) | (step << 0)];
}

typedef uint64_t clk_t;

struct Clock
{
    clk_t rate;
    clk_t clocks;

    Clock(clk_t rate) :
        rate(rate),
        clocks(0)
    {}

    Clock(const Clock& clock) :
        rate(clock.rate), 
        clocks(clock.clocks)
    {}

    Clock(const Clock& clock, clk_t newClocks) :
        rate(clock.rate), 
        clocks(newClocks)
    {}

    Clock& operator=(const Clock& clock)
    {
        clocks = clock.clocks;
        rate = clock.rate;
        return *this;
    }

    Clock& operator+=(clk_t inc)
    {
        clocks += inc;
        return *this;
    }

    Clock operator+(clk_t inc) const
    {
        return Clock(rate, clocks + inc);
    }

    // operator clk_t() const { return clocks; }
};

constexpr int UIUpdateFrequency = 30;

uint16_t u16from2xu8(uint8_t hi, uint8_t lo)
{
    return hi << 8 | lo;
}

struct Block
{
    std::string name;
    Block(const std::string& name) :
        name(name)
    {}
    // Evaluate block logic using inputs, return true if any of the
    // internal state or outputs of this block changed
    virtual bool Evaluate() = 0;
};

/* XXX should make a struct so it can have a std::string name for debugging and tracing */
typedef bool Wire;

template <int SIZE> struct Buffer;

// XXX should probably be constructed with tied-high / tied-low / floating state so
// has the correct values when not asserted from some Block.  But then would need some
// kind of event to de-assert from Blocks (part of Evaluate on clock falling?) and a
// method on Bus to un-drive in order to re-assert the tied-high state.
template <int SIZE>
struct Bus: public std::array<Wire, SIZE>
{
    std::string name;
    Bus(const std::string& name) :
        name(name)
    {}
    template <int OSIZE>
    const Bus<SIZE>& operator =(const Buffer<OSIZE>& input)
    {
        for(size_t i = 0; i < std::min(input.size(), this->size()); i++) {
            (*this)[i] = input[i];
        }
        if(this->size() > input.size()) {
            for(size_t i = input.size(); i < this->size(); i++) {
                (*this)[i] = 0;
            }
        }
        return *this;
    }
    operator uint32_t ()
    {
        uint32_t v = 0;
        for(size_t i = 0; i < SIZE; i++) {
            v = v | (this->at(i) ? (1 << i) : 0);
        }
        return v;
    }
    Bus<SIZE>& operator =(uint32_t v)
    {
        for(size_t i = 0; i < SIZE; i++) {
            (*this)[i] = v & (1 << i);
        }
        return *this;
    }
};

template <int SIZE>
struct Buffer : public std::array<Wire, SIZE>
{
    std::string name;
    Buffer(const std::string& name) :
        name(name)
    {}
    Buffer<SIZE>& operator =(uint32_t v)
    {
        for(size_t i = 0; i < SIZE; i++) {
            (*this)[i] = v & (1 << i);
        }
        return *this;
    }
    operator uint32_t ()
    {
        uint32_t v = 0;
        for(size_t i = 0; i < SIZE; i++) {
            v = v | (this->at(i) ? (1 << i) : 0);
        }
        return v;
    }
    template <int OSIZE>
    const Buffer<SIZE>& operator =(const Bus<OSIZE>& input)
    {
        for(size_t i = 0; i < std::min(input.size(), this->size()); i++) {
            (*this)[i] = input[i];
        }
        if(this->size() > input.size()) {
            for(size_t i = input.size(); i < this->size(); i++) {
                (*this)[i] = 0;
            }
        }
        return *this;
    }
};

template <int SIZE, typename InputBus, typename OutputBus>
struct Register : public Block
{
    Wire& reset;
    Wire& clock;
    Wire& input_enable;
    Wire& output_enable;
    InputBus& input;
    std::vector<OutputBus*> outputs;
    Buffer<SIZE> value;

    Register<SIZE, InputBus, OutputBus>& operator=(uint32_t v)
    {
        value = v;
        return *this;
    }

    Register(const std::string& name, Wire& reset, Wire& clock, Wire& input_enable, Wire& output_enable, InputBus& input, std::vector<OutputBus*>outputs) :
        Block(name),
        reset(reset),
        clock(clock),
        input_enable(input_enable),
        output_enable(output_enable),
        input(input),
        outputs(outputs),
        value(name + "-value")
    {
    }
    virtual bool Evaluate()
    {
        bool changed = false;
        if(reset) {
            changed = value != 0;
            value = 0;
        } else {
            if(input_enable) {
                auto old = value;
                value = input;
                changed = value != old;
                // printf("%s input enable, value now 0x%x\n", this->name.c_str(), (uint32_t)value);
            }
        }
        if(output_enable) {
            // printf("%s output enable\n", this->name.c_str());
            if(clock) {
                // printf("%s clock enable\n", this->name.c_str());
                for(auto* output: outputs) {
                    auto old = *output;
                    changed = changed || (*output != value);
                    *output = value;
                    // printf("%s value is 0x%x, output is 0x%x\n", this->name.c_str(), (uint32_t)value, (uint32_t)*output);
                }
            }
            return changed;
        } else {
            return false;
        }
    }
};

template <int SIZE, typename InputBus, typename OutputBus>
struct RegisterWithTap : public Register<SIZE, InputBus, OutputBus>
{
    Bus<SIZE>& tap;

    RegisterWithTap(const std::string& name, Wire& reset, Wire& clock, Wire& input_enable, Wire& output_enable, InputBus& input, std::vector<OutputBus*>outputs, Bus<SIZE>& tap) :
        Register<SIZE, InputBus, OutputBus>(name, reset, clock, input_enable, output_enable, input, outputs),
        tap(tap)
    { }

    RegisterWithTap<SIZE, InputBus, OutputBus>& operator=(uint32_t v)
    {
        this->value = v;
        return *this;
    }

    virtual bool Evaluate()
    {
        bool changed = Register<SIZE, InputBus, OutputBus>::Evaluate();
        changed = changed || (this->value != tap);
        tap = this->value;
        return changed;
    }
};

struct Or : public Block
{
    Wire& A;
    Wire& B;
    Wire& Out;
    bool oldOut;
    Or(const std::string& name, Wire& A, Wire& B, Wire& Out) :
        Block(name),
        A(A),
        B(B),
        Out(Out)
    {}
    virtual bool Evaluate()
    {
        oldOut = Out;
        Out = A || B;
        return Out != oldOut;
    }
};

template <int SIZE, typename InputBus, typename OutputBus>
struct Counter : public Register<SIZE, InputBus, OutputBus>
{
    Wire& load;
    Wire& increment;
    Wire& carry;
    bool oldclock = false;

    Counter(const std::string& name, Wire& reset, Wire& clock, Wire& increment, Wire& load, Wire& output_enable, InputBus& input, std::vector<OutputBus*> outputs, Wire& carry) :
        Register<SIZE, InputBus, OutputBus>(name, reset, clock, load, output_enable, input, outputs),
        load(load),
        increment(increment),
        carry(carry)
    {
    }
    virtual bool Evaluate()
    {
        bool changed = false;
        if(this->reset) {
            changed = this->value != 0;
            this->value = 0;
        } else {
            if(load) {
                changed = this->value != this->input;
                this->value = this->input;
            } else {
                if(!oldclock && this->clock & increment) {
                    changed = true;
                    carry = (this->value + 1 >= (1 << SIZE));
                    this->value = this->value + 1;
                }
            }
        }
        if(this->clock && this->output_enable) {
            for(auto* output: this->outputs) {
                auto old = *output;
                // printf("old output value = 0x%x\n", (uint32_t) old);
                changed = changed || (*output != this->value);
                *output = this->value;
                // printf("new output value = 0x%x\n", (uint32_t) *output);
            }
        }
        oldclock = this->clock;
        return changed;
    }
};

struct ConsoleIO : public Block
{
    Wire& clock;
    Wire& input_enable;
    Wire& output_enable;
    Bus<8>& input;
    std::vector<Bus<8>*> outputs;
    bool oldClock = false;
    std::queue<uint8_t> inputBuffer;

    ConsoleIO(const std::string& name, Wire& clock, Wire& input_enable, Wire& output_enable, Bus<8>& input, std::vector<Bus<8>*> outputs) :
        Block(name),
        clock(clock),
        input_enable(input_enable),
        output_enable(output_enable),
        input(input),
        outputs(outputs)
    {}
    virtual bool Evaluate()
    {
        bool changed = false;
        if(!oldClock && clock && input_enable) {
            // printf("%s input enabled; input = 0x%x\n", this->name.c_str(), (uint32_t)input);
            putchar(input);
            changed = true;
        }
        if(!oldClock && clock && output_enable) {
            uint8_t value = 0xFF;
            if(!inputBuffer.empty()) {
                value = inputBuffer.front();
                // printf("requested UART; returned 0x%X from the buffer; XXX implement\n", value);
                inputBuffer.pop();
                changed = true;
            } else {
                // printf("requested UART; nothing available, returning 0xFF\n");
            }
            for(auto* output: this->outputs) {
                auto old = *output;
                *output = value;
                // printf("%s output enable, write 0x%x\n", this->name.c_str(), value);
                changed = changed || (old != value);
            }
        }
        oldClock = clock;
        return changed;
    }
};

struct Adder : public Block
{
    Wire& ES;
    Wire& EC;
    Bus<8>& FromA;
    Bus<8>& FromB;
    Wire& EO;
    Bus<8>& ResultOut;
    Bus<3>& FlagsOut;
    Buffer<8> value;

    Adder(const std::string& name, Wire &clock, Wire& ES, Wire& EC, Bus<8>& FromA, Bus<8>& FromB, Wire& EO, Bus<8>& ResultOut, Bus<3>& FlagsOut) :
        Block(name),
        ES(ES),
        EC(EC),
        FromA(FromA),
        FromB(FromB),
        EO(EO),
        ResultOut(ResultOut),
        FlagsOut(FlagsOut),
        value(name + "-name")
    {}
    virtual bool Evaluate()
    {
        bool changed = false;
        uint8_t A = FromA;
        uint8_t B = ES ? ~FromB : FromB;
        uint32_t carry = EC ? 1 : 0;
        uint32_t result = carry + A + B;
        uint8_t N = (result & 0x80) ? 1 : 0;
        uint8_t C = (result > 0xFF) ? 1 : 0;
        uint8_t Z = ((result & 0xFF) == 0) ? 1 : 0;
        uint8_t flags = (N << 2) | (C << 1) | (Z << 0);
        // printf("%s: 0x%x+0x%x+0x%x yielding 0x%x, flags 0x%x\n", this->name.c_str(), carry, A, B, result & 0xFF, flags);
        changed = changed || (flags != FlagsOut);
        FlagsOut = (N << 2) | (C << 1) | (Z << 0);
        if(EO) {
            uint8_t value = result & 0xFF;
            auto old = ResultOut;
            ResultOut = value;
            // printf("%s output enable, write 0x%x\n", this->name.c_str(), value);
            changed = changed || (old != value);
        }
        return changed;
    }
};

struct RAMAndFlash : public Block
{
    Wire& reset;
    Wire& clock;
    Wire& input_enable;
    Wire& output_enable;
    Bus<8>& memory_address_low;
    Bus<8>& memory_address_high;
    Bus<4>& bank;
    Bus<8>& input;
    std::vector<Bus<8>*> outputs;
    std::array<uint8_t, RAMSize> RAM;
    std::array<uint8_t, FlashSize> Flash;

    RAMAndFlash(const std::string& name, Wire& reset, Wire &clock, Wire& input_enable, Wire& output_enable, Bus<8>& memory_address_low, Bus<8>& memory_address_high, Bus<4>& bank, Bus<8>& input, std::vector<Bus<8>*> outputs) :
        Block(name),
        reset(reset),
        clock(clock),
        input_enable(input_enable),
        output_enable(output_enable),
        memory_address_low(memory_address_low),
        memory_address_high(memory_address_high),
        bank(bank),
        input(input),
        outputs(outputs)
    {}
    virtual bool Evaluate()
    {
        bool changed = false;
        uint16_t is_ram = memory_address_high & 0x80;
        uint16_t ramaddress = ((memory_address_high & 0x7F) << 8) | (memory_address_low);
        uint32_t flashaddress = (bank << 11) | ((memory_address_high & 0x7F) << 8) | (memory_address_low);
        if(input_enable) {
            // printf("%s input enabled; is_ram = %d, MA = 0x%x, ramaddress = 0x%x, flashaddress = 0x%x\n", this->name.c_str(), is_ram ? 1 : 0, (memory_address_high << 8) | (memory_address_low), ramaddress, flashaddress);
            if(is_ram) {
                // printf("%s input enable, write 0x%x to RAM 0x%x\n", this->name.c_str(), (uint32_t)input, ramaddress);
                changed = RAM[ramaddress] != input;
                RAM[ramaddress] = input;
            } else {
                // printf("%s input enable, write 0x%x to Flash 0x%x\n", this->name.c_str(), (uint32_t)input, ramaddress);
                changed = Flash[flashaddress] != input;
                Flash[flashaddress] = input;
            }
        }
        if(output_enable) {
            uint8_t value;
            if(!is_ram) {
                value = Flash[flashaddress];
                // printf("%s output enable, read 0x%x from Flash 0x%x\n", this->name.c_str(), value, flashaddress);
            } else {
                value = RAM[ramaddress];
                // printf("%s output enable, read 0x%x from RAM 0x%x\n", this->name.c_str(), value, ramaddress);
            }
            for(auto* output: this->outputs) {
                auto old = *output;
                *output = value;
                // printf("%s output enable, write 0x%x\n", this->name.c_str(), value);
                changed = changed || (old != value);
            }
        }
        return changed;
    }
};

struct ControlROM : public Block
{
    Bus<3>& flags;
    Bus<6>& instruction;
    Bus<4>& step;
    Wire& CISignal;
    Wire& COSignal;
    Wire& CEMESignal;
    Wire& TRSignal;
    Wire& ICSignal;
    Wire& ECSignal;
    Wire& ESSignal;
    Wire& EOFISignal;
    Wire& HISignal;
    Wire& MISignal;
    Wire& RISignal;
    Wire& ROSignal;
    Wire& AISignal;
    Wire& AOSignal;
    Wire& BISignal;
    Wire& BOSignal;

    bool disableForDebug = false;

    uint32_t oldromaddress = 0;
    ControlROM(const std::string& name, Bus<3>& flags, Bus<6>& instruction, Bus<4>& step, Wire& CISignal, Wire& COSignal, Wire& CEMESignal, Wire& TRSignal, Wire& ICSignal, Wire& ECSignal, Wire& ESSignal, Wire& EOFISignal, Wire& HISignal, Wire& MISignal, Wire& RISignal, Wire& ROSignal, Wire& AISignal, Wire& AOSignal, Wire& BISignal, Wire& BOSignal) :
        Block(name),
        flags(flags),
        instruction(instruction),
        step(step),
        CISignal(CISignal),
        COSignal(COSignal),
        CEMESignal(CEMESignal),
        TRSignal(TRSignal),
        ICSignal(ICSignal),
        ECSignal(ECSignal),
        ESSignal(ESSignal),
        EOFISignal(EOFISignal),
        HISignal(HISignal),
        MISignal(MISignal),
        RISignal(RISignal),
        ROSignal(ROSignal),
        AISignal(AISignal),
        AOSignal(AOSignal),
        BISignal(BISignal),
        BOSignal(BOSignal)
    { }
    bool Evaluate()
    {
        if(disableForDebug) {
            return false;
        }

        uint32_t romaddress = (flags << 10) | (instruction << 4) | (step << 0);
        bool changed = romaddress != oldromaddress;
        oldromaddress = romaddress;
        uint16_t microcode_word = mEEPROM[romaddress];
        CISignal = microcode_word & CI;
        COSignal = microcode_word & CO;
        CEMESignal = microcode_word & CEME;
        TRSignal = microcode_word & TR;
        ICSignal = microcode_word & IC;
        ECSignal = microcode_word & EC;
        ESSignal = microcode_word & ES;
        EOFISignal = microcode_word & EOFI;
        HISignal = microcode_word & HI;
        MISignal = microcode_word & MI;
        RISignal = microcode_word & RI;
        ROSignal = microcode_word & RO;
        AISignal = microcode_word & AI;
        AOSignal = microcode_word & AO;
        BISignal = microcode_word & BI;
        BOSignal = microcode_word & BO;
        return changed;
    }
};

struct ControlLogic : public Block
{
    Wire& HISignal;
    Wire& CISignal;
    Wire& COSignal;
    Wire& MISignal;
    Wire& TRSignal;
    Wire& CEMESignal;
    Wire& ECSignal;
    Wire& cohSignal;
    Wire& colSignal;
    Wire& cihSignal;
    Wire& cilSignal;
    Wire& mihSignal;
    Wire& milSignal;
    Wire& tiSignal;
    Wire& toSignal;
    Wire& iiSignal;
    Wire& kiSignal;
    bool oldHISignal;
    bool oldCISignal;
    bool oldCOSignal;
    bool oldTRSignal;
    bool oldMISignal;
    bool oldCEMESignal;
    bool oldECSignal;
    ControlLogic(const std::string& name, Wire& HISignal, Wire& CISignal, Wire& COSignal, Wire& MISignal, Wire& TRSignal, Wire& CEMESignal, Wire& ECSignal, Wire& cohSignal, Wire& colSignal, Wire& cihSignal, Wire& cilSignal, Wire& mihSignal, Wire& milSignal, Wire& tiSignal, Wire& toSignal, Wire& iiSignal, Wire& kiSignal) :
        Block(name),
        HISignal(HISignal),
        CISignal(CISignal),
        COSignal(COSignal),
        MISignal(MISignal),
        TRSignal(TRSignal),
        CEMESignal(CEMESignal),
        ECSignal(ECSignal),
        cohSignal(cohSignal),
        colSignal(colSignal),
        cihSignal(cihSignal),
        cilSignal(cilSignal),
        mihSignal(mihSignal),
        milSignal(milSignal),
        tiSignal(tiSignal),
        toSignal(toSignal),
        iiSignal(iiSignal),
        kiSignal(kiSignal)
    {}
    bool Evaluate()
    {
        bool changed = false;
        changed = changed || (oldHISignal != HISignal); oldHISignal = HISignal;
        changed = changed || (oldCISignal != CISignal); oldCISignal = CISignal;
        changed = changed || (oldCOSignal != COSignal); oldCOSignal = COSignal;
        changed = changed || (oldMISignal != MISignal); oldMISignal = MISignal;
        changed = changed || (oldTRSignal != TRSignal); oldTRSignal = TRSignal;
        changed = changed || (oldCEMESignal != CEMESignal); oldCEMESignal = CEMESignal;
        changed = changed || (oldECSignal != ECSignal); oldECSignal = ECSignal;
        cihSignal = CISignal && HISignal;
        cilSignal = CISignal && ~HISignal;
        cohSignal = COSignal && HISignal;
        colSignal = COSignal && ~HISignal;
        mihSignal = MISignal && HISignal;
        milSignal = MISignal && ~HISignal;
        tiSignal = TRSignal && HISignal;
        toSignal = TRSignal && ~HISignal;
        iiSignal = CEMESignal && HISignal;
        kiSignal = ECSignal && HISignal;
        return changed;
    }
};

struct System
{
    Bus<8> MainBus{"MainBus"};
    Bus<8> AToAdder{"AToAdder"};
    Bus<8> BToAdder{"BToAdder"};
    Bus<8> PCLToMemory{"PCLToMemory"};
    Bus<8> PCHToMemory{"PCHToMemory"};
    Bus<8> MALToMemory{"MALToMemory"};
    Bus<8> MAHToMemory{"MAHToMemory"};
    Bus<4> BANKToMemory{"BANKToMemory"};
    Bus<3> AdderFlagsBus{"AdderFlagsBus"};
    Bus<3> FlagsToControlLogicBus{"FlagsToControlLogicBus"};
    Bus<6> InstructionToControlLogicBus{"InstructionToControlLogicBus"};
    Bus<4> StepToControlLogicBus{"StepToControlLogicBus"};
    Wire CISignal = false;
    Wire COSignal = false;
    Wire CEMESignal = false;
    Wire TRSignal = false;
    Wire ICSignal = false;
    Wire ECSignal = false;
    Wire ESSignal = false;
    Wire EOFISignal = false;
    Wire HISignal = false;
    Wire MISignal = false;
    Wire RISignal = false;
    Wire ROSignal = false;
    Wire AISignal = false;
    Wire AOSignal = false;
    Wire BISignal = false;
    Wire BOSignal = false;
    Wire cilSignal = false;
    Wire colSignal = false;
    Wire cihSignal = false;
    Wire cohSignal = false;
    Wire milSignal = false;
    Wire mihSignal = false;
    Wire kiSignal = false;
    Wire iiSignal = false;
    Wire tiSignal = false;
    Wire toSignal = false;

    Wire alwaysTrue = true;
    Wire alwaysFalse = false;
    Wire clock = false;
    Wire nclock = true;
    Wire reset = true;
    Bus<8> emptyBusForInputs{"emptyBusForInputs"};

    RegisterWithTap<8, Bus<8>, Bus<8>> ARegister{"ARegister", reset, clock, AISignal, AOSignal, MainBus, {&MainBus}, AToAdder};
    RegisterWithTap<8, Bus<8>, Bus<8>> BRegister{"BRegister", reset, clock, BISignal, BOSignal, MainBus, {&MainBus}, BToAdder};
    // XXX PC is supposed to be hooked to CEME for increment on clock
    Wire PCLcarry;
    Wire PCHcarry_discard;
    Counter<8, Bus<8>, Bus<8>> PCLRegister{"PCLRegister", reset, clock, CEMESignal, cilSignal, colSignal, MainBus, {&MainBus}, PCLcarry};
    Counter<8, Bus<8>, Bus<8>> PCHRegister{"PCHRegister", reset, clock, PCLcarry, cihSignal, cohSignal, MainBus, {&MainBus}, PCHcarry_discard};
    Register<8, Bus<8>, Bus<8>> MALRegister{"MALRegister", reset, clock, milSignal, alwaysTrue, MainBus, {&MALToMemory}};
    Register<8, Bus<8>, Bus<8>> MAHRegister{"MAHRegister", reset, clock, mihSignal, alwaysTrue, MainBus, {&MAHToMemory}};
    Register<4, Bus<8>, Bus<4>> BANKRegister{"BANKRegister", reset, clock, kiSignal, alwaysTrue, MainBus, {&BANKToMemory}};
    Register<3, Bus<3>, Bus<3>> FlagsRegister{"FlagsRegister", reset, clock, EOFISignal, alwaysTrue, AdderFlagsBus, {&FlagsToControlLogicBus}};
    Register<6, Bus<8>, Bus<6>> InstructionRegister{"InstructionRegister", reset, nclock, iiSignal, alwaysTrue, MainBus, {&InstructionToControlLogicBus}};

    Wire StepCounterReset;
    Or ICOrReset{"ICOrReset", ICSignal, reset, StepCounterReset};
    Wire carry_discarded;
    Counter<4, Bus<8>, Bus<4>> StepCounter{"StepCounter", StepCounterReset, nclock, nclock, alwaysFalse, alwaysTrue, emptyBusForInputs, {&StepToControlLogicBus}, carry_discarded};

    RAMAndFlash Memory{"Memory", reset, clock, RISignal, ROSignal, MALToMemory, MAHToMemory, BANKToMemory, MainBus, {&MainBus}};

    ConsoleIO UART{"UART", clock, tiSignal, toSignal, MainBus, {&MainBus}};

    Adder ALU{"ALU", clock, ESSignal, ECSignal, AToAdder, BToAdder, EOFISignal, MainBus, AdderFlagsBus};

    ControlROM MicrocodeROM{"MicrocodeROM", FlagsToControlLogicBus, InstructionToControlLogicBus, StepToControlLogicBus, CISignal, COSignal, CEMESignal, TRSignal, ICSignal, ECSignal, ESSignal, EOFISignal, HISignal, MISignal, RISignal, ROSignal, AISignal, AOSignal, BISignal, BOSignal};

    ControlLogic Logic{"Logic", HISignal, CISignal, COSignal, MISignal, TRSignal, CEMESignal, ECSignal, cohSignal, colSignal, cihSignal, cilSignal, mihSignal, milSignal, tiSignal, toSignal, iiSignal, kiSignal};

    std::vector<Block*> blocks = {&ICOrReset, &ARegister, &BRegister, &PCLRegister, &PCHRegister, &MALRegister, &MAHRegister, &BANKRegister, &FlagsRegister, &InstructionRegister, &StepCounter, &Memory, &UART, &ALU, &MicrocodeROM, &Logic};

    System()
    {
    }

    void Step()
    {
        bool changed;

        MainBus = 0xFF; // XXX this should be part of bus state? - tied high, tied low, floats?
        clock = true;
        nclock = !clock;
        int cycles = 0;
        do {
            if(cycles >= QuiescentEvaluateMaxCycles) {
                throw std::runtime_error(std::string("Step: exceeded maximum number of cycles to achieve quiescence with clock high"));
            }
            if(debug) printf("    clock high in loop:\n");
            changed = false;
            for(auto* b : blocks) {
                bool block_changed = b->Evaluate();
                changed = changed || block_changed;
                if(debug) printf("        %s output %s\n", b->name.c_str(), block_changed ? "changed" : "did not change");
            }
            if(debug) printf("        ARegister = 0x%x:\n", (uint32_t)ARegister.value);
            if(debug) printf("        BRegister = 0x%x:\n", (uint32_t)BRegister.value);
            if(debug) printf("        MainBus = 0x%x:\n", (uint32_t)MainBus);
            cycles++;
        } while(changed);

        clock = false;
        nclock = !clock;
        cycles = 0;
        do {
            if(cycles >= QuiescentEvaluateMaxCycles) {
                throw std::runtime_error(std::string("Step: exceeded maximum number of cycles to achieve quiescence with clock high"));
            }
            if(debug) printf("    clock low in loop:\n");
            changed = false;
            for(auto* b : blocks) {
                bool block_changed = b->Evaluate();
                changed = changed || block_changed;
                if(debug) printf("        %s output %s\n", b->name.c_str(), block_changed ? "changed" : "did not change");
            }
            if(debug) printf("        ARegister = 0x%x:\n", (uint32_t)ARegister.value);
            if(debug) printf("        BRegister = 0x%x:\n", (uint32_t)BRegister.value);
            if(debug) printf("        MainBus = 0x%x:\n", (uint32_t)MainBus);
            cycles++;
        } while(changed);
    }
};

void TestSystem()
{
    {
        System sys; sys.MicrocodeROM.disableForDebug = true;

        if(debug) printf("Reset test\n");
        sys.reset = true;
        sys.Step();
        assert((sys.StepCounter.value == 0) && "reset StepCounter");
        assert((sys.ARegister.value == 0) && "reset ARegister");
        assert((sys.BRegister.value == 0) && "reset BRegister");
        assert((sys.MainBus == 0xFF) && "MainBus unasserted is 0xFF");
    }

    {
        System sys; sys.MicrocodeROM.disableForDebug = true; sys.reset = true; sys.Step(); sys.reset = false;

        if(debug) printf("Clock test 1\n");
        sys.AOSignal = true;
        sys.BISignal = true;
        sys.ARegister.value = 0x5a;
        sys.BRegister.value = 0x00;
        sys.Step();
        assert((sys.BRegister.value == 0x5a) && "load BRegister");
        assert((sys.StepCounter.value == 1) && "increment StepCounter");
    }

    {
        System sys; sys.MicrocodeROM.disableForDebug = true; sys.reset = true; sys.Step(); sys.reset = false;

        if(debug) printf("PCL PCH test\n");

        sys.AOSignal = true;
        sys.ARegister.value = 0xca;
        sys.cihSignal = true;
        sys.Step();
        sys.cihSignal = false;
        assert((sys.PCHRegister.value == 0xca) && "load PCH");

        sys.AOSignal = true;
        sys.ARegister.value = 0xfe;
        sys.cilSignal = true;
        sys.Step();
        sys.cilSignal = false;
        sys.AOSignal = false;
        assert((sys.PCLRegister.value == 0xfe) && "load PCL");

        sys.PCHRegister.value = 0xda;
        sys.PCLRegister.value = 0xff;
        sys.CEMESignal = true;
        sys.Step();
        assert((sys.PCLRegister.value == 0x00) && "increment PC, PCL overflow");
        assert((sys.PCHRegister.value == 0xdb) && "increment PC, carry into PCH");
    }

    {
        System sys; sys.MicrocodeROM.disableForDebug = true; sys.reset = true; sys.Step(); sys.reset = false;

        if(debug) printf("MAH MAL BNK test\n");

        sys.AOSignal = true;
        sys.ARegister.value = 0xca;
        sys.mihSignal = true;
        sys.Step();
        sys.mihSignal = false;
        assert((sys.MAHRegister.value == 0xca) && "load MAH");

        sys.AOSignal = true;
        sys.ARegister.value = 0xfe;
        sys.milSignal = true;
        sys.Step();
        sys.milSignal = false;
        assert((sys.MALRegister.value == 0xfe) && "load MAL");

        sys.AOSignal = true;
        sys.ARegister.value = 0xff;
        sys.kiSignal = true;
        sys.Step();
        sys.kiSignal = false;
        assert((sys.BANKRegister.value == 0x0f) && "load BANKAH");
    }

    {
        System sys; sys.MicrocodeROM.disableForDebug = true; sys.reset = true; sys.Step(); sys.reset = false;

        if(debug) printf("RAM write test\n");

        sys.MAHRegister = 0xDE;
        sys.MALRegister = 0xAD;
        sys.Memory.RAM[0xDEAD & 0x7FFF] = 0x5A;
        sys.AOSignal = true;
        sys.ARegister.value = 0xBA;
        sys.RISignal = true;
        sys.Step();
        assert((sys.Memory.RAM[0xDEAD & 0x7FFF] == 0xBA) && "write RAM");
    }

    {
        System sys; sys.MicrocodeROM.disableForDebug = true; sys.reset = true; sys.Step(); sys.reset = false;

        if(debug) printf("Flash write test\n");

        sys.MAHRegister = 0x13;
        sys.MALRegister = 0x37;
        sys.BANKRegister = 0x5;
        sys.Memory.Flash[(0x5 << 11) | 0x1337] = 0x5a;
        sys.AOSignal = true;
        sys.ARegister.value = 0xCA;
        sys.RISignal = true;
        sys.Step();
        assert((sys.Memory.Flash[(0x5 << 11) | 0x1337] == 0xCA) && "write Flash");
    }

    {
        System sys; sys.MicrocodeROM.disableForDebug = true; sys.reset = true; sys.Step(); sys.reset = false;

        if(debug) printf("RAM read test\n");

        sys.MAHRegister = 0xDE;
        sys.MALRegister = 0xCA;
        sys.Memory.RAM[0xDECA & 0x7FFF] = 0xA5;
        sys.ROSignal = true;
        sys.Step();
        assert((sys.Memory.RAM[0xDECA & 0x7FFF] == 0xA5) && "read RAM");
    }

    {
        System sys; sys.MicrocodeROM.disableForDebug = true; sys.reset = true; sys.Step(); sys.reset = false;

        if(debug) printf("Flash read test\n");

        sys.MAHRegister = 0x06;
        sys.MALRegister = 0x66;
        sys.BANKRegister = 0x6;
        sys.Memory.Flash[(0x6 << 11) | 0x0666] = 0x3F;
        sys.ROSignal = true;
        sys.Step();
        assert((sys.Memory.Flash[(0x6 << 11) | 0x0666] == 0x3F) && "read Flash");
    }

    {
        System sys; sys.MicrocodeROM.disableForDebug = true; sys.reset = true; sys.Step(); sys.reset = false;

        if(debug) printf("UART write test\n");

        sys.AOSignal = true;
        sys.ARegister.value = '?';
        sys.tiSignal = true;
        sys.Step();
        if(debug) printf("    Should have printed a '?'\n");
    }

    {
        System sys; sys.MicrocodeROM.disableForDebug = true; sys.reset = true; sys.Step(); sys.reset = false;

        if(debug) printf("UART read test\n");

        sys.AISignal = true;
        sys.UART.inputBuffer.push('!');
        sys.toSignal = true;
        sys.Step();
        assert((sys.ARegister.value == '!') && "read UART");
        sys.Step();
        assert((sys.ARegister.value == 0xFF) && "read UART");
    }
    
    auto testALU = [&](const char* what, uint8_t A, uint8_t B, bool EC, bool ES, bool EOFI, uint8_t result, uint8_t flagsbus, uint8_t flagsreg)
    {
        System sys; sys.MicrocodeROM.disableForDebug = true; sys.reset = true; sys.Step(); sys.reset = false;

        if(debug) printf("ALU test %s\n", what);

        sys.ARegister = A;
        sys.BRegister = B;
        sys.ECSignal = EC;
        sys.ESSignal = ES;
        sys.EOFISignal = EOFI;
        sys.Step();
        assert(sys.MainBus == result);
        assert(sys.AdderFlagsBus == flagsbus);
        assert((uint32_t)sys.FlagsRegister.value == flagsreg);
    };
    testALU("0+0", 0, 0, false, false, true, 0, 1, 1);
    testALU("1+1", 1, 1, false, false, true, 2, 0, 0);
    testALU("63+0", 63, 0, false, false, true, 63, 0, 0);
    testALU("-128+1", 0x80, 0, false, false, true, 0x80, 4, 4);
    testALU("-128+1", 0x80, 1, false, false, true, 0x81, 4, 4);
    testALU("128+128 overflow", 0x80, 0x80, false, false, true, 0, 3, 3);
    testALU("invert 0x55", 0, 0x55, false, true, true, 0xAA, 4, 4);
    testALU("invert 0xFF", 0, 0xFF, false, true, true, 0x0, 1, 1);
    testALU("128+128 overflow, no EOFI", 0x80, 0x80, false, false, false, 0xFF, 3, 0);
    testALU("invert 0x55, no EOFI", 0, 0x55, false, true, false, 0xFF, 4, 0);
}

template <class MEMORY, class INTERFACE>
struct MinimalEmulator
{
    uint64_t cpuClockLengthInSystemClocks;
    Clock mostRecentSystemClock;

    enum StepResult {
        CONTINUE,
        EXIT,
    };

    MinimalEmulator(uint64_t CPUClockRate, const Clock& systemClock) :
        mostRecentSystemClock(systemClock)
    {
        assert(systemClock.rate % CPUClockRate == 0);
        cpuClockLengthInSystemClocks = systemClock.rate / CPUClockRate;
    }

    StepResult step(MEMORY& memory, INTERFACE& interface, const Clock& systemClock)
    {
        abort();
        return CONTINUE;
    }

    // Return the next system clock tick at which the CPU will have transitioned one CPU clock,
    // that is to say return the least clock for which the CPU has to do some work.
    clk_t calculateNextActivity()
    {
        clk_t next = (mostRecentSystemClock.clocks + cpuClockLengthInSystemClocks - 1) / cpuClockLengthInSystemClocks * cpuClockLengthInSystemClocks;
        // XXX debug printf("CPU next is %llu\n", next);
        return next;
    }

    // Do work associated with CPU clock transitioning to active,
    // after mostRecentSystemClock and up to and including systemClock.
    // Do not repeat work if called twice with same clock.
    StepResult updatePastClock(MEMORY& memory, INTERFACE& interface, const Clock& systemClock)
    {
        for(uint64_t clock = calculateNextActivity(); clock <= systemClock.clocks; clock += cpuClockLengthInSystemClocks) {
            StepResult result = step(memory, interface, Clock(systemClock, clock));
            if(result != CONTINUE) {
                return result;
            }
        }
        mostRecentSystemClock = systemClock + 1;
        // XXX debug printf("systemClock is %llu, most recent is now %llu\n", systemClock.clocks, mostRecentSystemClock.clocks);
        return CONTINUE;
    }
};

struct Memory
{
    std::array<uint8_t, FlashSize> flash;
    std::array<uint8_t, RAMSize> RAM;
    uint32_t bank = 0;
    bool succeeded = false;

    Memory(const std::string& flash_file)
    {
        FILE *fp = fopen(flash_file.c_str(), "rb");
        if(!fp) {
            throw "couldn't open " + flash_file;
        }
        fseek(fp, 0, SEEK_END);
        long size = ftell(fp);
        assert(size == FlashSize);
        fseek(fp, 0, SEEK_SET);
        fread(flash.data(), flash.size(), 1, fp);
        fclose(fp);
        succeeded = true;
    }

    void setBank(uint8_t bank_)
    {
        assert(bank_ < 16);
        bank = bank_;
    }

    bool read(uint16_t address, uint8_t& data)
    {
        if(address < 0x8000) {
            data = flash.at(bank * 0x8000 + address);
            return true;
        }
        data = RAM[address - 0x8000];
        return true;
    }
    bool write(uint16_t address, uint8_t data)
    {
        if(address >= 0x8000) {
            data = RAM[address - 0x8000] = data;
            return true;
        }
        return false;
    }
};

struct Interface
{
    bool succeeded = false;
    Clock mostRecentSystemClock;

    bool attemptIterate()
    {
        return true;
    }

    Interface(const Clock& systemClock) :
        mostRecentSystemClock(systemClock)
    {
        succeeded = true;
    }

    clk_t calculateNextActivity()
    {
        // clk_t next = (mostRecentSystemClock.clocks + audioOutputSampleLengthInSystemClocks - 1) / audioOutputSampleLengthInSystemClocks * audioOutputSampleLengthInSystemClocks;
        clk_t next = mostRecentSystemClock.clocks + 10000000;
        // XXX debug printf("interface next is %llu\n", next);
        return next;
    }

// Do not repeat work if called twice with same clock.

    void updatePastClock(const Clock& systemClock)
    {
    }
};

void usage(const char *name)
{
    fprintf(stderr, "usage: %s flash.bin\n", name);
    // fprintf(stderr, "usage: %s [options] flash.bin\n", name);
    // fprintf(stderr, "options:\n");
    // fprintf(stderr, "\t--rate N           - issue N instructions per 60Hz field\n");
    // --clock mhz
}

int main(int argc, char **argv)
{
    const char *progname = argv[0];
    argc -= 1;
    argv += 1;

    while((argc > 1) && (argv[0][0] == '-')) {
        if(
            (strcmp(argv[0], "-help") == 0) ||
            (strcmp(argv[0], "-h") == 0) ||
            (strcmp(argv[0], "-?") == 0))
        {
            usage(progname);
            exit(EXIT_SUCCESS);
	} else {
	    fprintf(stderr, "unknown parameter \"%s\"\n", argv[0]);
            usage(progname);
	    exit(EXIT_FAILURE);
	}
    }

    if(argc < 1) {
        usage(progname);
        exit(EXIT_FAILURE);
    }

    std::string flash_file = argv[0];

    Clock systemClock(SystemClockRate);

    Interface interface(systemClock);
    if(!interface.succeeded) {
        fprintf(stderr, "opening the user interface failed.\n");
        exit(EXIT_FAILURE);
    }

    Memory memory(flash_file);

    MinimalEmulator<Memory,Interface> minimal(CPUClockRate, systemClock);

    std::chrono::time_point<std::chrono::system_clock> interfaceThen = std::chrono::system_clock::now();

    if(true) {
        TestSystem();
        if(false) {
            exit(EXIT_FAILURE);
        }
    }

    System sys;
    {
        FILE *fp = fopen(flash_file.c_str(), "rb");
        if(!fp) {
            throw "couldn't open " + flash_file;
        }
        fseek(fp, 0, SEEK_END);
        long size = ftell(fp);
        assert(size == FlashSize);
        fseek(fp, 0, SEEK_SET);
        fread(sys.Memory.Flash.data(), sys.Memory.Flash.size(), 1, fp);
        fclose(fp);
    }
    while(1) {
        sys.Step();
    }

    printf("Power up.\n");
    bool done = false;
    while(!done) {

        uint64_t newClock = systemClock.clocks + systemClock.rate / 240; // XXX I dunno, 4 chunks of a 60Hz tick???
        while(systemClock.clocks < newClock) {
            uint64_t nextCPU = minimal.calculateNextActivity();
            uint64_t nextInterface = interface.calculateNextActivity();
            // XXX debug printf("cpu : %llu, interface: %llu\n", nextCPU, nextInterface);
            if(nextCPU < nextInterface) {
                // XXX debug printf("do cpu\n");
                MinimalEmulator<Memory,Interface>::StepResult result = minimal.updatePastClock(memory, interface, systemClock);
                if(result != MinimalEmulator<Memory,Interface>::CONTINUE) {
                    // XXX debug printf("exit on unsupported instruction\n");
                    exit(EXIT_SUCCESS);
                }
                systemClock.clocks = nextCPU;
            } else {
                // XXX debug printf("do interface\n");
                interface.updatePastClock(systemClock);
                systemClock.clocks = nextInterface;
            }
        }

        std::chrono::time_point<std::chrono::system_clock> interfaceNow = std::chrono::system_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::duration<float>>(interfaceNow - interfaceThen);
        float dt = elapsed.count();
        if(dt > (.9f * 1.0f / UIUpdateFrequency)) {
            done = !interface.attemptIterate();
            interfaceThen = interfaceNow;
        }
    }
}
