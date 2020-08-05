%% Example Script for sending a stimulation configuration from Maltab to the Simulink executable
%
%% To run this example:
% 1. open 'test_Rehamove3_NetworkInterface.slx' in Matlab and compile the Simulink diagram
% 2. run THIS Matlab script, this script will also execute and stop the Simulink executable!!
%
%% This script will:
% 1. start the executable './test_Rehamove3_NetworkInterface'
% 2. connect to the Simulink NetworkInterface block
% 3. update the stimulation intensity for 30 seconds 
% 4. close the connection to the NetworkInterface block
% 5. stop the executable

%% CONFIG
doStartTheExecutable = true;

%% implementation
% #########################################################################
disp('Rehamove3 control via Matlab example');
disp('#########################################');

% does the executable exist?
if (doStartTheExecutable)
    if (~exist('./test_Rehamove3_NetworkInterface', 'file'))
        error(['The executable ''./test_Rehamove3_NetworkInterface'' was not found in ', char(10), pwd(), char(10), char(10), ...
            'Please open ''test_Rehamove3_NetworkInterface.slx'' in Simulink and compile the Simulink diagram.']);
    end
end

%% RehaMove3 configuration
stop = false;
pw = ones(700, 1);
pw(1:400) = 300;
pw(401:595) = 500;

cur = zeros(700, 1);
cur(81:360) = (((1:280)/280) *10) +5;
cur(421:490) = 10;
cur(491:530) = 13;
cur(531:560) = 15;
cur(561:590) = 11;

disp(' -> plot desired intensity');
figure();
subplot(2,1,1);
plot(pw); legend('PulseWidth'); grid on;
subplot(2,1,2);
plot(cur); legend('Current'); grid on;

% run this parts only, if the executable needs to be started...
if (doStartTheExecutable)
    % cleanup
    if (exist('SimulinkRehaMove3.mat', 'file'))
        disp(' -> deleting old ''SimulinkRehaMove3.mat''' );
        delete('SimulinkRehaMove3.mat');
    end
    
    % start the executable
    disp(' -> starting the executable');
    command = ['sudo ls'];
    [status,cmdout] = system(command);
    command = ['cd ', pwd(), '; sudo ./test_Rehamove3_NetworkInterface &'];
    [status,cmdout] = system(command);
    pause(1);
end

% close any existing connection from an earlier run
if (exist('con', 'var'))
    fclose(con);
    clear con;
end

% setup for the UDP connection
% IP and ports -> has to match the Simulink block setup!
con = udp('127.0.0.1', 20000, 'LocalPort', 20001);
% set byte order, since the default is bigEndian!!!
con.ByteOrder = 'littleEndian';


% open the connection
disp(' -> open connection');
fopen(con);

disp(' -> sending data for 30 seconds ...');
% run this loop for 30 seconds
for i = 1:600
    pause(0.05);
    % update RehaMove3 inputs
    setRehaMove3Intensity( con, pw(i), cur(i), stop )
end

% stop the executable
disp(' -> stopping the executable');
stop = true;
setRehaMove3Intensity( con, 0, 0, stop );

% close the connection
fclose(con);
disp(' -> connection closed');
disp('#########################################');

% load and display simulink data
disp(' -> loading ''SimulinkRehaMove3.mat''' );
pause(2);
load('SimulinkRehaMove3.mat');
d = rmConfig;
time = d(1,:);
inPW = d(2,:);
inCUR = d(3,:);
inSTOP = d(4,:);
stimStatus = d(5:6,:);
msgCounter = d(7,:);

% plots
disp(' -> ploting RehaMove3 Simulink inputs' );
figure();
subplot(3,1,1); hold on;
plot(time,inPW);
plot(time,pw(1:length(time)));
legend('PW from Simulink', 'PW send by Matlab');
subplot(3,1,2); hold on;
plot(time,inCUR);
plot(time,cur(1:length(time)));
legend('CUR from Simulink', 'CUR send by Matlab');
subplot(3,1,3);
plot(time,stimStatus); legend('STIM Status');