#include "m_pd.h"

#include <math.h>

/* The global variables
 * *******************************************************/
#define MINIMUM_MAX_DELAY 0.0
#define DEFAULT_MAX_DELAY 1000.0
#define MAXIMUM_MAX_DELAY 10000.0

#define MINIMUM_DELAY 0.0
#define DEFAULT_DELAY 100.0
#define MAXIMUM_DELAY MAXIMUM_MAX_DELAY

#define MINIMUM_FEEDBACK 0.0
#define DEFAULT_FEEDBACK 0.3
#define MAXIMUM_FEEDBACK 0.9999

/* The object structure
 * *******************************************************/
typedef struct _vdelay {
    t_object obj;
    t_float x_f;

    float max_delay;
    float delay;
    float feedback;

    float fs;

    long delay_length;
    long delay_bytes;
    float* delay_line;

    long write_idx;
    long read_idx;

    short delay_connected;
    short feedback_connected;
} t_vdelay;

/* The arguments/inlets/outlets/vectors indexes
 * *******************************/
enum ARGUMENTS { A_MAX_DELAY, A_DELAY, A_FEEDBACK };
enum INLETS { I_INPUT, I_DELAY, I_FEEDBACK, NUM_INLETS };
enum OUTLETS { O_OUTPUT, NUM_OUTLETS };
enum DSP {
    PERFORM,
    OBJECT,
    INPUT1,
    DELAY,
    FEEDBACK,
    OUTPUT1,
    VECTOR_SIZE,
    NEXT
};

/* The class pointer
 * **********************************************************/
static t_class* vdelay_class;

/* Function prototypes
 * ********************************************************/
void* vdelay_common_new(t_vdelay* x, short argc, t_atom* argv);
void vdelay_free(t_vdelay* x);
void vdelay_dsp(t_vdelay* x, t_signal** sp, short* count);
t_int* vdelay_perform(t_int* w);

/******************************************************************************/


/* Function prototypes
 * ********************************************************/
void* vdelay_new(t_symbol* s, short argc, t_atom* argv);

/* The 'initialization' routine
 * ***********************************************/
#ifdef WIN32
__declspec(dllexport) void vdelay_tilde_setup(void);
#endif
void vdelay_tilde_setup(void)
{
    /* Initialize the class */
    vdelay_class = class_new(gensym("vdelay~"), (t_newmethod)vdelay_new,
                             (t_method)vdelay_free, sizeof(t_vdelay), 0,
                             A_GIMME, 0);

    /* Specify signal input, with automatic float to signal conversion */
    CLASS_MAINSIGNALIN(vdelay_class, t_vdelay, x_f);

    /* Bind the DSP method, which is called when the DACs are turned on */
    class_addmethod(vdelay_class, (t_method)vdelay_dsp, gensym("dsp"), 0);

    /* Print message to Max window */
    post("vdelay~ • External was loaded");
}

/* The 'new instance' routine
 * *************************************************/
void* vdelay_new(t_symbol* s, short argc, t_atom* argv)
{
    /* Instantiate a new object */
    t_vdelay* x = (t_vdelay*)pd_new(vdelay_class);
    return vdelay_common_new(x, argc, argv);
}

/* The common 'new instance' routine
 * ******************************************/
void* vdelay_common_new(t_vdelay* x, short argc, t_atom* argv)
{
    /* Create signal inlets */
    inlet_new(&x->obj, &x->obj.ob_pd, gensym("signal"), gensym("signal"));
    inlet_new(&x->obj, &x->obj.ob_pd, gensym("signal"), gensym("signal"));

    /* Create signal outlets */
    outlet_new(&x->obj, gensym("signal"));

    /* Initialize input arguments */
    float max_delay = DEFAULT_MAX_DELAY;
    float delay = DEFAULT_DELAY;
    float feedback = DEFAULT_FEEDBACK;

    /* Parse arguments passed from object */
    if (argc > A_FEEDBACK) {
        feedback = atom_getfloatarg(A_FEEDBACK, argc, argv);
    }
    if (argc > A_DELAY) {
        delay = atom_getfloatarg(A_DELAY, argc, argv);
    }
    if (argc > A_MAX_DELAY) {
        max_delay = atom_getfloatarg(A_MAX_DELAY, argc, argv);
    }

    /* Check validity of passed arguments */
    if (max_delay < MINIMUM_MAX_DELAY) {
        max_delay = MINIMUM_MAX_DELAY;
        post("vdelay~ • Invalid argument: Maximum delay time set to %.4f[ms]",
             max_delay);
    } else if (max_delay > MAXIMUM_MAX_DELAY) {
        max_delay = MAXIMUM_MAX_DELAY;
        post("vdelay~ • Invalid argument: Maximum delay time set to %.4f[ms]",
             max_delay);
    }

    if (delay < MINIMUM_DELAY) {
        delay = MINIMUM_DELAY;
        post("vdelay~ • Invalid argument: Delay time set to %.4f[ms]", delay);
    } else if (delay > MAXIMUM_DELAY) {
        delay = MAXIMUM_DELAY;
        post("vdelay~ • Invalid argument: Delay time set to %.4f[ms]", delay);
    }

    if (feedback < MINIMUM_FEEDBACK) {
        feedback = MINIMUM_FEEDBACK;
        post("vdelay~ • Invalid argument: Feedback factor set to %.4f",
             feedback);
    } else if (feedback > MAXIMUM_FEEDBACK) {
        feedback = MAXIMUM_FEEDBACK;
        post("vdelay~ • Invalid argument: Feedback factor set to %.4f",
             feedback);
    }

    /* Initialize state variables */
    x->max_delay = max_delay;
    x->delay = delay;
    x->feedback = feedback;

    x->fs = sys_getsr();

    x->delay_length = (x->max_delay * 1e-3 * x->fs) + 1;
    x->delay_bytes = x->delay_length * sizeof(float);

    x->delay_line = (float*)getbytes(x->delay_bytes);


    if (x->delay_line == NULL) {
        post("vdelay~ • Cannot allocate memory for this object");
        return NULL;
    }

    for (int ii = 0; ii < x->delay_length; ii++) {
        x->delay_line[ii] = 0.0;
    }

    x->write_idx = 0;
    x->read_idx = 0;

    /* Print message to Max window */
    post("vdelay~ • Object was created");

    /* Return a pointer to the new object */
    return x;
}

