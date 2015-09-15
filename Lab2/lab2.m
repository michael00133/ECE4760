clear
close all
L = 10;             % Length of signal

hold all

for i=6:10
Fs = 1500*i;            % Sampling frequency
T = 1/Fs;             % Sampling period
t=(0:L-1)*T;
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
f = Fs*(0:(L/2))/L;
figure(1)
plot(f,20*log10(P1))
figure
plot(St,S);
legendinfo{i}=num2str(i);
end
figure(1)
legend(legendinfo)
% figure
% plot(f,20*log10(P3))
% title('Single-Sided Amplitude Spectrum of X(t)')
% xlabel('f (Hz)')
% ylabel('|P1(f)|')

%SNR=max(20*log10(P1))+mean(20*log10(P3))
