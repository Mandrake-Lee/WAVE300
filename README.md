# WAVE300
Sources for Lantiq WAVE300 wi-fi driver

## Scope
The aim of this project is to upbring the sources to newer Linux kernels as original sources available seem to be fitted for kernels 2.6

## -LEGAL DISCLAIMER-
The original sources for this git (Lantiq noted as 3.2.1), upon with all changes are made, came with no license file although in the code of the modules it is clearly stated "proprietary".

However, in previous releases of the drivers like 3.01.0, there's a general LICENSE (txt) file where dual BSD/GPL is granted.

Being the sources of this git an evolution of the old ones, the LICENSE (txt) file and its content must be kept.

Nonetheless, being myself an absolute newcomer into open-source world and licensing, I beg the original authors & owners of this code Infineon/Lantiq/Intel to correct me if I'm wrong and I will immediatly turn down this git.

## Start-off
Please read the instructions.txt file.

## Status
- Compilation successful in embedded linux (LEDE) 4.4
- **Not working.** The driver is stopped during PCI MSI request. Further investigations points that this is a system issue, nothing to do with the driver

## TODO
* Driver roll-back (whenever it encounters an error during driver start-up) is not working properly, crashing the system
* PCI MSI request (interrupts) not working. This seems more a system (linux LEDE distro) issue than the driver itself
* Bypassing MSI request, the driver stops during response request of the internal chipset CPU. However, first fix MSI issue above
