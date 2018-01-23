function  mCallback_RM3_CheckFrequencysML( )
%MCALLBACK_RM3_CHECKFREQUENCYSML checks the frequencie for the MidLevel mode.

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
stimFrequency = evalin('caller', get_param(gcb, 'mlFStim'));
if (stimFrequency < 0.5 && stimFrequency ~= -1)
    stimFrequency = 0.5;
    errordlg('The stimulation frequency must not deceed 0.5 Hz!','Stimulation Frequency Config Error');
    set_param(gcb, 'mlFStim', num2str(stimFrequency));
end
if (stimFrequency > 200)
    stimFrequency = 200;
    errordlg('The stimulation frequency must not exceed 200 Hz!','Stimulation Frequency Config Error');
    set_param(gcb, 'mlFStim', num2str(stimFrequency));
end

end

