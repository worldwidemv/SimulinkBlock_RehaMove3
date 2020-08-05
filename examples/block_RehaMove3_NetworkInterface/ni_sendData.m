function ni_sendData( t, data )
%NI_SENDDATA Summary of this function goes here
%   Detailed explanation goes here

out = double(zeros(1, length(data)+ 4));
out(1) = 333.333;       % id
out(2) = round(t.ValuesSent /length(out)) +1;   % counter
out(3) = length(data);  % nData

out(4:(end-1)) = double(data);

out(end) = 333.333;     % id

fwrite(t, out, 'double');
end

