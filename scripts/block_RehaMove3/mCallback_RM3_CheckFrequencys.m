function  mCallback_RM3_CheckFrequencys( )
%MCALLBACK_RM3_CHECKFREQUENCYS checks the frequencies and updated the sample rate.

%   TU Berlin --- Fachgebiet Regelungssystem
%   Author: Markus Valtin
%   Copyright © 2017 Markus Valtin. All rights reserved.
%
%   This program is free software: you can redistribute it and/or modify it under the terms of the 
%   GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or any later version.
%
%   This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
%   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

%% check the input value
SampleTimeStrOld = get_param(gcb, 'SampleTime');
stimFrequency = evalin('caller', get_param(gcb, 'stimFrequency'));
if (stimFrequency < 1)
    stimFrequency = 1;
    errordlg('The stimulation frequency must not deceed 1 Hz!','Stimulation Frequency Config Error');
    set_param(gcb, 'stimFrequency', num2str(stimFrequency));
end
if (stimFrequency > 100)
    stimFrequency = 100;
    errordlg('The stimulation frequency must not exceed 100 Hz!','Stimulation Frequency Config Error');
    set_param(gcb, 'stimFrequency', num2str(stimFrequency));
end

SampleTime = 1/stimFrequency;
textSampleTime = [num2str(SampleTime), 's (based on stimulation frequency)'];

%% set the updatet value
if (~strcmp(SampleTimeStrOld, num2str(SampleTime)))
    set_param(gcb, 'SampleTime', num2str(SampleTime));
    set_param(gcb, 'stimTextSampleTime', textSampleTime);
end

end

