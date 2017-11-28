function mCallback_RM3_EnableMask( )
%MCALLBACK_RM3_ENABLEMASK sets the Enabel/Disable mask for the RM3 setup elements.

tabStim = {'on','on','on','on','on','on','on','on','off','off','on','on','on','on'};

switch get_param(gcb, 'stimRehaMoveProProtocol')
    case 'Use the LowLevel protocol   -> Each stimulation pulse is send separatly.'
        tabLowLevel = { 'on','off','on','off' };
        tabMidLevel = { 'off' };
    case 'Use the LowLevel protocol and use the user provieded pulse configs.'
        tabLowLevel = { 'off','on','on','off' };
        tabMidLevel = { 'off' };
    case 'Use the MidLevel protocol   -> Only stimulation pulse updates are send.'
        tabLowLevel = { 'off','off','off','off' };
        tabMidLevel = { 'on' };
    otherwise
        tabLowLevel = { 'off','off','off','off' };
        tabMidLevel = { 'off' };
        warning(['Unknown protocol: "', get_param(gcb, 'stimRehaMoveProProtocol'),'"']);
end


tabMisc1 = {'on','on','on','on','on','on','on','on','on','on','on'};
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

