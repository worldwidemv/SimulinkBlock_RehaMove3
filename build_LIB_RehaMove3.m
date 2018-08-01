%% LCT build Blocks - RehaMove

%   TU Berlin --- Fachgebiet Regelungssystem
%   Author: Markus Valtin
%   Copyright © 2017 Markus Valtin. All rights reserved.
%
%   This program is free software: you can redistribute it and/or modify it under the terms of the 
%   GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or any later version.
%
%   This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
%   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

%% disable specific warnings
warn_state = warning();
warning('off','MATLAB:mex:GccVersion_link');
warning('off','MATLAB:MKDIR:DirectoryExists');

%% check if the Soft Realtime Toolbox is available
if (~exist('xsrt_buildLibScriptInitLCT', 'file')), error(['The Soft-Realtime Simulink Toolbox is not availabe!', char(10), char(10), ...
        'This Simulink block depends on the "Soft-Realtime Simulink Toolbox", a universal framework for', char(10), 'creating own Simulink blocks (e.g. for own hardware) and soft realtime execution of Simulink diagrams.', char(10), char(10), ...
        'Please install the Soft-Realtime Simulink Toolbox from https://github.com/worldwidemv/SimulinkToolchain', char(10), 'and make sure the Matlab path is setup correctly, e.g. by running the Matlab script "srt_InstallSRT.m"!']);
end

%% check if the installation was done
if (~exist('.installDone', 'file')), error(['The installation was not done yet!', char(10), 'Please run the Matlab script "srt_InstallSRT.m", which is part of the Soft Realtime Toolbox and on which these block depends on!']); end

%% initialize the Matlab/SRT variables
[startDir, libDir, sfuncFolderName, srtAddPath, defs] = xsrt_buildLibScriptInitLCT(mfilename('fullpath'));

%% add function to check if the library is availabe
addpath(fullfile(pwd, 'scripts', 'getRehaMoveLib')); 

%% generate configuration for RehaMove blocks
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
libName = 'RehaMove';
%% RehaMove3_01 block
def = legacy_code('initialize');
def.SFunctionName = 'sfunc_RehaMove3_01';
%sfunc_RehaMove3_XX
def.StartFcnSpec  =    'void lctRM3_Initialise( void **work1, uint16 p1[], uint16 size(p1,1), uint16 p2[], uint16 size(p2,1), double p3[], uint16 size(p3,1), uint16 p4[], uint16 size(p4,1), uint16 p5, uint16 p6, uint16 p7, double p8 )';
def.OutputFcnSpec =    'void lctRM3_InputOutput( void **work1, double u1[p6][p5], double u2[p7][p5], double y1[2] )';
def.TerminateFcnSpec = 'void lctRM3_Deinitialise( void **work1 )';
def.IncPaths     = {fullfile(pwd, 'srcRehaMove_LibV3.2', 'src'), fullfile(pwd, 'incRehaMove_LibV3.2_lin_x86_64', 'include', 'general'), fullfile(pwd, 'incRehaMove_LibV3.2_lin_x86_64', 'include', 'low-level'), fullfile(pwd, 'incRehaMove_LibV3.2_lin_x86_64', 'include', 'mid-level')};
def.SrcPaths     = {fullfile(pwd, 'srcRehaMove_LibV3.2', 'src')};
def.HeaderFiles  = {'RehaMove3Interface_SMPT32X.hpp', 'block_RehaMove3_01.hpp'};
def.SourceFiles  = {'RehaMove3Interface_SMPT32X.cpp', 'block_RehaMove3_01.cpp'};
def.LibPaths     = {fullfile(pwd, 'incRehaMove_LibV3.2_lin_x86_64', 'lib')};
def.HostLibFiles = {'libsmpt.a'};
def.TargetLibFiles  = {'libsmpt.a'};
def.Options.language = 'C++';
def.Options.useTlcWithAccel = false;
def.SampleTime   = 'parameterized';
if (isSmptLibAvailable( 'smpt_rm3_gcc_linux_x86_amd64_static', 'incRehaMove_LibV3.2_lin_x86_64' ))
    defs{end+1} = def;
    srtAddPath{end+1} = fullfile(pwd, 'scripts', 'block_RehaMove3'); 
    srtAddPath{end+1} = fullfile(pwd, 'html', 'block_RehaMove3');
else
    warning('The Hasomed SMPT library for Linux 64 bit is not available! The sfunction %s will not be build!', def.SFunctionName);
end

%% auto generate the Simulink S-functions via LCT
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
result = xsrt_buildLibScriptEndLCT(startDir, libDir, libName, sfuncFolderName, srtAddPath, defs);
% copy the Simulink lib
copyfile(['.', filesep, '*.slx'], ['.', filesep, sfuncFolderName]);

%% restore the warning settings
warning(warn_state);

%% remove function to check if the library is availabe from the path
rmpath(fullfile(pwd, 'scripts', 'getRehaMoveLib'));
