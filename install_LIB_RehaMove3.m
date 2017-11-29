%% Install SRT Blocks - RehaMove

%   TU Berlin --- Fachgebiet Regelungssystem
%   Author: Markus Valtin
%   Copyright © 2017 Markus Valtin. All rights reserved.
%
%   This program is free software: you can redistribute it and/or modify it under the terms of the 
%   GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or any later version.
%
%   This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
%   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

%% install and setup the library
% add function to check if the library is availabe
addpath(fullfile(pwd, 'scripts', 'getRehaMoveLib'));
if (~isSmptLibAvailable( 'smpt_rm3_gcc_linux_x86_amd64_static', 'incRehaMove_LibV3.2_lin_x86_64' ))
    warning('The Hasomed SMPT library for Linux 64 bit was not installed correctly!');
end
% remove function to check if the library is availabe from the path
rmpath(fullfile(pwd, 'scripts', 'getRehaMoveLib'));

%% copy udev ruels
disp('TODO: copy udev rules');

%% mark this block Lib as installed
fileID = fopen('.installDone','w');
fwrite(fileID,' ');
fclose(fileID);