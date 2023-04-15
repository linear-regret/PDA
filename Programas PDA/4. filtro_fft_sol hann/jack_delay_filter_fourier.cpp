/*
 * A simple example of how to do FFT with FFTW3 and JACK.
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <jack/jack.h>

// Include FFTW header
#include <complex>
#include <fftw3.h>

std::complex<double> *i_fft, *i_time, *o_fft, *o_time, *delay_array;
fftw_plan i_forward, o_inverse;

jack_port_t *input_port;
jack_port_t *input_port2;
jack_port_t *output_port;
jack_port_t *output_port2;
jack_client_t *client;

double sample_rate;
double delay;
// wola buffers
jack_default_audio_sample_t *b1, *b2, *b_ref, *b_temp;
double *hann_wola; // sqrt of a hann window
double *freqs;
int fft_size; // 2 * nframes

void filter(jack_default_audio_sample_t *b)
{
  /*
    Toma una señal, la multiplica por wola, luego la pasa a los complejos, le aplica el delay, la regresa a los reales y le vuelve a aplicar wola
    Como le pasas el apuntador, no requieres regresar nada
  */
  // do wola analysis and apply fft
  for (int i = 0; i < fft_size; ++i)
  {
    i_time[i] = b[i] * hann_wola[i];
  }
  fftw_execute(i_forward); //* Función de fftw3 para ejecutar la transformada sobre i_time y guardarlo en i_fft

  // do filtering
  o_fft[0] = i_fft[0]; // pass through DC untouched
  for (int i = 1; i < fft_size; ++i)
  {
    /* if(fabs(freqs[i]) < 2000 || fabs(freqs[i]) > 5000){ Filtro de frecuencias
    //   o_fft[i] = 0.0;
    // }else{
    //   o_fft[i] = i_fft[i];
    }*/

    o_fft[i] = i_fft[i] * std::exp(std::complex<double>(0, 2 * M_PI * freqs[i] * delay));
  }
  // Multiplicación para obtener PHAT
  // fftw2_execute(i_forward2)
  // Multiplicación de los vectores entre sus normas
  // Qué se hace con el resultado?

  // apply ifft and wola synthesis
  fftw_execute(o_inverse); //* Función de fftw3 para ejecutar la transformada inversa sobre o_fft y guardarla en o_time

  for (int i = 0; i < fft_size; ++i)
  {
    b[i] = real(o_time[i]) / fft_size; // fftw3 requiere normalizar su salida real de esta manera
    b[i] *= hann_wola[i];
  }
}

int jack_callback(jack_nframes_t nframes, void *arg)
{
  jack_default_audio_sample_t *in, *out, *out2;
  int i, j;

  in = (jack_default_audio_sample_t *)jack_port_get_buffer(input_port, nframes);
  out = (jack_default_audio_sample_t *)jack_port_get_buffer(output_port, nframes);   //* La señal retrasada
  out2 = (jack_default_audio_sample_t *)jack_port_get_buffer(output_port2, nframes); // * La señal original (retrasada un buffer para permitir el wola en la otra)

  // Puerto de referencia
  // Lo hacemos en un buffer porque se retrasa una ventana el procesamiento
  for (i = 0; i < nframes; ++i)
  {
    out2[i] = b_ref[i];
    b_ref[i] = in[i];
  }

  // copy in to 2nd half of b2
  for (i = 0, j = nframes; i < nframes; ++i, ++j)
  {
    b2[j] = in[i];
  }

  // filter/hann b2
  filter(b2);

  // overlap and add, to out
  for (i = 0, j = nframes; i < nframes; ++i, ++j)
  {
    out[i] = b1[j] + b2[i];
  }

  // * copy b2 to b1. Dos maneras análogas, la segunda toma mucho menos tiempo.
  // for (i = 0; i < fft_size; ++i)
  // {
  //   b1[i] = b2[i];
  // }
  b_temp = b2;
  // Esta linea es necesaria?
  b2 = b1;
  b1 = b_temp;

  // copy in to 1st half of b2
  for (i = 0; i < nframes; i++)
  {
    b2[i] = in[i];
  }

  return 0;
}

void jack_shutdown(void *arg)
{
  exit(1);
}

