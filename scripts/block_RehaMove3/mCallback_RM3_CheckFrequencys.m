function  mCallback_RM3_CheckFrequencys( )
%MCALLBACK_RM3_CHECKFREQUENCYS checks the frequencies and updated the sample rate.

%% check the input value
SampleTimeStrOld = get_param(gcb, 'SampleTime');
stimFrequency = evalin('caller', get_param(gcb, 'stimFrequency'));
if (stimFrequency < 5)
    stimFrequency = 5;
    errordlg('The stimulation frequency must not deceed 5 Hz!','Stimulation Frequency Config Error');
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

