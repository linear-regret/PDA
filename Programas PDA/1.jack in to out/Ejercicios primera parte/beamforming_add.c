/**
 * A simple 1-input to 1-output JACK client.
 * Hace un agente que suma las contribuciones de los dos micrófonos
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <jack/jack.h>

jack_port_t *input_port;
jack_port_t *output_port;
jack_port_t *output_port2;
jack_client_t *client;

/**
 * The process callback for this JACK application is called in a
 * special realtime thread once for each audio cycle.
 *
 * This client does nothing more than copy data from its input
 * port to its output port. It will exit when stopped by
 * the user (e.g. using Ctrl-C on a unix-ish operating system)
 */

int jack_callback(jack_nframes_t nframes, void *arg)
{
  jack_default_audio_sample_t *in, *out, *out2;

  /*
   * jack_port_get_buffer adquiere los buffers de un puerto, requiere variable de puerto y el tamaño del buffer
   */
  in = jack_port_get_buffer(input_port, nframes);
  out = jack_port_get_buffer(output_port, nframes);
  out2 = jack_port_get_buffer(output_port2, nframes);
  // memcpy(out, in, nframes * sizeof(jack_default_audio_sample_t)); //  *Copia datos de un segmento a memoria a otra. Es equivalente a hacer el for siguiente

  for (int i = 0; i < nframes; ++i)
  {
    out[i] = in[i];
    out2[i] = 2 * in[i];
  }
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
  const char *client_name = "in_to_out";
  jack_options_t options = JackNoStartServer; // * Opciones de comienzo. JackNoStartServer le dice que no incie el servidor si no está ya abierto. Nosotros lo abrimos con qjackctl antes. Más opciones en https://jackaudio.org/api/types_8h.html#a396617de2ef101891c51346f408a375e
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
     either entirely, or if it just decides to stop calling us.
     * Cómo se podría llamar a otro agente cuando este muera?
      */
  jack_on_shutdown(client, jack_shutdown, 0);

  /* display the current sample rate. */
  printf("Engine sample rate: %d\n", jack_get_sample_rate(client));

  /* display the current window size. */
  printf("Engine window size: %d\n", jack_get_buffer_size(client));

  // * Los puertos son distintos a los in outs. Regresa el apuntador del puerto, para que se pueda modificar dentro de las funciones que se le pasen. Se pueden registrar muchos puertos, es como el servidor de la computadora, que maneja las solicitudes y respuestas
  /* create the agent input port */
  input_port = jack_port_register(client, "input", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
  /* create the agent output port */
  output_port = jack_port_register(client, "output", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
  output_port2 = jack_port_register(client, "output2", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

  /* check that both ports were created succesfully */
  if ((input_port == NULL) || (output_port == NULL))
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
  // * Aquí están los output del server que son los micrófonos.
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
  if (jack_connect(client, jack_port_name(output_port), serverports_names[1]))
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
