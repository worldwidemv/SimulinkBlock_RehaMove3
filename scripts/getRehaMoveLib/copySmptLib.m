function available = copySmptLib( sourceDir, targetDir, targetType )
%COPYSMPTLIB Asks for the path to the SMPT Lib and copies the needed files.

%   TU Berlin --- Fachgebiet Regelungssystem
%   Author: Markus Valtin
%   Copyright © 2017 Markus Valtin. All rights reserved.
%
%   This program is free software: you can redistribute it and/or modify it under the terms of the 
%   GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or any later version.
%
%   This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
%   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

%% show an information dialoge
uiwait(msgbox( ['The Hasomed SMPT library for ', targetType, ', needed for the RehaMove3 block, was not found yet!', char(10), char(10), ...
                'Please download the SMPT library from the Hasomed site', char(10), ...
                '( https://www.rehamove.com/what-is-rehamove/science-mode.html ) and', char(10), ...
                'unzip the ZIP-file. You will be asked for the location of this directory next.', char(10) ...
               ], 'SMPT library info','modal'));

%% get the folder of the Hasomed library
run = true;
while(run)
    smptFolder = uigetdir('','SMPT library folder?');
    if (isnumeric(smptFolder))
        available = false;
        return;
    end
    
    if (~isdir(smptFolder))
        uiwait(errordlg(['The folder "', smptFolder, '" does not exist!']));
        continue;
    end
    
    if (~isdir(fullfile(smptFolder, 'library')))
        uiwait(errordlg(['There is no "library" folder within ', char(10), '"', smptFolder, '"!', char(10),char(10), 'Please select a valid SMPT library directory!']));
        continue;
    end
    
    if (~isdir(fullfile(smptFolder, 'library', sourceDir)))
        uiwait(errordlg(['There is no "library/', sourceDir, '" folder within ', char(10), '"', smptFolder, '"!', char(10),char(10), 'Please select a valid SMPT library directory!']));
        continue;
    end
    
    copyfile(fullfile(smptFolder, 'library', sourceDir), ['.', filesep, targetDir]);
    disp(['SMPT library directory "', targetDir, '" added.']);
    available = true;
    run = false;
end

end

