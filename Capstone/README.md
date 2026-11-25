# Smart Energy Consumption Analysis and Optimization System

A menu-driven C console project for recording appliance usage, estimating electricity consumption and energy charges, ranking the largest consumers, and producing practical savings suggestions.

## Features

- Records appliance name, rated power in watts, hours used per day, and days used per month.
- Calculates daily kWh and projected monthly kWh for each appliance and the whole household.
- Estimates an energy-only bill using progressive sample tariff slabs.
- Ranks appliances by monthly consumption and shows each appliance's share.
- Suggests reduced run time or a more efficient replacement, with estimated savings.
- Saves records in `appliances.dat` and creates a formatted `energy_report.txt` report.
- Checks menu choices and numeric input before using them.

## Build and run

Install GCC, then open a terminal in this project directory.

Windows (PowerShell with GCC on PATH):

```powershell
gcc -std=c11 -Wall -Wextra -pedantic smart_energy.c -o smart_energy.exe
.\smart_energy.exe
```

Linux or macOS:

```sh
gcc -std=c11 -Wall -Wextra -pedantic smart_energy.c -o smart_energy
./smart_energy
```

The data and report files are created in the program's current working directory.

## Sample tariff used

The program estimates progressive energy charges with these illustrative rates:

| Monthly units | Sample rate |
| --- | ---: |
| First 100 kWh | INR 3 per kWh |
| Next 100 kWh (101–200) | INR 5 per kWh |
| Next 300 kWh (201–500) | INR 7 per kWh |
| Above 500 kWh | INR 8 per kWh |

These are demonstration values, not a utility's official tariff. Update the `upper_limits` and `rates` arrays in `smart_energy.c` to match the applicable provider. The estimate excludes taxes, fixed charges, and other adjustments.

## Calculation notes

- Daily kWh = watts × hours per day ÷ 1,000.
- Monthly kWh = daily kWh × days used per month.
- For suggestions, the example savings assume one fewer hour of use per day when an appliance runs more than one hour daily. Otherwise, they show an example based on a replacement using 20% less power. Actual savings depend on usage and equipment.
- The app records hours and days, not time-of-day usage, so it does not calculate peak-hour patterns or time-of-use charges.

## Open and run from VS Code on Windows

The Microsoft C/C++ extension is already installed for this VS Code setup. A GCC compiler is also required; the extension alone cannot compile C programs.

1. Download and install MSYS2 from [msys2.org](https://www.msys2.org/). Keep the recommended install folder `C:\msys64`.
2. Open **MSYS2 UCRT64** from the Start menu and run:

   ```sh
   pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-gdb
   ```

   Press Enter to accept the package list and wait for it to finish.
3. Open this project folder in VS Code, open `smart_energy.c`, and press **F5**. VS Code will build the program and launch it in a console window so menu input works.

The `.vscode` settings expect GCC and GDB at `C:\msys64\ucrt64\bin`.

