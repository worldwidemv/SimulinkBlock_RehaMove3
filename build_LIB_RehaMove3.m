%% LCT build Blocks - RehaMove

%% disable specific warnings
warn_state = warning();
warning('off','MATLAB:mex:GccVersion_link');
warning('off','MATLAB:MKDIR:DirectoryExists');

%% initialize the Matlab/SRT variables
[startDir, libDir, sfuncFolderName, srtAddPath, defs] = xsrt_buildLibScriptInitLCT(mfilename('fullpath'));

%% add function to check if the library is availabe
addpath(fullfile(pwd, 'scripts', 'getRehaMoveLib')); 

%% generate configuration for RehaMove blocks
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
libName = 'RehaMove';
%% RehaMove3_01 block
def = legacy_code('initialize');
def.SFunctionName = 'sfunc_RehaMove3_01';
%sfunc_RehaMove3_XX
def.StartFcnSpec  =    'void lctRM3_Initialise( void **work1, uint16 p1[], uint16 size(p1,1), uint16 p2[], uint16 size(p2,1), uint16 p3[], uint16 size(p3,1), uint16 p4[], uint16 size(p4,1), uint16 p5, uint16 p6, double p7 )';
def.OutputFcnSpec =    'void lctRM3_InputOutput( void **work1, double u1[p5][p6], double u2[p5][p6], double y1[3] )';
def.TerminateFcnSpec = 'void lctRM3_Deinitialise( void **work1 )';
def.IncPaths     = {fullfile(pwd, 'srcRehaMove_LibV3.2', 'src'), fullfile(pwd, 'incRehaMove_LibV3.2_lin_x86_64', 'include', 'general'), fullfile(pwd, 'incRehaMove_LibV3.2_lin_x86_64', 'include', 'low-level'), fullfile(pwd, 'incRehaMove_LibV3.2_lin_x86_64', 'include', 'mid-level')};
def.SrcPaths     = {fullfile(pwd, 'srcRehaMove_LibV3.2', 'src')};
def.HeaderFiles  = {'RehaMove3Interface_SMPT32X.hpp', 'block_RehaMove3_01.hpp'};
def.SourceFiles  = {'RehaMove3Interface_SMPT32X.cpp', 'block_RehaMove3_01.cpp'};
def.LibPaths     = {fullfile(pwd, 'incRehaMove_LibV3.2_lin_x86_64', 'lib')};
def.HostLibFiles = {'libsmpt.a'};
def.TargetLibFiles  = {'libsmpt.a'};
def.Options.language = 'C++';
def.Options.useTlcWithAccel = false;
def.SampleTime   = 'parameterized';
if (isSmptLibAvailable( 'smpt_rm3_gcc_linux_x86_amd64_static', 'incRehaMove_LibV3.2_lin_x86_64' ))
    defs{end+1} = def;
    srtAddPath{end+1} = fullfile(pwd, 'scripts', 'block_RehaMove3'); 
    srtAddPath{end+1} = fullfile(pwd, 'html', 'block_RehaMove3');
else
    warning('The Hasomed SMPT library for Linux 64 bit is not available! The sfunction %s will not be build!', def.SFunctionName);
end

%% auto generate the Simulink S-functions via LCT
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
 result = xsrt_buildLibScriptEndLCT(startDir, libDir, libName, sfuncFolderName, srtAddPath, defs);
 
%% restore the warning settings
warning(warn_state);

%% remove function to check if the library is availabe from the path
rmpath(fullfile(pwd, 'scripts', 'getRehaMoveLib')); 