
udpObject = udpport("datagram",LocalHost="192.168.1.124",LocalPort=8080,EnablePortSharing=true)
try
    while true
        udpObject.NumDatagramsAvailable
        data = read(udpObject,udpObject.NumDatagramsAvailable,"uint32")
        
    end
catch
    disp('UDP Port stopped and deleted.');
end
