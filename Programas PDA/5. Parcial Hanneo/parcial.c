/**
 * A simple 1-input to 1-output JACK client.
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <jack/jack.h>

jack_port_t *input_port;
jack_port_t *output_port;
jack_client_t *client;

double *buffer1;
double *buffer2;
int buffer_size;
int window_size;
double sample_rate;
int buffer_i1;
int buffer_i2;
int n_ventana;

int *hann(double *buffer)
{
  return buffer;
}

int jack_callback(jack_nframes_t nframes, void *arg)
{
  jack_default_audio_sample_t *in, *out;

  in = jack_port_get_buffer(input_port, nframes);
  out = jack_port_get_buffer(output_port, nframes);
  memcpy(out, in, nframes * sizeof(jack_default_audio_sample_t));

  // En main ya se llenó la primera mitad de b1 con ceros

  // Primera iteración
  if (n_ventana == 0)
  {
    // En este loop se rellena la primera mitad de b1 con la primer ventana
    // y la primera mitad del segundo buffer también con la primer ventana
    for (int i = 0; i < nframes; ++i)
    {
      buffer1[buffer_i1] = in[i];
      buffer2[buffer_i2] = in[i];
      buffer_i2++; // al final del loop buffer_i2=n_ventana
      buffer_i1++; // al final del loop buffer_i1=n_ventana y ya jamás se mueve
    }
    // Hanneas el buffer 1
    buffer1 = hann(buffer1);
  }
  // Para las siguientes ventanas solo rellenas el b2 y cuando esté lleno hannas el b2 y lo transfieres al b1
  else
  {
    for (int k = 0; k < nframes; ++k)
    {
      buffer2[buffer_i2] = in[k];
      buffer_i2++;
      // Cuando acabe de llenar el b2, hannificas, sumas con el b1
      // lo pasas a out
      // y después lo traspasas al b1
      if (buffer_i2 == buffer_size)
      {
        buffer2 = hann(buffer2); // Hañerizas el b2

        // Sumas la primer parte del b2 con la segunda del b1
        for (int l = 0; l < n_ventana; l++)
        {
          out[l] = buffer1[n_ventana + l] + buffer2[l];
        }

        // Copias el b2 en el b1
        for (int m = 0; m < buffer_size; m++)
        {
          buffer1[m] = buffer2[m];
        }
      }
      buffer_i2 %= buffer_size;
    }
  }

  n_ventana++;
  return 0;
}

/**
 * JACK calls this shutdown_callback if the server ever shuts down or
 * decides to disconnect the client.
 */
void jack_shutdown(void *arg)
{
  exit(1);
}

int main(int argc, char *argv[])
{
  float seconds = atof(argv[1]);

  const char *client_name = "in_to_out";
  jack_options_t options = JackNoStartServer;
  jack_status_t status;
  n_ventana = 0;
  /* open a client connection to the JACK server */
  client = jack_client_open(client_name, options, &status);
  if (client == NULL)
  {
    /* if connection failed, say why */
    printf("jack_client_open() failed, status = 0x%2.0x\n", status);
    if (status & JackServerFailed)
    {
      printf("Unable to connect to JACK server.\n");
    }
    exit(1);
  }

  /* if connection was successful, check if the name we proposed is not in use */
  if (status & JackNameNotUnique)
  {
    client_name = jack_get_client_name(client);
    printf("Warning: other agent with our name is running, `%s' has been assigned to us.\n", client_name);
  }

  /* tell the JACK server to call 'jack_callback()' whenever there is work to be done. */
  jack_set_process_callback(client, jack_callback, 0);

  /* tell the JACK server to call 'jack_shutdown()' if it ever shuts down,
     either entirely, or if it just decides to stop calling us. */
  jack_on_shutdown(client, jack_shutdown, 0);

  /* display the current sample rate. */
  printf("Engine sample rate: %d\n", jack_get_sample_rate(client));

  /* display the current window size. */
  window_size = jack_get_buffer_size(client);
  printf("Engine window size: %d\n", window_size);

  /* create the agent input port */
  input_port = jack_port_register(client, "input", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);

  /* create the agent output port */
  output_port = jack_port_register(client, "output", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

  /* check that both ports were created succesfully */
  if ((input_port == NULL) || (output_port == NULL))
  {
    printf("Could not create agent ports. Have we reached the maximum amount of JACK agent ports?\n");
    exit(1);
  }
  sample_rate = jack_get_sample_rate(client);
  buffer_size = 2 * window_size;
  buffer1 = malloc(buffer_size * sizeof(double));
  // ? Se inicializa a cero la primera mitad para poder hacer la suma desde la primer ventana
  for (int j = 0; j < window_size; j++)
  {
    buffer1[buffer_i1] = 0;
    buffer_i1++;
  }
  buffer2 = malloc(buffer_size * sizeof(double));
  /* Tell the JACK server that we are ready to roll.
     Our jack_callback() callback will start running now. */
  if (jack_activate(client))
  {
    printf("Cannot activate client.");
    exit(1);
  }

  printf("Agent activated.\n");

  /* Connect the ports.  You can't do this before the client is
   * activated, because we can't make connections to clients
   * that aren't running.  Note the confusing (but necessary)
   * orientation of the driver backend ports: playback ports are
   * "input" to the backend, and capture ports are "output" from
   * it.
   */
  printf("Connecting ports... ");

  /* Assign our input port to a server output port*/
  // Find possible output server port names
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
