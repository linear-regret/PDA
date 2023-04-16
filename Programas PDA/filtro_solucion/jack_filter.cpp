/**
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

std::complex<double> *i_fft, *i_time, *o_fft, *o_time;
fftw_plan i_forward, o_inverse;

jack_port_t *input_port;
jack_port_t *output_port;
jack_port_t *output_port2;
jack_client_t *client;

double sample_rate;

//wola buffers
jack_default_audio_sample_t *b1, *b2, *b_ref;
double *hann_wola; //sqrt of a hann window
double *freqs;
int fft_size; // 2 * nframes

void filter(jack_default_audio_sample_t *b){
	int i;

	//do wola analysis and apply fft
  for(i = 0; i < fft_size; ++i){
    i_time[i] = b[i]*hann_wola[i];
  }
  fftw_execute(i_forward);
	
	//do filtering
	o_fft[0] = i_fft[0]; // pass through DC untouched
  for(i = 1; i < fft_size; ++i){
  	if(fabs(freqs[i]) < 2000 || fabs(freqs[i]) > 5000){
	    o_fft[i] = 0.0;
  	}else{
	    o_fft[i] = i_fft[i];
  	}
  }
	
	//apply ifft and wola synthesis
  fftw_execute(o_inverse);
  for(i = 0; i < fft_size; ++i){
    b[i] = real(o_time[i])/fft_size; //fftw3 requiere normalizar su salida real de esta manera
    b[i] *= hann_wola[i]; 
  }
}

int jack_callback (jack_nframes_t nframes, void *arg){
  jack_default_audio_sample_t *in, *out, *out2;
  int i,j;
  
  in = (jack_default_audio_sample_t *)jack_port_get_buffer (input_port, nframes);
  out = (jack_default_audio_sample_t *)jack_port_get_buffer (output_port, nframes);
  out2 = (jack_default_audio_sample_t *)jack_port_get_buffer (output_port2, nframes);

  // puerto de referencia
  for(i = 0; i < nframes; ++i){
    out2[i] = b_ref[i];
    b_ref[i] = in[i];
  }
  
  // copy in to 2nd half of b2
  for(i = 0, j=nframes; i < nframes; ++i,++j){
    b2[j] = in[i];
  }
  
  // filter/hann b2
  filter(b2);
  
  // overlap and add, to out
  for(i = 0, j=nframes; i < nframes; ++i,++j){
    out[i] = b1[j]+b2[i];
  }
  
  // copy b2 to b1
  for (i = 0; i < fft_size; ++i){
  	b1[i] = b2[i];
  }	
  
  // copy in to 1st half of b2
  for(i = 0; i < nframes; i++){
    b2[i] = in[i];
  }
  
  return 0;
}


/**
 * JACK calls this shutdown_callback if the server ever shuts down or
 * decides to disconnect the client.
 */
void jack_shutdown (void *arg){
  exit (1);
}


