function mCallback_RM3_CheckChannels( )
%MCALLBACK_RM3_CHECKCHANNELS checks that the input channels are only 1 ... 4.

    %% check the input value
    channelsStr = get_param(gcb, 'stimChannels');
    channels = round( evalin('caller', channelsStr) );
    
    has_error = false;
    for i = 1:length(channels)
        if (channels(i) < 0)
            channels(i) = 0; 
            has_error = true;
        end
        if (channels(i) > 4)
            channels(i) = 0; 
            has_error = true;
        end
        if (round(channels(i)) ~= channels(i))
            channels(i) = round(channels(i)); 
            has_error = true;
        end
    end
    if (has_error)
        errordlg('Channels must be integers from 1 to 4!','Channel Config Error');
    end
    
    switch get_param(gcb, 'stimRehaMoveProProtocol')
    case ' Use the MidLevel protocol   -> Only stimulation pulse updates are send.'
        if (size(channels,2) ~= size(unique(channels),2))
            channels = unique(channels);
            errordlg('While using the MidLevel protocol, channels must be UNIQUE integers from 1 to 4!','Channel Config Error');
        end
    end
    
    % check the number of channels
    Nchannels = length(channels);
    if (Nchannels > 10)
        Nchannels = 10;
        errordlg('You can NOT use more than 10 stimulation pulses! ','Channel Config Error');
    end
    
    %% set the updatet value
    channelsStrNew = ['[ ',num2str(channels(1:Nchannels)), ' ]'];
    if (~strcmp(channelsStrNew, channelsStr))
        set_param(gcb, 'stimChannels', channelsStrNew);
    end
end

