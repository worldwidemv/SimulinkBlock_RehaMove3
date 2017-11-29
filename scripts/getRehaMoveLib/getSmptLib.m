function available = getSmptLib( sourceDir, targetDir )
%GETSMPTLIB copyies the libray to the corresponding directory

%   TU Berlin --- Fachgebiet Regelungssystem
%   Author: Markus Valtin
%   Copyright © 2017 Markus Valtin. All rights reserved.
%
%   This program is free software: you can redistribute it and/or modify it under the terms of the 
%   GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or any later version.
%
%   This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
%   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

% default
available = false;

switch (sourceDir)
    case 'smpt_rm3_gcc_linux_x86_amd64_static'
        %% Linux 64 bit
        if (isunix)
            available = copySmptLib( sourceDir, targetDir, 'Linux 64bit');
        end
end

end

