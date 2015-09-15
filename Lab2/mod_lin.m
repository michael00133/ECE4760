function [] = mod_lin(f, fs, p)
fc = 100;
En = p/fc;

t1 = [0:1/(fs):1/(2*fc)];
t2 = [1/(2*fc):1/fs:En];
L = length(t1) + length(t2);

[x1, y1] = stairs(cos(2*pi*f*[t1 t2]));
[x2, y2] = stairs([(2*fc)*t1 ones(1,length(t2))]);
x1 = (x1/L)*(En);
x2 = (x2/L)*(En);

figure
plot(x1,y1, x2,y2, x1, y1.*y2);
legend('Original Wave','Modulator','Modulated Wave');
xlabel('time(s)');
ylabel('Amplitude');

figure
Y = fft(y1.*y2);
Y1 = fft(y1);
Y2 = fft(y2);
P2 = abs(Y/L);
P21 = abs(Y1/L);
P22 = abs(Y2/L);
P1 = P2(1:L/2+1);
P11 = P21(1:L/2+1);
P12 = P22(1:L/2+1);
P1(2:end-1) = 2*P1(2:end-1);
P11(2:end-1) = 2*P11(2:end-1);
P12(2:end-1) = 2*P12(2:end-1);
axis = fs*(0:(L/2))/L;
plot(axis,20*log10(P1),axis,20*log10(P11),axis,20*log10(P12),[0,max(axis)],[-20, -20]);
legend('Modulated Wave', 'Original Wave', 'Modulator');
ylabel('power(dB)');
xlabel('frequency(Hz)');

