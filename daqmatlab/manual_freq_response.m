
adc = WSStream('ws://192.168.1.88:8080');
involt=10; % expected input Vp-p
adc.pause(); % stop data
tval = 0;
resp=[];
f_arr=[1 2 3 4 5 6 7 8 9 10 20 40 100 200 500 1000 2000 4000 8000 16000 18000 19000 20000 21000 22000 23000];
f_idx=1;

while f_idx<(length(f_arr)+1)
    n = f_arr(f_idx);
    fprintf("Set frequency to %d Hz and press a key:\n", n);
    pause;
    adc.Data=[]; % clear old data
    adc.resume(); % start data
    tval = 1;
    if f_idx<12
        tval = 10;
    end
    T = timer('TimerFcn',@(~,~)disp('Fired.'),'StartDelay',tval); start(T); wait(T);
    adc.pause(); % stop data
    resp = [resp rms(adc.Data(2,:))];
    
    f_idx = f_idx + 1;
end

rel=(sqrt(2)*2*resp)/involt; % this is the ratio
respdb=20*log10(rel);
semilogx(f_arr, respdb);
grid on;