int main (int argc, char *argv[]) {
  const char *client_name = "jack_fft";
  jack_options_t options = JackNoStartServer;
  jack_status_t status;
  
  /* open a client connection to the JACK server */
  client = jack_client_open (client_name, options, &status);
  if (client == NULL){
    /* if connection failed, say why */
    printf ("jack_client_open() failed, status = 0x%2.0x\n", status);
    if (status & JackServerFailed) {
      printf ("Unable to connect to JACK server.\n");
    }
    exit (1);
  }
  
  /* if connection was successful, check if the name we proposed is not in use */
  if (status & JackNameNotUnique){
    client_name = jack_get_client_name(client);
    printf ("Warning: other agent with our name is running, `%s' has been assigned to us.\n", client_name);
  }
  
  /* tell the JACK server to call 'jack_callback()' whenever there is work to be done. */
  jack_set_process_callback (client, jack_callback, 0);
  
  
  /* tell the JACK server to call 'jack_shutdown()' if it ever shuts down,
     either entirely, or if it just decides to stop calling us. */
  jack_on_shutdown (client, jack_shutdown, 0);
  
  
  /* display the current sample rate. */
  printf ("Sample rate: %d\n", jack_get_sample_rate (client));
  printf ("Window size: %d\n", jack_get_buffer_size (client));
  sample_rate = (double)jack_get_sample_rate(client);
  int nframes = jack_get_buffer_size (client);
  int i;
  fft_size = 2*nframes;
  
  b_ref = (jack_default_audio_sample_t *) malloc(sizeof(jack_default_audio_sample_t) * nframes);
  
  // prepare freqs array
  freqs = (double *) malloc(sizeof(double) * fft_size);
  hann_wola = (double *) malloc(sizeof(double) * fft_size);
  b1 = (jack_default_audio_sample_t *) malloc(sizeof(jack_default_audio_sample_t) * fft_size);
  b2 = (jack_default_audio_sample_t *) malloc(sizeof(jack_default_audio_sample_t) * fft_size);
  for (i = 0; i < fft_size; ++i){
  	hann_wola[i] = sqrt(0.5 * (1 - cos(2*M_PI*i/fft_size)));
  	b1[i] = 0.0;
  	b2[i] = 0.0;
  }
  
  // prepare freqs array
  double f1 = sample_rate/fft_size;
  freqs[0] = 0;
  for (i = 1; i < fft_size/2; ++i){
  	freqs[i] = i*f1;
  	freqs[fft_size-i] = -freqs[i];
  }
  freqs[fft_size/2] = sample_rate/2;
  
  //preparing FFTW3 buffers
  i_fft = (std::complex<double>*) fftw_malloc(sizeof(std::complex<double>) * fft_size);
  i_time = (std::complex<double>*) fftw_malloc(sizeof(std::complex<double>) * fft_size);
  o_fft = (std::complex<double>*) fftw_malloc(sizeof(std::complex<double>) * fft_size);
  o_time = (std::complex<double>*) fftw_malloc(sizeof(std::complex<double>) * fft_size);
  
  i_forward = fftw_plan_dft_1d(fft_size, reinterpret_cast<fftw_complex*>(i_time), reinterpret_cast<fftw_complex*>(i_fft), FFTW_FORWARD, FFTW_MEASURE);
  o_inverse = fftw_plan_dft_1d(fft_size, reinterpret_cast<fftw_complex*>(o_fft), reinterpret_cast<fftw_complex*>(o_time), FFTW_BACKWARD, FFTW_MEASURE);
  
  /* create the agent input port */
  input_port = jack_port_register (client, "input", JACK_DEFAULT_AUDIO_TYPE,JackPortIsInput, 0);
  
  /* create the agent output port */
  output_port = jack_port_register (client, "output1",JACK_DEFAULT_AUDIO_TYPE,JackPortIsOutput, 0);
  output_port2 = jack_port_register (client, "output2",JACK_DEFAULT_AUDIO_TYPE,JackPortIsOutput, 0);
  
  /* check that both ports were created succesfully */
  if ((input_port == NULL) || (output_port == NULL) || (output_port2 == NULL)) {
    printf("Could not create agent ports. Have we reached the maximum amount of JACK agent ports?\n");
    exit (1);
  }
  
  /* Tell the JACK server that we are ready to roll.
     Our jack_callback() callback will start running now. */
  if (jack_activate (client)) {
    printf ("Cannot activate client.");
    exit (1);
  }
  
  printf ("Agent activated.\n");
  
  /* Connect the ports.  You can't do this before the client is
   * activated, because we can't make connections to clients
   * that aren't running.  Note the confusing (but necessary)
   * orientation of the driver backend ports: playback ports are
   * "input" to the backend, and capture ports are "output" from
   * it.
   */
  printf ("Connecting ports... ");
   
  /* Assign our input port to a server output port*/
  // Find possible output server port names
  const char **serverports_names;
  serverports_names = jack_get_ports (client, NULL, NULL, JackPortIsPhysical|JackPortIsOutput);
  if (serverports_names == NULL) {
    printf("No available physical capture (server output) ports.\n");
    exit (1);
  }
  // Connect the first available to our input port
  if (jack_connect (client, serverports_names[0], jack_port_name (input_port))) {
    printf("Cannot connect input port.\n");
    exit (1);
  }
  // free serverports_names variable for reuse in next part of the code
  free (serverports_names);
  
  
  /* Assign our output port to a server input port*/
  // Find possible input server port names
  serverports_names = jack_get_ports (client, NULL, NULL, JackPortIsPhysical|JackPortIsInput);
  if (serverports_names == NULL) {
    printf("No available physical playback (server input) ports.\n");
    exit (1);
  }
  // Connect the first available to our output port
  if (jack_connect (client, jack_port_name (output_port), serverports_names[0])) {
    printf ("Cannot connect output ports.\n");
    exit (1);
  }
  // free serverports_names variable, we're not going to use it again
  free (serverports_names);
  
  
  printf ("done.\n");
  /* keep running until stopped by the user */
  sleep (-1);
  
  
  /* this is never reached but if the program
     had some other way to exit besides being killed,
     they would be important to call.
  */
  jack_client_close (client);
  exit (0);
}
