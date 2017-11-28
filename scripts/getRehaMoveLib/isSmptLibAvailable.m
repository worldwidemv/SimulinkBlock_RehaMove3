function available = isSmptLibAvailable( sourceDir, targetDir )
%ISSMPTLIBAVAILABLE checks if the library is availabe and promts for it if not.
%   

%% check if the targetDir is in the current directory
if (isdir(['.', filesep, targetDir]))
    % dir is available -> success -> return
    available = true;
else
    % dir is NOT availabe -> try to get it
    available = getSmptLib(sourceDir, targetDir);
end

end

