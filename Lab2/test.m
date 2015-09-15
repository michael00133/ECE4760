function [] = test(f, fs, p)
fc = 100;
En = p/fc;

t1 = [0:1/(fs):1/(2*fc)];
t2 = [1/(2*fc):1/fs:En];
L = length(t1) + length(t2);

y1 =(cos(2*pi*f*[t1 t2]));
x1 = [t1 t2];

figure
L = length(y1);
Y1 = fft(y1);
P21 = abs(Y1/L);
P11 = P21(1:L/2+1);
P11(2:end-1) = 2*P11(2:end-1);
axis = fs*(0:(L/2))/L;
plot(axis,20*log10(P11));
ylabel('power(dB)');
xlabel('frequency(Hz)');

