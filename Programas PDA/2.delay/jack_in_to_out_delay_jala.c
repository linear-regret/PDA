/**
 * * Este programa tiene como inputs la cantidad de segundos y como ouput dos señales, una en tiempo real y la otra desfasada el número de segundos que especificamos
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <jack/jack.h>

jack_port_t *input_port1;
jack_port_t *input_port2;
jack_port_t *output_port1;
jack_port_t *output_port2;
jack_client_t *client;
double *buffer;
int buffer_size;
double sample_rate;
int buffer_i;

int jack_callback(jack_nframes_t nframes, void *arg)
{
  jack_default_audio_sample_t *in1, *in2, *out1, *out2;

  in1 = jack_port_get_buffer(input_port1, nframes);
  in2 = jack_port_get_buffer(input_port2, nframes);
  out1 = jack_port_get_buffer(output_port1, nframes);
  out2 = jack_port_get_buffer(output_port2, nframes);

  for (int i = 0; i < nframes; ++i)
  {
    // * Lee del buffer, al principio vacío
    out1[i] = buffer[buffer_i];
    // Escribe en buffer lo que se leerá tiempo después
    buffer[buffer_i] = in1[i];
    // Incrementa índice del buffer
    buffer_i++;
    //  Da la vueta al buffer
    buffer_i %= buffer_size;

    out2[i] = in2[i];
  }
  return 0;
}

void jack_shutdown(void *arg)
{
  exit(1);
}

int main(int argc, char *argv[])
{
  float seconds = atof(argv[1]);

  const char *client_name = "smith";
  jack_options_t options = JackNoStartServer;
  jack_status_t status;

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
  printf("Engine window size: %d\n", jack_get_buffer_size(client));

  /* create the agent input port */
  input_port1 = jack_port_register(client, "input1", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
  input_port2 = jack_port_register(client, "input2", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);

  /* create the agent output port */
  output_port1 = jack_port_register(client, "output1", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
  output_port2 = jack_port_register(client, "output2", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

  /* check that both ports were created succesfully */
  if ((input_port1 == NULL) || (input_port2 == NULL) || (output_port1 == NULL) || (output_port2 == NULL))
  {
    printf("Could not create agent ports. Have we reached the maximum amount of JACK agent ports?\n");
    exit(1);
  }
  sample_rate = jack_get_sample_rate(client);
  buffer_size = seconds * sample_rate;
  buffer = malloc(buffer_size * sizeof(double));
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
  // serverports_names = jack_get_ports(client, NULL, NULL, JackPortIsPhysical | JackPortIsOutput);
  serverports_names = jack_get_ports(client, NULL, NULL, JackPortIsPhysical | JackPortIsOutput);
  if (serverports_names == NULL)
  {
    printf("No available physical capture (server output) ports.\n");
    exit(1);
  }
  // Connect the first available to our input port
  if (jack_connect(client, serverports_names[0], jack_port_name(input_port1)))
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
  if (jack_connect(client, jack_port_name(output_port1), serverports_names[0]))
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
