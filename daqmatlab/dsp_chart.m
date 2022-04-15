
%set(groot,'defaultAxesYLim',[-140 0]);
%set(groot,'defaultAxesYLimMode','manual'); % set back to auto afterwards

url = 'http://192.168.1.88:8000/dspgen/';
adc = WSStream('ws://192.168.1.88:8080');
Fs=51200;
Ts=1/Fs;

cmd = [url 'a' num2str(0.95)]; % set amplitude to 0.8
webwrite(cmd, ""); % issue the command to set the amplitude
adc.pause(); % don't need any data yet
% wait for amplitude to take effect
T = timer('TimerFcn',@(~,~)disp('Fired.'),'StartDelay',1);
start(T); wait(T);

level1=[];
res_dist=[];
res_sinad=[];
f_arr=[20 25 30 40 60 80 100 140 200 300 400 600 700 800 900 1000 1200 1500 2000 2500 3000 3300 3500];
f_idx=1;

while f_idx<(length(f_arr)+1)
    n = f_arr(f_idx);
    fprintf("doing frequency: %d\n", n);
    cmd = [url 'f' num2str(n)]; % set frequency to n
    webwrite(cmd, ""); % issue the command to set the frequency
    % wait for tone to stabilise
    T = timer('TimerFcn',@(~,~)disp('Fired.'),'StartDelay',1); start(T); wait(T);
    adc.Data=[]; % clear old data
    adc.resume(); % start capturing data
    T = timer('TimerFcn',@(~,~)disp('Fired.'),'StartDelay',3); start(T); wait(T);
    adc.pause();
    res_dist=[res_dist thd(adc.Data(1,:))];
    res_sinad=[res_sinad sinad(adc.Data(1,:))];
    r1 = rms(adc.Data(1,:)); % get RMS value for first channel
    level1 = [level1 r1];


    
    f_idx = f_idx + 1;
end



figure(1);
h1=semilogx(f_arr, res_dist);
title('Total Harmonic Distortion (THD)');
curtick = get(gca, 'XTick');
set(gca, 'XTickLabel', cellstr(num2str(curtick(:))));
set(h1(1),'linewidth',3);
grid on;
xlabel('Frequency (Hz)');
ylabel('THD (dB)');

figure(2);
h2=semilogx(f_arr, res_sinad);
title('SINAD');
curtick = get(gca, 'XTick');
set(gca, 'XTickLabel', cellstr(num2str(curtick(:))));
set(h2(1),'linewidth',3);
grid on;
xlabel('Frequency (Hz)');
ylabel('SINAD (dB)');

figure(3);
h3=semilogx(f_arr, level1);
title('Output Level');
curtick = get(gca, 'XTick');
set(gca, 'XTickLabel', cellstr(num2str(curtick(:))));
set(h3(1),'linewidth',3);
grid on;
xlabel('Frequency (Hz)');
ylabel('Amplitude (Vrms)');

