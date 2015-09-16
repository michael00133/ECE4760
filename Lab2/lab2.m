clear
close all
legendinfo={};


for i=5:20
Fs = 1000*i;            % Sampling frequency
T = 1/Fs;             % Sampling period
t=(0:T:1/1477);
[St,S]=stairs(t,sin(2*pi*1477*t));

Noise=0.5*rand(size(St));


L=length(St);
% 
% figure
% plot(St,S);
X=S+Noise;
Y=fft(S);
YN=fft(Noise);
P2 = abs(Y/L);
P1 = P2(1:L/2+1);
P1(2:end-1) = 2*P1(2:end-1);
P4 = abs(YN/L);
P3 = P4(1:L/2+1);
P3(2:end-1) = 2*P3(2:end-1);
f = 2*Fs*(0:(L/2))/L;
figure(1)
plot(St,S);
hold all
figure(2)
plot(f,20*log10(P1))
legendinfo=[legendinfo num2str(1000*i)];
hold all
end

legend(legendinfo)

% figure
% plot(f,20*log10(P3))
% title('Single-Sided Amplitude Spectrum of X(t)')
% xlabel('f (Hz)')
% ylabel('|P1(f)|')

%SNR=max(20*log10(P1))+mean(20*log10(P3))
