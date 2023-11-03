udpObject = udpport("datagram","IPV4",LocalHost="192.168.1.124",LocalPort=8080);
start(udpObject);
disp('UDP Port started.');
try
    while true
        dataReceived = receive(udpObject);
        % Process the received data here
        disp(['Received data: ', char(dataReceived)]);
    end
catch
    stop(udpObject);
    delete(udpObject);
    disp('UDP Port stopped and deleted.');
end
