# SimulinkBlock_RehaMove3

This repository contains a Simulink block for the [HASOMED RehaMove3](https://www.rehamove.com/what-is-rehamove/science-mode.html) FES stimulator, and the underling C++ interface class.

The Simulink block itself *requires*:

   * [Simulink](https://www.mathworks.com/products/simulink.html) + Legacy Code Toolbox,
   * [Simulink Coder](https://www.mathworks.com/products/simulink-coder.html),
   * [Embedded Coder](https://www.mathworks.com/products/embedded-coder.html), and
   * the [Soft-Realtime Simulink Toolbox](https://github.com/worldwidemv/SimulinkToolchain), which must be installed first



## Installation

Please install the [Soft-Realtime Simulink Toolbox](https://github.com/worldwidemv/SimulinkToolchain) first.  
We assume you have installed Soft-Realtime Simulink Toolbox (SRT) into the directory `SimulinkToolchain` from now one.

### Get the Code
Go to the library directory and clone this repository:
```shell
cd SimulinkToolchain/SimulinkLib_linux64/
git clone https://github.com/worldwidemv/SimulinkBlock_RehaMove3.git
```

### Get the Library
Next, you have to download the ScienceMode library from Hasomed. 
This library provides a low-level API and is a build requirement for your Simulink models.  
The library must be downloaded from the Hasomed website: [https://www.rehamove.com/what-is-rehamove/science-mode.html](https://www.rehamove.com/what-is-rehamove/science-mode.html).   
The last tested library version is [smpt_rm3_V3.2.4a](https://www.rehamove.com/fileadmin/user_upload/RehaMove/ScienceMode/smpt_rm3_V3.2.4a.zip).

Download and unzip the library in any folder, you will be asked for the folder later on and the relevant files will be copied from there.

### Run the Install Scripts

Now, you have to run the SRT install script and build the block functions.  
This is all done by the SRT script `srt_InstallSRT.m`.

Please run this script in Matlab. The script should:

* guide you through the installation,
* build your blocks,
* create/update the Simulink Library Pallet, and
* update and save the Matlab path.

### Test your Installation

You can test if every works with one of the examples provided with the block.
In Matlab, go to `SimulinkToolchain/SimulinkLib_linux64/SimulinkBlock_RehaMove3/examples/block_RehaMove3/` and open e.g. the file test_`RehaMove3_block__LowLevel1_simple.slx`

Build the diagram by clicking the "_Build Model_" button.  
[Example_LowLevel1_Simple.png](https://github.com/worldwidemv/SimulinkBlock_RehaMove3/raw/master/html/block_RehaMove3/Example_LowLevel1_Simple.png "Simple_LowLevel1_Example")

This should compile without errors!  
Details are shown in the Diagnostic Viewer, available (once you hit "_Build Model_") at the bottom of the window.

If the compilation was successful, you will find an executable called like the model, e.g. `test_RehaMove3_block__LowLevel1_simple` inside the current Matlab working directory, e.g. `SimulinkToolchain/SimulinkLib_linux64/SimulinkBlock_RehaMove3/examples/block_RehaMove3/`.

This executable must now be started as root, because the Linux low latency scheduler needs root privileges.
```shell
cd SimulinkToolchain/SimulinkLib_linux64/SimulinkBlock_RehaMove3/examples/block_RehaMove3/
sudo ./test_RehaMove3_block__LowLevel1_simple
```

This should run the generated Simulink Model, and you should see a similar output than below.  
**To stop** the executable, you can press CTL-ALT-C or go to Simulink, click on "_Connect to Target_" (External Mode) and the click on "_Stop_". 

```shell
$ sudo ./test_RehaMove3_block__LowLevel1_simple

** starting the model **
STIM1: Initialising /dev/ttyUSB0 was successful!
STIM1: Status Report
     -> Interface: /dev/ttyUSB0
     -> Device ID: 160550007
     -> Battery Voltage: 100% (8.22V)
     -> Last updated: 0.001 seconds ago
     -> Init Threat running: yes
     -> Receiver Threat running: yes
     -> LowLevel:
        -> Initialised: yes
        -> Current/Last High Voltage: 90V
        -> Use Denervation: no
        -> Abort after 2 stimulation errors
        -> Resume the stimulation after 100 sequences


STIM1 Puls Info: time=0.208
  Puls 1 -> Channel=1; Shape=0; PW=300; I=10.0
     PointConfig  1: Duration= 300µs; Current= +10.00mA; (Mode=0; IM=0)
     PointConfig  2: Duration= 100µs; Current=  +0.00mA; (Mode=0; IM=0)
     PointConfig  3: Duration= 300µs; Current= -10.00mA; (Mode=0; IM=0)


......
...... a lot more output ...
......



STIM1 Puls Info: time=4.258
  Puls 1 -> Channel=1; Shape=0; PW=300; I=10.0
     PointConfig  1: Duration= 300µs; Current= +10.00mA; (Mode=0; IM=0)
     PointConfig  2: Duration= 100µs; Current=  +0.00mA; (Mode=0; IM=0)
     PointConfig  3: Duration= 300µs; Current= -10.00mA; (Mode=0; IM=0)

^CSimulation aborted by pressing CTRL+C
STIM1: DeInitialising the device /dev/ttyUSB0 was successful!
STIM1: Status Report
     -> Interface: /dev/ttyUSB0
     -> Device ID: 160550007
     -> Battery Voltage: 100% (8.17V)
     -> Last updated: 0.001 seconds ago
     -> Init Threat running: no
     -> Receiver Threat running: yes
     -> LowLevel:
        -> Initialised: no
        -> Current/Last High Voltage: 90V
        -> Use Denervation: no
        -> Abort after 2 stimulation errors
        -> Resume the stimulation after 100 sequences

STIM1: Statistic Report LowLevel:
     -> Pulse SEQUENCES send: 82 (82 pulses send; 0 pulses NOT send)
        -> Successful: 82 (82 pulses)
        -> Unsuccessful: 0 (0 pulses)
           -> Stimulation Error: 0
        -> Missing: 0
     -> Input Corrections:
        -> Invalid Input: 0 pulses
        -> Current correction (to high): 0 pulses
        -> Current correction (to low):  0 pulses
        -> Pulsewidth correction (to high): 0 pulses
        -> Pulsewidth correction (to low):  0 pulses
```

## Usage / Documentation

Please find the documentation in the [Wiki](https://github.com/worldwidemv/SimulinkBlock_RehaMove3/wiki).


## License

This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.
A copy is provided in the the LICENSE file or can be at [http://www.gnu.org/licenses/](http://www.gnu.org/licenses/).

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
