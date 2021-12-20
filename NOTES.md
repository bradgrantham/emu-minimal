Emulation for https://github.com/slu4coder/Minimal-UART-CPU-System CPU, ROM, RAM, and UART.

Current status: Dec 19 2021 - Shelving to work on other projects

Notes
* microcode stepping does not occur as expected - there is a circuit loop from memory back through memory somehow, so repeated Evaluate() with clock high produce a sequence of outputs instead of quiescence
* probably need to have a way to do a "delay" on some objects, like delay until the second clock through, and that means returning "true" for whether internal state means on the first clock so the Step loop keeps going.
* maybe just want to move to emulating the CPU instructions directly

To build and run:
```
# copy down flash.bin from Slu's project
cmake -Bbuild -DCMAKE_BUILD_TYPE=Debug # or your preferred CMake incantation
./build/emu-minimal
