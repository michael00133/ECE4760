close all
Fs = 3000;            % Sampling frequency
T = 1/Fs;             % Sampling period
L = 1000;             % Length of signal
t = (0:L-1)*T;        % Time vector
Noise=rand(size(t));
S=sin(2*pi*1477*t);
figure
plot(t,S);
X=S+Noise;
Y=fft(X);
YN=fft(Noise);
P2 = abs(Y/L);
P1 = P2(1:L/2+1);
P1(2:end-1) = 2*P1(2:end-1);
P4 = abs(YN/L);
P3 = P4(1:L/2+1);
P3(2:end-1) = 2*P3(2:end-1);
f = Fs*(0:(L/2))/L;
plot(f,20*log10(P1))
figure
plot(f,20*log10(P3))
title('Single-Sided Amplitude Spectrum of X(t)')
xlabel('f (Hz)')
ylabel('|P1(f)|')

SNR=max(20*log10(P1))+mean(20*log10(P3))
