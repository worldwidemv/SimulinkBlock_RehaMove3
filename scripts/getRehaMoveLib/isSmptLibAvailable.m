function available = isSmptLibAvailable( sourceDir, targetDir )
%ISSMPTLIBAVAILABLE checks if the library is availabe and promts for it if not.

%   TU Berlin --- Fachgebiet Regelungssystem
%   Author: Markus Valtin
%   Copyright © 2017 Markus Valtin. All rights reserved.
%
%   This program is free software: you can redistribute it and/or modify it under the terms of the 
%   GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or any later version.
%
%   This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
%   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

%% check if the targetDir is in the current directory
if (isdir(['.', filesep, targetDir]))
    % dir is available -> success -> return
    available = true;
else
    % dir is NOT availabe -> try to get it
    available = getSmptLib(sourceDir, targetDir);
end

end

