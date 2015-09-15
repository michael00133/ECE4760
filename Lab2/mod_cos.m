f = 1000;
<<<<<<< HEAD:Lab2/modulate.m
fc = 300;
mult = 10;
=======
fc = 100;
mult = 10;
axis = (mult*f/2)*(axis-0.5);
>>>>>>> 5bed4401f28f46fc902ce479daf7b1b478527b62:Lab2/mod_cos.m
t1 = [0:1/(mult*f):1/(2*fc)];
t2 = [1/(2*fc):1/(mult*f):8/f];
L = length(t1) + length(t2);

x1 = cos(2*pi*f*[t1 t2]);
x2 = [0.5*(1-cos(2*pi*fc*t1)) ones(1,length(t2))];

figure
plot([t1 t2],x1, [t1 t2], x2, [t1 t2], x1.*x2);
legend('Original Wave','Modulator','Modulated Wave');

figure
Y = fft(x1.*x2);
Y1 = fft(x1);
Y2 = fft(x2);
P2 = abs(Y/L);
P21 = abs(Y1/L);
P22 = abs(Y2/L);
P1 = P2(1:L/2+1);
P11 = P21(1:L/2+1);
P12 = P22(1:L/2+1);
P1(2:end-1) = 2*P1(2:end-1);
P11(2:end-1) = 2*P11(2:end-1);
P12(2:end-1) = 2*P12(2:end-1);
axis = mult*f*(0:(L/2))/L;
plot(axis,P1,axis,P11,axis,P12);
legend('Modulated Wave', 'Original Wave', 'Modulator');
