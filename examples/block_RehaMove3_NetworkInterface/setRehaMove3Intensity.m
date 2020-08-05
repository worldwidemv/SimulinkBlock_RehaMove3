function setRehaMove3Intensity( con, pw, cur, stop )
%SETREHAMOVE3INTENSITY build the RM3 config and send it to the NetworkInterface block

data = [pw, cur, double(stop)];
ni_sendData(con, data);

end

