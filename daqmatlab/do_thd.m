
set(groot,'defaultAxesYLim',[-140 0]);
set(groot,'defaultAxesYLimMode','manual'); % set back to auto afterwards

MakeTimer();


function MakeTimer()
   adc = WSStream('ws://192.168.1.88:8080');
   i=0; % do THD calculation when i=0, and re-enable samples when i=iproc
   iproc=10; % long enough to let the THD calculation finish
   t=timer;
   t.StartFcn = @initTimer;
   t.TimerFcn = @timerCallback;
   t.StopFcn = @stoppedTimer;
   t.Period   = 0.1;
   t.TasksToExecute = 200;
   t.ExecutionMode  = 'fixedRate';
   start(t);
   wait(t);
   delete(t);
   
   function initTimer(src, event)
        disp('initialised')
    end
 
    function timerCallback(src, event)
        s = size(adc.Data,2);
        fprintf("val is %d\n", s);
        if s>5000 % number of samples to use for THD calculation
            if i==0
                adc.pause();
                thd(adc.Data(1,end-5000+1:end), 51200);
            end
            i=i+1;
            if i>=iproc
                adc.Data=[];
                adc.resume();
                i=0;
            end
        end
    end

    function stoppedTimer(src, event)
        fprintf("Finished, pausing\n");
        adc.pause();
        set(groot,'defaultAxesYLimMode','auto');
    end

end


