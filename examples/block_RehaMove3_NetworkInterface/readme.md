# Readme

This examples only works with the [NetworkInterface block](https://github.com/worldwidemv/SimulinkBlock_NetworkInterface).

This example allows you to control the RehaMove3 stimulation intensity from a Matlab script.  
You can also use any other network client you like, provided you can send the network package required by the NetworkInface block.

## To run This Example:
1. open 'test_Rehamove3_NetworkInterface.slx' in Matlab and compile the Simulink diagram
2. run the Matlab script _test_MatlabToRehamove3.m_, this script will also execute and stop the Simulink executable!!

## The Maltab Script will:
1. start the executable './test_Rehamove3_NetworkInterface'
2. connect to the Simulink NetworkInterface block
3. update the stimulation intensity for 30 seconds 
4. close the connection to the NetworkInterface block
5. stop the executable

## Preparations

You should connect a test LED or dummy load to the channel 1 of the RehaMove3, connect the device with the PC, and turn the device on.