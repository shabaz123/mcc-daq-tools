
%set(groot,'defaultAxesYLim',[-140 0]);
%set(groot,'defaultAxesYLimMode','manual'); % set back to auto afterwards

url = 'http://192.168.1.88:8000/dspgen/';
adc = WSStream('ws://192.168.1.88:8080');
Fs=51200;
Ts=1/Fs;

cmd = [url 'a' num2str(0.5)]; % set amplitude to 0.5
adc.pause(); % don't need any data yet
% wait for amplitude to take effect
T = timer('TimerFcn',@(~,~)disp('Fired.'),'StartDelay',1);
start(T); wait(T);

level1=[];
level2=[];
ph=[];
f_arr=[10 20 30 40 50 60 70 80 90 100 200 400 1000 2000 5000 10000 15000 20000];
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
    T = timer('TimerFcn',@(~,~)disp('Fired.'),'StartDelay',1); start(T); wait(T);
    adc.pause();
    r1 = rms(adc.Data(1,:)); % get RMS value for first channel
    r2 = rms(adc.Data(2,:)); % get RMS value for second channel
    level1 = [level1 r1];
    level2 = [level2 r2];
    % calculate phase
    count=length(adc.Data(1,:)); % will be approx Fs since we ran a 1 second timer
    NFFT=2^nextpow2(count);
    y1=fft(adc.Data(1,:),NFFT)/count;
    y2=fft(adc.Data(2,:),NFFT)/count;
    phase1=angle(y1);
    phase2=angle(y2);
    u1=unwrap(phase1);
    u2=unwrap(phase2);
    
    [m1,i1]=max(abs(y1(1:NFFT/2))); % i1 is the index to the peak freq for input signal
    [m2,i2]=max(abs(y2(1:NFFT/2))); % i2 is the index to the peak freq for output signal
    % (i1/(NFFT/2)) * (Fs/2) should be closely equal to n (frequency).
    % same for i2.
    m = u1(i1) - u2(i2); % i1 and i2 should be very close for Fs/2!
    
    
    % a bit ugly, but it's a way of grabbing a portion of the data:
    %istart = (NFFT/2)/4;
    %istop = (NFFT/2)-istart;
    %udiff = (u1(istart:istop)-u2(istart:istop)) * (180/pi);
    %udiff = mod(udiff, 360);
    %m=mean(u1(istart:istop)-u2(istart:istop)); 
    m = m*(180/pi);
    m = mod(m, 360);
    %m=mean(udiff);
    ph = [ph m];
    
    f_idx = f_idx + 1;
end

hold off;
yyaxis left;
hl=semilogx(f_arr, 20*log10(level2./level1));
ylabel('Output (dB)');
xlabel('Frequency (Hz)');
grid on;
set(hl(1),'linewidth',3);
title('Frequency Response');
yyaxis right;
hr=semilogx(f_arr, ph);
ylabel('Phase (deg)');
set(hr(1),'linewidth',3);

curtick = get(gca, 'XTick');
set(gca, 'XTickLabel', cellstr(num2str(curtick(:))));