/* The 'free instance' routine
 * ************************************************/
void vdelay_free(t_vdelay* x)
{
    /* Free allocated dynamic memory */
    freebytes(x->delay_line, x->delay_bytes);

    /* Print message to Max window */
    post("vdelay~ • Memory was freed");
}

/* The 'DSP' method
 * ***********************************************************/

void vdelay_dsp(t_vdelay* x, t_signal** sp, short* count)
{
    /* Store signal connection states of inlets */
    x->delay_connected = 1;
    x->feedback_connected = 1;

    /* Adjust to changes in the sampling rate */
    if (x->fs != sp[0]->s_sr) {
        x->fs = sp[0]->s_sr;

        x->delay_length = (x->max_delay * 1e-3 * x->fs) + 1;
        long delay_bytes_old = x->delay_bytes;
        x->delay_bytes = x->delay_length * sizeof(float);
        x->delay_line = (float*)resizebytes((void*)x, delay_bytes_old,
                                            x->delay_bytes);


        if (x->delay_line == NULL) {
            post("vdelay~ • Cannot reallocate memory for this object");
            return;
        }

        for (int ii = 0; ii < x->delay_length; ii++) {
            x->delay_line[ii] = 0.0;
        }

        x->write_idx = 0;
        x->read_idx = 0;
    }

    /* Attach the object to the DSP chain */
    dsp_add(vdelay_perform, NEXT - 1, x, sp[0]->s_vec, sp[1]->s_vec,
            sp[2]->s_vec, sp[3]->s_vec, sp[0]->s_n);

    /* Print message to Max window */
    post("vdelay~ • Executing 32-bit perform routine");
}

/* The 'perform' routine
 * ******************************************************/
t_int* vdelay_perform(t_int* w)
{
    /* Copy the object pointer */
    t_vdelay* x = (t_vdelay*)w[OBJECT];

    /* Copy signal pointers */
    t_float* input = (t_float*)w[INPUT1];
    t_float* delay = (t_float*)w[DELAY];
    t_float* feedback = (t_float*)w[FEEDBACK];
    t_float* output = (t_float*)w[OUTPUT1];

    /* Copy the signal vector size */
    t_int n = w[VECTOR_SIZE];

    /* Load state variables */
    float delay_float = x->delay;
    float feedback_float = x->feedback;
    float fsms = x->fs * 1e-3;
    long delay_length = x->delay_length;
    float* delay_line = x->delay_line;
    long write_idx = x->write_idx;
    long read_idx = x->read_idx;
    short delay_connected = x->delay_connected;
    short feedback_connected = x->feedback_connected;

    /* Perform the DSP loop */
    long delay_time;
    float fb;

    long idelay;
    float fraction;
    float samp1;
    float samp2;

    float feed_sample;
    float out_sample;

    while (n--) {
        if (delay_connected) {
            delay_time = *delay++ * fsms;
        } else {
            delay_time = delay_float * fsms;
        }

        if (feedback_connected) {
            fb = *feedback++;
        } else {
            fb = feedback_float;
        }

        idelay = trunc(delay_time);
        fraction = delay_time - idelay;

        if (idelay < 0) {
            idelay = 0;
        } else if (idelay > delay_length) {
            idelay = delay_length - 1;
        }

        read_idx = write_idx - idelay;
        while (read_idx < 0) {
            read_idx += delay_length;
        }

        if (read_idx == write_idx) {
            out_sample = *input++;
        } else {
            samp1 = delay_line[(read_idx + 0)];
            samp2 = delay_line[(read_idx + 1) % delay_length];
            out_sample = samp1 + fraction * (samp2 - samp1);

            feed_sample = (*input++) + (out_sample * fb);
            if (fabs(feed_sample) < 1e-6) {
                feed_sample = 0.0;
            }
            delay_line[write_idx] = feed_sample;
        }

        *output++ = out_sample;

        write_idx++;
        if (write_idx >= delay_length) {
            write_idx -= delay_length;
        }
    }

    /* Update state variables */
    x->write_idx = write_idx;

    /* Return the next address in the DSP chain */
    return w + NEXT;
}
