% Se hace una señal completa que se puede dividir en ventanas para ver cómo se podría hacer un promedio o algo por el estilo de los delays
## Se puede poner con todos los parámetros del modelo real

max_delay = 15;                 %maximum delay possible in samples

delay_x1 = 0;                   %delay of signal 1 in mic 1
delay_x2 = 10;                  %delay of signal 2 in mic 1

delay_y1 = 5;                   %delay of signal 1 in mic 2
delay_y2 = 0;                   %delay of signal 2 in mic 2

noise_w = .5;                    %noise presence (between 0 and 1)
reverb_w = .3;                %reverb presence (between 0 and 1)

K = 100;                        %signal size in samples
%%%%%%%%

t = 1:K;                        %time vector

%original signals
%s1 = e.^(-pi*((t+1).^2)/10)+3;
s1=cos(10*2*pi*t/K);
s2=cos(3*2*pi*(t-20)/K);




%microphones (input signals)
x = [zeros(1,delay_x1) s1(1:end-delay_x1)] + [zeros(1,delay_x2) s2(1:end-delay_x2)];
y = [zeros(1,delay_y1) s1(1:end-delay_y1)] + [zeros(1,delay_y2) s2(1:end-delay_y2)];
##
######adding reverberation
##x = add_reverb(x,reverb_w);
##y = add_reverb(y,reverb_w);
##
######adding noise
##x = x + randn(1,K)*noise_w/10;
##y = y + randn(1,K)*noise_w/10;

figure(1); plot(t,x,t,y); title('Senales de entrada')

% Ver si se agarra bien el delay para señales no periódicas en los dos esquemas de PHAT y CCV
% No agarra muy bien, hay intervalos feos. Esto es como si fuera una ventana entonces se pueden usar



%%% GCC

%centering
x_c = x - mean(x);
y_c = y - mean(y);

## Aplicar WOLA a la señal de entrada antes de hacerle la transformada
##wola=sqrt(1-cos(2*pi*t/K));
##x_c=wola.*x_c;
##y_c=wola.*y_c;
##figure(4); plot(t,x_c,t,y_c);axis([0,K]); title('WOLA xy');


%fft'ing input signals
%fft'ing input signals
x_f = fft(x_c);
y_f = fft(y_c);




%cross-correlation via Pearson
ccv_f = x_f.*conj(y_f);
norm(x_c)*norm(y_c)
ccv = real(ifft(ccv_f))/(norm(x_c)*norm(y_c));
ccv = fftshift(ccv);
mid = (size(ccv,2)/2)+1;
figure(2); plot(-max_delay:max_delay,ccv(mid-max_delay:mid+max_delay)); axis([-max_delay max_delay -1 1]); title('Correlacion Cruzada - Pearson')

%cross-correlation via GCC-PHAT
ccv_fp = x_f.*conj(y_f)./(abs(x_f.*conj(y_f))); % El denominador es un vector de normas

% Para quitar los nan que vengan de un denominador igual a 0. Igual se puede agregar un offset al denominador
for i=1:length(ccv_fp)
  if isnan(ccv_fp(i))
     ccv_fp(i)=0
   endif
endfor
ccvp = real(ifft(ccv_fp));
ccvp = fftshift(ccvp);
mid = (size(ccvp,2)/2)+1;
figure(3); plot(-max_delay:max_delay,ccvp(mid-max_delay:mid+max_delay)); axis([-max_delay max_delay -0.2 1]); title('GCC-PHAT')

