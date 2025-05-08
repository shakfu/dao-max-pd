#include "ext.h"
#include "ext_obex.h"
#include "z_dsp.h"

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
    t_pxobject obj;

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

void vdelay_dsp64(t_vdelay* x, t_object* dsp64, short* count,
                  double samplerate, long maxvectorsize, long flags);
void vdelay_perform64(t_vdelay* x, t_object* dsp64, double** ins, long numins,
                      double** outs, long numouts, long sampleframes,
                      long flags, void* userparam);

/******************************************************************************/


/* Function prototypes
 * ********************************************************/
void* vdelay_new(t_symbol* s, short argc, t_atom* argv);

void vdelay_float(t_vdelay* x, double farg);
void vdelay_assist(t_vdelay* x, void* b, long msg, long arg, char* dst);

/* The 'initialization' routine
 * ***********************************************/
int C74_EXPORT main()
{
    /* Initialize the class */
    vdelay_class = class_new("vdelay~", (method)vdelay_new,
                             (method)vdelay_free, sizeof(t_vdelay), 0, A_GIMME,
                             0);

    /* Bind the DSP method, which is called when the DACs are turned on */
    class_addmethod(vdelay_class, (method)vdelay_dsp64, "dsp64", A_CANT, 0);

    /* Bind the float method, which is called when floats are sent to inlets */
    class_addmethod(vdelay_class, (method)vdelay_float, "float", A_FLOAT, 0);

    /* Bind the assist method, which is called on mouse-overs to inlets and
     * outlets */
    class_addmethod(vdelay_class, (method)vdelay_assist, "assist", A_CANT, 0);

    /* Add standard Max methods to the class */
    class_dspinit(vdelay_class);

    /* Register the class with Max */
    class_register(CLASS_BOX, vdelay_class);

    /* Print message to Max window */
    object_post(NULL, "vdelay~ • External was loaded");

    /* Return with no error */
    return 0;
}

/* The 'new instance' routine
 * *************************************************/
void* vdelay_new(t_symbol* s, short argc, t_atom* argv)
{
    /* Instantiate a new object */
    t_vdelay* x = (t_vdelay*)object_alloc(vdelay_class);
    return vdelay_common_new(x, argc, argv);
}

/* The 'float' method
 * *********************************************************/
void vdelay_float(t_vdelay* x, double farg)
{
    long inlet = ((t_pxobject*)x)->z_in;

    switch (inlet) {
    case I_DELAY:
        if (farg < MINIMUM_DELAY) {
            farg = MINIMUM_DELAY;
            object_warn((t_object*)x,
                        "Invalid argument: Delay time set to %.4f[ms]", farg);
        } else if (farg > MAXIMUM_DELAY) {
            farg = MAXIMUM_DELAY;
            object_warn((t_object*)x,
                        "Invalid argument: Delay time set to %.4f[ms]", farg);
        }
        x->delay = farg;
        break;

    case I_FEEDBACK:
        if (farg < MINIMUM_FEEDBACK) {
            farg = MINIMUM_FEEDBACK;
            object_warn((t_object*)x,
                        "Invalid argument: Feedback factor set to %.4f", farg);
        } else if (farg > MAXIMUM_FEEDBACK) {
            farg = MAXIMUM_FEEDBACK;
            object_warn((t_object*)x,
                        "Invalid argument: Feedback factor set to %.4f", farg);
        }
        x->feedback = farg;
        break;
    }

    /* Print message to Max window */
    object_post((t_object*)x, "Receiving floats");
}

/* The 'assist' method
 * ********************************************************/
void vdelay_assist(t_vdelay* x, void* b, long msg, long arg, char* dst)
{
    /* Document inlet functions */
    if (msg == ASSIST_INLET) {
        switch (arg) {
        case I_INPUT:
            snprintf_zero(dst, ASSIST_MAX_STRING_LEN, "(signal) Input");
            break;
        case I_DELAY:
            snprintf_zero(dst, ASSIST_MAX_STRING_LEN, "(signal/float) Delay");
            break;
        case I_FEEDBACK:
            snprintf_zero(dst, ASSIST_MAX_STRING_LEN,
                          "(signal/float) Feedback");
            break;
        }
    }

    /* Document outlet functions */
    else if (msg == ASSIST_OUTLET) {
        switch (arg) {
        case O_OUTPUT:
            snprintf_zero(dst, ASSIST_MAX_STRING_LEN, "(signal) Output");
            break;
        }
    }
}

/******************************************************************************/


/* The common 'new instance' routine
 * ******************************************/
void* vdelay_common_new(t_vdelay* x, short argc, t_atom* argv)
{
    /* Create signal inlets */
    dsp_setup((t_pxobject*)x, NUM_INLETS);

    /* Create signal outlets */
    outlet_new((t_object*)x, "signal");

    /* Avoid sharing memory among audio vectors */
    x->obj.z_misc |= Z_NO_INPLACE;

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

    x->delay_line = (float*)sysmem_newptr(x->delay_bytes);

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
    /* Remove the object from the DSP chain */
    dsp_free((t_pxobject*)x);

    /* Free allocated dynamic memory */
    sysmem_freeptr(x->delay_line);

    /* Print message to Max window */
    post("vdelay~ • Memory was freed");
}

/* The 'DSP' method
 * ***********************************************************/

void vdelay_dsp64(t_vdelay* x, t_object* dsp64, short* count,
                  double samplerate, long maxvectorsize, long flags)
{
    /* Store signal connection states of inlets */
    x->delay_connected = count[A_DELAY];
    x->feedback_connected = count[A_FEEDBACK];

    /* Adjust to changes in the sampling rate */
    if (x->fs != samplerate) {
        x->fs = samplerate;

        x->delay_length = (x->max_delay * 1e-3 * x->fs) + 1;
        x->delay_bytes = x->delay_length * sizeof(float);
        x->delay_line = (float*)sysmem_resizeptr((void*)x->delay_line,
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
    object_method(dsp64, gensym("dsp_add64"), x, vdelay_perform64, 0, NULL);

    /* Print message to Max window */
    post("vdelay~ • Executing 64-bit perform routine");
}

void vdelay_perform64(t_vdelay* x, t_object* dsp64, double** ins, long numins,
                      double** outs, long numouts, long sampleframes,
                      long flags, void* userparam)
{
    /* Copy signal pointers */
    t_double* input = ins[0];
    t_double* delay = ins[1];
    t_double* feedback = ins[2];
    t_double* output = outs[0];
    int n = sampleframes;

    /* Load state variables */
    double delay_double = x->delay;
    double feedback_double = x->feedback;
    float fsms = x->fs * 1e-3;
    long delay_length = x->delay_length;
    float* delay_line = x->delay_line;
    long write_idx = x->write_idx;
    long read_idx = x->read_idx;
    short delay_connected = x->delay_connected;
    short feedback_connected = x->feedback_connected;

    /* Perform the DSP loop */
    long delay_time;
    double fb;

    long idelay;
    double fraction;
    double samp1;
    double samp2;

    double feed_sample;
    double out_sample;

    while (n--) {
        if (delay_connected) {
            delay_time = *delay++ * fsms;
        } else {
            delay_time = delay_double * fsms;
        }

        if (feedback_connected) {
            fb = *feedback++;
        } else {
            fb = feedback_double;
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
}

/******************************************************************************/