int main(int argc, char *argv[])
{
  /*
  Se define arreglo de wola para multiplicar, se define el delay que va en la fase para retrasar.
  Se inicializan en 0 los buffers a los que se les hace el overlap and add
  Se inicializan los callbacks de fft que pasan al espacio de frecuencias y luego de regreso.
  Se inicializan los agentes, etc
  */
  const char *client_name = "jack_fft";
  jack_options_t options = JackNoStartServer;
  jack_status_t status;
  // Poner argc para checar num parametros

  delay = atof(argv[1]); // Como va a ser delay en frecuencia, sólo se puede hacer del tamaño de las ventanas. Y tiene tiempo mínimo del tamaño de ventana 1/framerate
  printf("%f\n", delay);
  client = jack_client_open(client_name, options, &status);

  if (client == NULL)
  {
    printf("jack_client_open() failed, status = 0x%2.0x\n", status);
    if (status & JackServerFailed)
    {
      printf("Unable to connect to JACK server.\n");
    }
    exit(1);
  }

  if (status & JackNameNotUnique)
  {
    client_name = jack_get_client_name(client);
    printf("Warning: other agent with our name is running, `%s' has been assigned to us.\n", client_name);
  }

  jack_set_process_callback(client, jack_callback, 0);

  jack_on_shutdown(client, jack_shutdown, 0);

  printf("Sample rate: %d\n", jack_get_sample_rate(client));
  printf("Window size: %d\n", jack_get_buffer_size(client));

  sample_rate = (double)jack_get_sample_rate(client);
  int nframes = jack_get_buffer_size(client);

  fft_size = 2 * nframes; // * El tamaño de la transformada es de dos veces los frames para permitir hacer el traslape
  b_ref = (jack_default_audio_sample_t *)malloc(sizeof(jack_default_audio_sample_t) * nframes);
  delay_array = (std::complex<double> *)fftw_malloc(sizeof(std::complex<double>) * fft_size);

  // prepare freqs array
  // * La frecuencia unitaria es el inverso del periodo=tamaño muestra / sample_rate
  double f1 = sample_rate / fft_size;

  freqs = (double *)malloc(sizeof(double) * fft_size);

  freqs[0] = 0;
  for (int i = 1; i < fft_size / 2; ++i)
  {
    freqs[i] = i * f1;
    freqs[fft_size - i] = -freqs[i];
  }                                      // | 0| f1=|NS | f1|
  freqs[fft_size / 2] = sample_rate / 2; // * La frecuencia de nyquist-shannon

  // prepare freqs array. Este no se usa, es solo para ver que vamos a tener desde la frecuencia de sampleo hasta la de NS
  hann_wola = (double *)malloc(sizeof(double) * fft_size);
  b1 = (jack_default_audio_sample_t *)malloc(sizeof(jack_default_audio_sample_t) * fft_size);
  b2 = (jack_default_audio_sample_t *)malloc(sizeof(jack_default_audio_sample_t) * fft_size);
  for (int i = 0; i < fft_size; ++i)
  {
    hann_wola[i] = sqrt(0.5 * (1 - cos(2 * M_PI * i / fft_size)));
    b1[i] = 0.0;
    b2[i] = 0.0;
    delay_array[i] = std::exp(std::complex<double>(0, 2 * M_PI * freqs[i] * delay));
  }

  // preparing FFTW3 buffers
  i_fft = (std::complex<double> *)fftw_malloc(sizeof(std::complex<double>) * fft_size);
  i_time = (std::complex<double> *)fftw_malloc(sizeof(std::complex<double>) * fft_size);
  o_fft = (std::complex<double> *)fftw_malloc(sizeof(std::complex<double>) * fft_size);
  o_time = (std::complex<double> *)fftw_malloc(sizeof(std::complex<double>) * fft_size);

  i_forward = fftw_plan_dft_1d(fft_size, reinterpret_cast<fftw_complex *>(i_time), reinterpret_cast<fftw_complex *>(i_fft), FFTW_FORWARD, FFTW_MEASURE);
  o_inverse = fftw_plan_dft_1d(fft_size, reinterpret_cast<fftw_complex *>(o_fft), reinterpret_cast<fftw_complex *>(o_time), FFTW_BACKWARD, FFTW_MEASURE);

  /* create the agent input port */
  input_port = jack_port_register(client, "input1", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
  input_port2 = jack_port_register(client, "input2", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);

  /* create the agent output port */
  output_port = jack_port_register(client, "output1", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
  output_port2 = jack_port_register(client, "output2", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

  /* check that both ports were created succesfully */
  if ((input_port == NULL) || (input_port2 == NULL) || (output_port == NULL) || (output_port2 == NULL))
  {
    printf("Could not create agent ports. Have we reached the maximum amount of JACK agent ports?\n");
    exit(1);
  }

  /* Tell the JACK server that we are ready to roll.
     Our jack_callback() callback will start running now. */
  if (jack_activate(client))
  {
    printf("Cannot activate client.");
    exit(1);
  }

  printf("Agent activated.\n");

  printf("Connecting ports... ");

  const char **serverports_names;
  serverports_names = jack_get_ports(client, NULL, NULL, JackPortIsPhysical | JackPortIsOutput);
  if (serverports_names == NULL)
  {
    printf("No available physical capture (server output) ports.\n");
    exit(1);
  }
  // Connect the first available to our input port
  if (jack_connect(client, serverports_names[0], jack_port_name(input_port)))
  {
    printf("Cannot connect input port.\n");
    exit(1);
  }
  // free serverports_names variable for reuse in next part of the code
  free(serverports_names);

  /* Assign our output port to a server input port*/
  // Find possible input server port names
  serverports_names = jack_get_ports(client, NULL, NULL, JackPortIsPhysical | JackPortIsInput);
  if (serverports_names == NULL)
  {
    printf("No available physical playback (server input) ports.\n");
    exit(1);
  }
  // Connect the first available to our output port
  if (jack_connect(client, jack_port_name(output_port), serverports_names[0]))
  {
    printf("Cannot connect output ports.\n");
    exit(1);
  }
  if (jack_connect(client, jack_port_name(output_port), "baudline:in_1"))
  {
    printf("Cannot connect output ports.\n");
    exit(1);
  }

  if (jack_connect(client, jack_port_name(output_port2), "baudline:in_2"))
  {
    printf("Cannot connect output ports.\n");
    exit(1);
  }
  // free serverports_names variable, we're not going to use it again
  free(serverports_names);

  printf("done.\n");
  /* keep running until stopped by the user */
  sleep(-1);

  /* this is never reached but if the program
     had some other way to exit besides being killed,
     they would be important to call.
  */
  jack_client_close(client);
  exit(0);
}
