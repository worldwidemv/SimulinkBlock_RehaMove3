function available = getSmptLib( sourceDir, targetDir )
%GETSMPTLIB copyies the libray to the corresponding directory
%   Detailed explanation goes here

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

