function [c1,c2] = daqgrab(tval)
%daqgrab Acquire samples for time t seconds
%   Simply acquires data from the daqstream application
%   Has some hard-coded values, that may need changing, like IP address!

    adc = WSStream('ws://192.168.1.88:8080');
    adc.pause(); % don't need any data yet
    adc.Data=[]; % clear old data
    adc.resume(); % start capturing data
    T = timer('TimerFcn',@(~,~)disp('Fired.'),'StartDelay',tval); start(T); wait(T);
    adc.pause();
    c1=adc.Data(1,:);
    c2=adc.Data(2,:);
end

