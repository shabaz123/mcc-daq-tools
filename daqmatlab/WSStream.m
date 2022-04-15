classdef WSStream < WebSocketClient
    %CLIENT Summary of this class goes here
    %   Detailed explanation goes here
    
    properties
        Data = []
    end
    
    methods
        function obj = WSStream(varargin)
            %Constructor
            obj@WebSocketClient(varargin{:});
        end
        
        function pause(obj)
            send(obj, 'pause');
        end
        
        function resume(obj)
            send(obj, 'resume');
        end
    end
    
    methods (Access = protected)
        function onOpen(obj,message)
            % This function simply displays the message received
            fprintf('%s\n',message);
        end
        
        function onTextMessage(obj,message)
            % This function simply displays the message received
            fprintf('Message received:\n%s\n',message);
        end
        
        function onBinaryMessage(obj,bytearray)
            % convert the bytes to an array of double values
            newdata = typecast(bytearray', 'double');
            % get odd and even values into separate rows of a matrix
            dual = [newdata(1:2:end); newdata(2:2:end)];
            % append to any existing data
            obj.Data = [obj.Data dual];
        end
        
        function onError(obj,message)
            % This function simply displays the message received
            fprintf('Error: %s\n',message);
        end
        
        function onClose(obj,message)
            % This function simply displays the message received
            fprintf('%s\n',message);
        end
    end
end

