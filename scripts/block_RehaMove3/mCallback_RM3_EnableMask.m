function mCallback_RM3_EnableMask( )
%MCALLBACK_RM3_ENABLEMASK sets the Enabel/Disable mask for the RM3 setup elements.

%   TU Berlin --- Fachgebiet Regelungssystem
%   Author: Markus Valtin
%   Copyright © 2017 Markus Valtin. All rights reserved.
%
%   This program is free software: you can redistribute it and/or modify it under the terms of the 
%   GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or any later version.
%
%   This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
%   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

tabStim = {'on','on','on','on','on','on','on','on','off','off','on','on','on','on'};

switch get_param(gcb, 'stimRehaMoveProProtocol')
    case 'Use the LowLevel protocol   -> Each stimulation pulse is send separatly.'
        tabLowLevel = { 'on','off','on','off' };
        tabMidLevel = { 'off', 'off', 'off', 'off', 'off', 'off', 'off' };
    case 'Use the LowLevel protocol and use the user provieded pulse configs.'
        tabLowLevel = { 'off','on','on','off' };
        tabMidLevel = { 'off', 'off', 'off', 'off', 'off', 'off', 'off' };
    case 'Use the MidLevel protocol   -> Only stimulation pulse updates are send.'
        tabLowLevel = { 'off','off','off','off' };
        tabMidLevel = { 'on', 'on', 'on', 'on', 'on', 'on', 'on' };
    otherwise
        tabLowLevel = { 'off','off','off','off' };
        tabMidLevel = { 'off', 'off', 'off', 'off', 'off', 'off', 'off' };
        warning(['Unknown protocol: "', get_param(gcb, 'stimRehaMoveProProtocol'),'"']);
end


tabMisc1 = {'on','on','on','on','on','on','on','on','on','on','on', 'on'};
if (strcmp(get_param(gcb, 'miscEnableAdvancedSettings'), 'on'))
    tabMisc2 = {'on','on'};
else
    tabMisc2 = {'on','off'};
end

myEnableMask = [tabStim, tabLowLevel, tabMidLevel, tabMisc1, tabMisc2];
enableMask = get_param(gcb,'MaskEnables')';

if (min(strcmp(myEnableMask, enableMask)) == 0)
    set_param(gcb,'MaskEnables', myEnableMask);
end

end

