# ForgeView: A scope for Eurorack

<!-- <img src="./images/Logo-CLK.webp" alt="Logo" style="width:50%"/> -->

## Overview

ForgeView provides a simple oscilloscope for Eurorack systems. It can be used to visualize waveforms and signals in your modular system. The module has a 0.96" OLED display and can be used to visualize waveforms from LFOs, envelopes, oscillators, filters, and other signals in your system.

Part of the **Forge** series of modules which share a single hardware platform.

The hardware schematics and design files are completely open-source and available in the [GitHub repository](https://github.com/VoltageFoundryMod/ForgeSeries-Hardware).

<!-- <img src="./images/Front.jpg" alt="Logo" style="width:20%"/> -->

[ModularGrid](https://modulargrid.net/e/modules/view/52717)

## Features

- **4 modes of operation** - The module can display dual waveforms, single waveform in audio range, single waveform with external trigger and spectrum analyzer.
- **Dual Waveform Display** - Display two waveforms at the same time with configuration for vertical and horizontal scaling and vertical offset.
- **Single Audio Waveform** - Display a single waveform in audio range with configuration for vertical and horizontal scaling and vertical offset.
- **Single Waveform with External Trigger** - Display a single waveform with external trigger input to freeze the waveform like taking a snapshot of a drum hit.
- **Spectrum Analyzer** - Display a spectrum analyzer of the input signal (not implemented yet).

## Configuration Parameters

The display will show the full waveform of the input signals. When the encoder is turned, a menu will show up and the parameters can be adjusted. The parameters are:

- **Mode**: Select the display mode (Dual, Single Audio, Single with Trigger, Spectrum Analyzer).
- **Hori**: Horizontal scaling of the waveform.
- **Offs**: Vertical offset of the waveform.
- **Vert**: Vertical scaling of the waveform.

On dual mode, both traces are scaled and offset together. On single mode, the waveform is scaled and offset individually. This might be improved in the future.

## Operation

### Interface

- TRIG: External trigger input for mode 3 (0-5V)
- IN1, IN2: CV inputs for signals (0-5V)
- OUT 1 / 2: Outputs 1 and 2 (0-5V)
- OUT 3 / 4: Outputs 3 and 4 (0-5V)

The signals from IN1 will be copied in a S&H to OUT1 and the signals from IN2 will be copied in a S&H to OUT2. The signals from IN1 and IN2 will be displayed on the screen. Do not expect high resolution and accuracy on the outputs or the display. The module is intended for visualization and not for high precision measurements.

## Firmware Update

1. Download the latest firmware from the Releases section of the [GitHub repository](https://github.com/VoltageFoundryMod/ForgeSeries/releases). The firmware file is named `CURRENT.UF2`.
2. Connect the module to your computer using a USB-C cable. The CPU can be removed from the module as it's socketed to the main board. Firmware loading can be done with the CPU removed.
3. Use tweezers or a jumper wire to quickly short TWICE the two pads labeled `RESET` on the back of the module CPU. The orange LED will flicker and light up.
4. Copy and overwrite the `CURRENT.UF2` file to the module USB drive named "Seeed XIAO" that will appear. After copy is finished, the module will reboot and the new firmware will be loaded.

![Module bootloader mode](../images/XIAO-reset.gif)

## Troubleshooting

- **No Power**: Ensure the module is properly connected to the power supply and the power jumper is set correctly.
- **No Output**: Verify the board connections and output settings and ensure the module is not stopped.

## Powering

The module can be powered by either 12V or 5V if your power supply supports it. The supply can be selected from an on-board jumper where closing the the SEL with REG jumper, will take power from eurorack 12V supply and closing the SEL with BOARD jumper, will take power from 5V (requires 16 pin cable). It can also be powered by the USB-C jack on the Seeeduino Xiao.

## Specifications

- **Power Supply**: 12V or 5V jumper selectable
- **Input CV Range**: 5V
- **Output CV Range**: 5V
- **Dimensions**: 6HP
- **Depth**: 40mm
- **Current Draw**: 60mA @ +12V or +5V

## Contact

For support and inquiries, please open an issue on the [GitHub repository](https://github.com/VoltageFoundryMod/ForgeSeries).

## Development

If you want to build and developt for the module, check [this file](Building-Developing.md) for more information.

## Acknowledgements

Great part of the code are inspired by Hagiwo's code.
Thanks for the inspiration!

## License

This project is licensed under the MIT License. See the `LICENSE` file for more information.

---

Thank you for choosing the ForgeView module. We hope it enhances your musical creativity and performance.
