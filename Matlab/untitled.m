clear u
u = udpport("Byte", "IPV4", "LocalPort", 8080, "EnablePortSharing", true);
filename = 'output.wav';
fileFormat = 'wav';
sampleRate = 24000;
while true
    x = read(u,240*100,"int32");
    x = bitshift(x, -8, "int32");
    x = x / 8388608.0;
    player = audioplayer(x, sampleRate, 24);
    play(player);
    audiowrite(filename, x, sampleRate, 'BitsPerSample', 24);
    pause(player.TotalSamples/player.SampleRate);
    % Clean up
    %stop(player);
end