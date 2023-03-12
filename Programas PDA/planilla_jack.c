/*
 * Descripción del programa
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <jack/jack.h>

jack_port_t *input_port;
jack_port_t *output_port;
jack_client_t *client;
double sample_rate;

int jack_callback(jack_nframes_t nframes, void *arg)
{
    jack_default_audio_sample_t *in, *out;

    in = jack_port_get_buffer(input_port, nframes);
    out = jack_port_get_buffer(output_port, nframes);

    return 0;
}

void jack_shutdown(void *arg)
{
    exit(1);
}

int main(int argc, char *argv[])
{

    const char *client_name = "agente_smith";
    jack_options_t options = JackNoStartServer;
    jack_status_t status;

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
    /* Tell the JACK server that we are ready to roll. Our jack_callback() callback will start running now. */
    if (jack_activate(client))
    {
        printf("Cannot activate client.");
        exit(1);
    }

    printf("Agent activated.\n");

    /* Connect the ports.  You can't do this before the client is activated */
    printf("Connecting ports... ");

    /* Assign our input port to a server output port. Find possible output server port names */
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

    /* this is never reached but if the program had some other way to exit besides being killed, they would be important to call.
     */
    jack_client_close(client);
    exit(0);
}
